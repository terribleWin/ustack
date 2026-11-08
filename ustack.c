/*
 * ustack - A DPDK-based user-space TCP/IP stack, written for educational purposes.
 * 
 * This file only demonstrates HOW to build a user-space protocol stack on DPDK:
 * it is intentionally kept small and readable, not production-ready.
 * 
 * What it currently shows:
 *   - A minimal single-threaded DPDK polling loop (rx burst -> parse -> tx burst).
 *   - Global variables and a single-connection assumption, to keep the demo simple.
 *   - ARP: answering requests for the local IP, sending periodic broadcast
 *     requests over the whole subnet, and filling an ARP table from replies.
 *   - ICMP: echo reply, so the stack can be pinged.
 *   - IPv4/UDP/TCP: header parsing, printing, echoing the payload back,
 *     and a SYN -> SYN/ACK -> ACK handshake demo.
 * 
 * Known limitations (intentional, for simplicity):
 *   - The main loop, the protocol processing and the application tasks entry are deoupled into
 *     in three different while(1) block.Each block will run in a thread.
 *   - No timers other than the ARP scan, no retransmission, no window control.
 * 
 * Roadmap (next stages):
 *   1. Decouple the main loop from the protocol/business logic
 *      (rx/tx cores + ring buffers, like the tcp.c sample in this repo).
 *   2. Per-connection state instead of global variables, multiple connections.
 *   3. Multi-threading and a proper timer framework.
 * 
 * Build: see the Makefile (make) and run with the DPDK EAL arguments.
 */

#include <stdio.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <rte_timer.h>

#include <arpa/inet.h>


#include "arp.h"
#include "fd.h"
#include "posix_api.h"
// #include "hash.h"

struct rte_mbuf* ustack_send_udp(struct rte_mempool *mbuf_pool, unsigned char *data, uint16_t length);
int udp_out(struct rte_mempool *mbuf_pool);
int tcp_out (struct rte_mempool* mbuf_pool);

// TODO: 后续引入多网卡时，需要将 gDpdkPortId 替换为动态参数。
// 1. 收包时使用 mbuf->port 获取来源。
// 2. UCB/TCB 中需保存网卡 port_id。
// 3. 发包函数需显式传入 port_id。
int gDpdkPortId = 0;

int global_portid = 0;   /* 使用的端口号（对应第一个可用的 ethdev） */

#define NUM_MBUFS (4096 - 1)   /* mbuf 内存池容量，决定可同时在途的收包缓冲区数量 */
#define BURST_SIZE 128   /* 单次收包批量上限（rte_eth_rx_burst 一次最多收取的报文数） */
#define RING_SIZE 1024

#define ENABLE_SEND 1
#define ENABLE_TCP 1

#define TCP_INIT_WINDOWS 14600
#define TIMER_RESOLUTION_CYCLES 20000000ULL

#define TCP_OPTION_LENGTH 8

#define TCP_MAX_SEQ 0xFFFFFFFF

//TODO:后续采用 RFC 6928 规定的 min(10*MSS, max(2*MSS, 14600))来动态计算窗口
#define TCP_INITIAL_WINDOW  14600 // RFC 6928
static uint8_t gDefaultArpMac[RTE_ETHER_ADDR_LEN] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

#if ENABLE_SEND

#define MAKE_IPV4_ADDR(a, b, c, d) (a + (b<<8) + (c<<16) + (d<<24))

static uint32_t gLocalIp = MAKE_IPV4_ADDR(192, 168, 139, 137); // 192168139137

//TODO:对于全局唯一的源 MAC、IP 地址需要引入 DPDK 动态查询网卡 MAC、IP 地址
// 后续可以考虑引入多张网卡来实现数据收发
uint8_t global_smac[RTE_ETHER_ADDR_LEN]; // global default source mac address
uint8_t global_dmac[RTE_ETHER_ADDR_LEN];

static uint32_t global_sip;
static uint32_t global_dip;

uint16_t global_sport;
uint16_t global_dport;

#endif

#if ENABLE_TCP

// This part is about the ringbuffer which transport data between PMD and APP thread
// - PMD thread: process the received data from NICs, packet out the data
// - APP thread: process the application data
struct inout_ring {

    struct rte_ring *in;
    struct rte_ring *out;
};

//  Global Singleton inout_ring object
static struct inout_ring *rInst = NULL;

// Get the global singleton inout_ring instance
static struct inout_ring *ringInstance(void) {

    if (rInst == NULL) {
        rInst = rte_malloc("in/out ring", sizeof(struct inout_ring), 0);
        memset(rInst , 0, sizeof(struct inout_ring));
    }

    return rInst;

}

// 根据 RFC 793 建立 TCP 三次握手状态机
typedef enum __USTACK_TCP_STATUS {

	USTACK_TCP_STATUS_CLOSED = 0,
	USTACK_TCP_STATUS_LISTEN,
	USTACK_TCP_STATUS_SYN_RCVD,
	USTACK_TCP_STATUS_SYN_SENT,
	USTACK_TCP_STATUS_ESTABLISHED,
	USTACK_TCP_STATUS_FIN_WAIT_1,
	USTACK_TCP_STATUS_FIN_WAIT_2,
	USTACK_TCP_STATUS_CLOSING,
	USTACK_TCP_STATUS_TIMEWAIT,
	USTACK_TCP_STATUS_CLOSE_WAIT,
	USTACK_TCP_STATUS_LAST_ACK
	
} USTACK_TCP_STATUS;


/**
 * @brief TCP connection control block (TCB).
 *
 * @details
 * One instance is allocated per TCP connection and linked into the global
 * connection table, so that an incoming segment can be dispatched to the
 * connection that owns it by looking up the 5-tuple (sip, sport, dip, dport,
 * protocol). It plays the same role as `struct sock` / `struct tcp_sock` in
 * the Linux kernel, or `struct ng_tcp_stream` in the reference implementation.
 *
 * The two rings decouple the application lcore from the protocol lcore:
 *   - `sndbuf` carries the descriptors produced by the application (or by the
 *     state machine itself, e.g. SYN/ACK, ACK, FIN) towards the output path,
 *     where they are turned into real TCP segments.
 *   - `rcvbuf` carries the payload extracted from the received segments
 *     towards the application.
 *
 * @note Addresses and ports are stored in network byte order, while
 *       `snd_nxt` / `recv_nxt` are kept in host byte order and are only
 *       converted with htonl() when a segment is built.
 */
struct ustack_tcp_stream {
    //TODO: 做拥塞控制流量控制字段、可靠传输、小包确认与恢复字段
    int fd;                                 /**< socket descriptor exposed to the application, -1 until the connection is accepted */

    uint32_t dip;                           /**< remote (peer) IPv4 address, network byte order */
    uint8_t localmac[RTE_ETHER_ADDR_LEN];   /**< local MAC address, used as the Ethernet source of every outgoing segment */
    uint16_t dport;                        /**< remote (peer) TCP port, network byte order */

    uint8_t protocol;                       /**< transport protocol, always IPPROTO_TCP in this implementation */

    uint16_t sport;                         /**< local TCP port, network byte order */
    uint32_t sip;                           /**< local IPv4 address, network byte order */

    uint32_t snd_nxt;                       /**< next sequence number to send (RFC 793 SND.NXT), host byte order */
    uint32_t rcv_nxt;                      /**< next sequence number expected from the peer (RFC 793 RCV.NXT), host byte order */

    struct rte_ring *sndbuf;                /**< outgoing queue holding the descriptors waiting to be serialised and transmitted */
    struct rte_ring *rcvbuf;                /**< incoming queue holding the payload waiting to be read by the application */

    pthread_cond_t cond;                    /**< wakes a blocked application thread when data or a new connection arrives */
    pthread_mutex_t mutex;                  /**< paired with @ref cond to guard the wait/signal sequence */

    USTACK_TCP_STATUS status;

    struct ustack_tcp_stream *prev;
    struct ustack_tcp_stream *next;
};

//TODO: 现在由链表管理全局的 udp_sock 后续可以引入 rb_tree 使用全局的 rbtree来管理全部的tcp/udp连接控制块
struct ustack_tcp_table {

    int count;
    struct ustack_tcp_stream *tcb_set; //

};

/**
 * @brief TCP segment descriptor: the bridge between the control plane and the data plane.
 *
 * @details
 * This structure is NOT a TCP segment and must not be confused with
 * `struct rte_tcp_hdr`, which is the on-wire header layout. It is a purely local
 * descriptor that carries "what should be sent" (or "what has been received")
 * between the control plane and the data plane, so that the application side and
 * the packet-serialising side never have to share global state.
 *
 * A descriptor travels in two directions through the per-connection rings:
 *   - TX path: the application (or the state machine itself, for SYN/ACK, ACK and
 *     FIN) allocates a descriptor, fills it in and enqueues it into
 *     `ustack_tcp_stream::sndbuf`. tcp_out() dequeues it and hands it to
 *     ustack_encode_tcp_pkt(), which serialises it into a real segment and
 *     enqueues the resulting mbuf into `inout_ring::out`.
 *   - RX path: the protocol lcore strips the payload out of a received segment,
 *     stores it in a descriptor and enqueues it into `ustack_tcp_stream::rcvbuf`;
 *     the application thread blocked in recv() is woken up through the
 *     connection's condition variable and copies the bytes out.
 *
 * The fields fall into three groups, and the difference matters:
 *   1. on-wire header fields copied verbatim into `struct rte_tcp_hdr`:
 *      sport, dport, seqnum, acknum, hdrlen_off, tcp_flags, win_len, cksum, tcp_urp;
 *   2. on-wire option bytes: options[] holds the option area as raw bytes and is
 *      copied behind the fixed 20-byte header only when optlen > 0;
 *   3. local-only control fields: optlen, data and length. None of them exists on
 *      the wire - the peer recovers the option length from the data offset
 *      instead, as ((data_off >> 4) - 5) * 4.
 *
 * @note
 *  - All header fields are kept in HOST byte order here. The conversion with
 *    htonl()/htons() happens only while a segment is being built.
 *  - hdrlen_off stores the raw value of the TCP data offset field, not a byte
 *    count: 0x50 means 5 32-bit words, i.e. a 20-byte header without options.
 *  - optlen and hdrlen_off describe the same thing and must stay consistent,
 *    that is hdrlen_off = (5 + optlen) << 4. If they disagree the peer will
 *    interpret the option area as payload and every byte after it is shifted.
 *  - Ownership of data follows the descriptor: once the descriptor is enqueued,
 *    the dequeuing side must free both the payload buffer and the descriptor
 *    itself, otherwise memory leaks.
 */
struct tcp_fragment {

    uint16_t sport;  /**< TCP source port. */
	uint16_t dport;  /**< TCP destination port. */
	uint32_t seqnum;  /**< TX data sequence number. */
	uint32_t acknum;  /**< RX data acknowledgement sequence number. */
	uint8_t  hdrlen_off;  /**< Data offset. */
	uint8_t  tcp_flags; /**< TCP flags */
	uint16_t win_len;    /**< RX flow control window. */
	uint16_t cksum;     /**< TCP checksum. */
	uint16_t tcp_urp;   /**< TCP urgent pointer, if any. */

    int optlen; /**<The length of options area per 4 bytes> */
    uint32_t options[TCP_OPTION_LENGTH];
    unsigned char* data;
    int length;
};



struct ustack_tcp_table *tInst = NULL;

static struct ustack_tcp_table *tcpInstance(void) {

    if (tInst == NULL) {
        tInst = rte_malloc("ustack_tcp_table", sizeof(struct ustack_tcp_table), 0);
        memset(tInst, 0, sizeof(struct ustack_tcp_table));
    }
    
    return tInst;
}

uint8_t global_flags;
uint32_t global_seqnum;
uint32_t global_acknum;

#endif

/* 端口默认配置：普通以太网帧，最大接收长度 1518 字节 */
static const struct rte_eth_conf port_conf_default = {
    .rxmode = { .max_rx_pkt_len = RTE_ETHER_MAX_LEN }
};

/* 初始化 */
static int ustack_init_port(struct rte_mempool *mbuf_pool) {
    // 统计当前可用的以太网端口数（仅打印信息，本程序只使用 global_portid）
    uint16_t nb_sys_ports = rte_eth_dev_count_avail();
    if (nb_sys_ports == 0) {
        rte_exit(EXIT_FAILURE, "No supported eth found\n");
    }

    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(global_portid, &dev_info);

    printf("NIC Name: %s\n", dev_info.driver_name);
    printf("TX Desc Min: %u, Max: %u, Align: %u\n", 
       dev_info.tx_desc_lim.nb_min, 
       dev_info.tx_desc_lim.nb_max, 
       dev_info.tx_desc_lim.nb_align);


    // 配置端口的队列数量：1 个 RX 队列 + 1 个 TX 队列
    const int num_rx_queues = 1;
#if ENABLE_SEND
    const int num_tx_queues = 1;
#else
    const int num_tx_queues = 0;
#endif

    int ret = rte_eth_dev_configure(global_portid, num_rx_queues, num_tx_queues, &port_conf_default);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Could not configure port %d (ret=%d)\n", global_portid, ret);
    }

    // 初始化 RX 队列：128 个描述符，收包 mbuf 由 mbuf_pool 分配
    if (rte_eth_rx_queue_setup(global_portid, 0, 128, rte_eth_dev_socket_id(global_portid), NULL, mbuf_pool) < 0) {
        rte_exit(EXIT_FAILURE, "Could not set up RX queues\n");
    }

#if ENABLE_SEND

    struct rte_eth_txconf txq_conf = dev_info.default_txconf;
    txq_conf.offloads = port_conf_default.rxmode.offloads;

    // 不同 PMD 对描述符数量有最小/对齐要求（e1000 的 TX 最少 512），
    // 用 rte_eth_dev_adjust_nb_rx_tx_desc() 按 dev_info.tx_desc_lim 自动校正
    uint16_t nb_tx_desc = 512;
    rte_eth_dev_adjust_nb_rx_tx_desc(global_portid, NULL, &nb_tx_desc);

    // 初始化 TX 队列：NULL 表示使用驱动默认配置
    if (rte_eth_tx_queue_setup(global_portid, 0, nb_tx_desc, rte_eth_dev_socket_id(global_portid), &txq_conf) < 0) {
        rte_exit(EXIT_FAILURE, "Could not set up TX queues\n");
    }

#endif

    // 启动端口，之后才能收发报文
    if (rte_eth_dev_start(global_portid) < 0) {
        rte_exit(EXIT_FAILURE, "Could not start\n");
    }

    /* 关键：取出网卡真实硬件 MAC，作为本协议栈的源 MAC。
       ARP 应答的以太网源地址、arp_sha，以及 TCP/UDP 发包的源地址都依赖 global_smac。
       如果不在这里初始化，首次 ARP 应答会带 00:00:00:00:00:00，
       对端把该 MAC 写入 ARP 缓存后，回包全部丢失。 */
    struct rte_ether_addr mac;
    rte_eth_macaddr_get(global_portid, &mac);
    rte_memcpy(global_smac, mac.addr_bytes, RTE_ETHER_ADDR_LEN);

    /* 目的 MAC 未知时先用广播，避免发出目的地址全 0 的帧 */
    memset(global_dmac, 0xff, RTE_ETHER_ADDR_LEN);

    /* 本机 IP，网络字节序（内存字节序与报文一致，可直接和 arp_tip 比较） */
    global_sip = gLocalIp;

    printf("local mac: %02x:%02x:%02x:%02x:%02x:%02x, local ip: %u.%u.%u.%u\n",
           global_smac[0], global_smac[1], global_smac[2],
           global_smac[3], global_smac[4], global_smac[5],
           (uint8_t)(gLocalIp),     (uint8_t)(gLocalIp >> 8),
           (uint8_t)(gLocalIp >> 16), (uint8_t)(gLocalIp >> 24));

    return 0;
}

// Fill the UDP fields.
static int ustack_encode_udp_pkt (uint8_t *msg, uint32_t sip, uint32_t dip,
    uint16_t sport, uint16_t dport, uint8_t *srcmac, uint8_t *dstmac,
    unsigned char* data, uint16_t total_len) {

    // ether header
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    rte_memcpy(eth->d_addr.addr_bytes, dstmac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(eth->s_addr.addr_bytes, srcmac, RTE_ETHER_ADDR_LEN);
    eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);

    // ip header
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr*)(eth + 1); //msg + 
    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = htons(total_len - sizeof(struct rte_ether_hdr));
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64;
    ip->next_proto_id = IPPROTO_UDP;
    ip->src_addr = sip;
    ip->dst_addr = dip;

    // The checksum field must be set to 0 by the caller.
    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);
    
    // udp header
    struct rte_udp_hdr *udp = (struct rte_udp_hdr*)(ip + 1);
    udp->src_port = sport;
    udp->dst_port = dport;

    uint16_t payload_len = total_len - sizeof(struct rte_ether_hdr) 
                                     - sizeof(struct rte_ipv4_hdr) 
                                     - sizeof(struct rte_udp_hdr);
    udp->dgram_len = htons(payload_len + sizeof(struct rte_udp_hdr));

    rte_memcpy((uint8_t *)(udp + 1), data, payload_len);
    udp->dgram_cksum = 0;
    udp->dgram_cksum = rte_ipv4_udptcp_cksum(ip, udp);

    return 0;
}

// Get a mbuf from the memory pool, and fill the UDP fields.return the mbuf.
static struct rte_mbuf *ustack_udp_pkt (struct rte_mempool *mbuf_pool, uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport, 
            const uint8_t *srcmac, uint8_t *dstmac, uint8_t *data, uint16_t length) {

    const uint16_t total_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + 
                                sizeof(struct rte_udp_hdr) + length;
    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
    if (mbuf == NULL) {
        return NULL;
    }

    mbuf->pkt_len = total_len;
    mbuf->data_len = total_len;
    
    uint8_t *pkt = rte_pktmbuf_mtod(mbuf, uint8_t *);
    ustack_encode_udp_pkt(pkt, sip, dip, sport, dport, srcmac, dstmac, data, total_len);

    return mbuf;
}

// construct TCP msg
static int ustack_encode_tcp_pkt(uint8_t *msg, uint32_t sip, uint32_t dip, 
    uint8_t *srcmac, uint8_t * dstmac, struct tcp_fragment *fragment) {
    
    const unsigned total_len = fragment->length + sizeof(struct rte_ether_hdr) +
        sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_tcp_hdr) + fragment->optlen * sizeof(uint32_t);

    // ether header
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    rte_memcpy(eth->d_addr.addr_bytes, dstmac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(eth->s_addr.addr_bytes, srcmac, RTE_ETHER_ADDR_LEN);
    eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);
    
    // ip header
    // TODO[FIXME]:如果以太网头携带 Vlan 标志的话就需要动态计算 ip 头部指针偏移
    uint8_t hdr_words = fragment->hdrlen_off >> 4;
    uint16_t opt_bytes = (hdr_words > 5) ? (hdr_words - 5) : 0;

    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr*)(eth + 1); //msg + 
    ip->version_ihl = 0x45;  // RTE_IPV4_VHL_DEF
    ip->type_of_service = 0;
    ip->total_length = htons(total_len - sizeof(struct rte_ether_hdr));
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64; // TTL = 64
    ip->next_proto_id = IPPROTO_TCP;
    ip->src_addr = sip;
    ip->dst_addr = dip; // global variable tcp_sock fileds:from dingle connection to multi-connections
    
    // The checksum field must be set to 0 by the caller before caculate the checksum.
    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    // tcp header
    uint8_t iphdr_len = (ip->version_ihl & 0x0f) * 4;
    struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr*)((uint8_t*)ip + iphdr_len);
    tcp->src_port = fragment->sport;
    tcp->dst_port = fragment->dport;
    tcp->sent_seq = htonl(fragment->seqnum);
    tcp->recv_ack = htonl(fragment->acknum);
    
    // hdrlen_off 高四位为 header_length 字段，低四位为 reserved bits(must be 0)
    // 曾经中三位保留 最后一位为 NS（也可以被当做保留位），后来由 RFC 8311 废除
    tcp->data_off = fragment->hdrlen_off;
    tcp->tcp_flags = fragment->tcp_flags;
    tcp->rx_win = htons(fragment->win_len);
    tcp->tcp_urp = fragment->tcp_urp;
    
    // TCP options
    uint8_t* payload = (uint8_t*)(tcp + 1);
    if (opt_bytes > 0) {
        rte_memcpy(payload, fragment->options, opt_bytes);
    }

    payload += opt_bytes;
    if (fragment->data != NULL && fragment->length > 0) {
        rte_memcpy(payload, fragment->data, fragment->length);
    }


	tcp->cksum = 0;
	tcp->cksum = rte_ipv4_udptcp_cksum(ip, tcp);

    return 0;
}

static struct rte_mbuf *ustack_tcp_pkt(struct rte_mempool* mbuf_pool,
    uint32_t sip, uint32_t dip, uint8_t *srcmac, uint8_t *dstmac, struct tcp_fragment *fragment) {

        uint8_t hdr_words = fragment->hdrlen_off >> 4;
        uint8_t opt_bytes = (hdr_words > 5) ? (hdr_words - 5) : 0;
        const unsigned total_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr)
            + sizeof(struct rte_tcp_hdr) + opt_bytes + fragment->length;

        struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
        if (mbuf == NULL) return NULL;

        mbuf->pkt_len = total_len;
        mbuf->data_len = total_len;

        // pointer to the type of uint8_t*.
        uint8_t *pkt = rte_pktmbuf_mtod(mbuf, uint8_t *);
        ustack_encode_tcp_pkt(pkt, sip, dip, srcmac, dstmac, fragment);

        return mbuf;
}   


/** 
 * @brief This function is used to construct an ARP packet.
 * 
 * @details This function fills the fields of an ARP packet (including the Ethernet 
 * header and the ARP payload) for both ARP Request and ARP Reply. 
 * 
 * ARP has specific characteristics based on its opcode:
 * - For ARP Request (RTE_ARP_OP_REQUEST): The Ethernet destination MAC is set to 
 *   the broadcast address (FF:FF:FF:FF:FF:FF), and the ARP target hardware address 
 *   (ar$tha) is set to all zeros, indicating an unknown MAC.
 * - For ARP Reply (RTE_ARP_OP_REPLY): Both the Ethernet destination MAC and the ARP 
 *   target hardware address (ar$tha) are set to the actual unicast MAC of the peer.
 *
 * @param msg Pointer to the contiguous memory region of the mbuf (Ethernet frame start).
 * @param opcode The ARP operation code (RTE_ARP_OP_REQUEST or RTE_ARP_OP_REPLY).
 * @param dst_mac Pointer to the destination MAC address (used for unicast Reply).
 * @param sip Source IP address (network byte order).
 * @param dip Destination IP address (network byte order).
 * @return 0 on success.
 **/
static int ustack_encode_arp_pkt(uint8_t *msg, uint16_t opcode, const uint8_t *dst_mac, uint32_t sip, uint32_t dip) {

    // ether header
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    // ethernet frame package
    rte_memcpy(eth->s_addr.addr_bytes, global_smac, RTE_ETHER_ADDR_LEN);

    // fill up destinate mac address in the ethernet frame
    if (opcode == RTE_ARP_OP_REQUEST) {
        // ethernet destination: broadcast
        uint8_t broadcast_mac[RTE_ETHER_ADDR_LEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        rte_memcpy(eth->d_addr.addr_bytes, broadcast_mac, RTE_ETHER_ADDR_LEN);
    } else {
        rte_memcpy(eth->d_addr.addr_bytes, dst_mac, RTE_ETHER_ADDR_LEN);
    }

    eth->ether_type = htons(RTE_ETHER_TYPE_ARP);

    // Fill up the arp header field
    struct rte_arp_hdr *arp = (struct rte_arp_hdr *)(eth + 1);
    arp->arp_hardware = htons(RTE_ARP_HRD_ETHER);
    arp->arp_protocol = htons(RTE_ETHER_TYPE_IPV4);
    arp->arp_hlen = RTE_ETHER_ADDR_LEN; // 
    arp->arp_plen = sizeof(uint32_t);
    arp->arp_opcode = htons(opcode); // reply ---> opcode
    
    // aha 源 MAC IP 地址填充
    rte_memcpy(arp->arp_data.arp_sha.addr_bytes, global_smac, RTE_ETHER_ADDR_LEN);
    arp->arp_data.arp_sip = sip;

    if (opcode == RTE_ARP_OP_REQUEST) {
        // tha 目标 MAC 地址
        uint8_t zero_mac[RTE_ETHER_ADDR_LEN] = {0};
        rte_memcpy(arp->arp_data.arp_tha.addr_bytes, zero_mac, RTE_ETHER_ADDR_LEN); 
    } else {
        // arp reply: fill up the recieved destination mac address
        rte_memcpy(arp->arp_data.arp_tha.addr_bytes, dst_mac, RTE_ETHER_ADDR_LEN);
    }

    // destination ip
    arp->arp_data.arp_tip = dip;

    return 0;
}

/// @brief construct an ICMP echo reply packet according to RFC 792
/// @param msg the buffer used to store the constructed packet, containing the ethernet header, the ipv4 header and the icmp header
/// @param dst_mac the destination mac address, that is the mac address of the ping requester
/// @param sip the source ip address (the local ip) in network byte order
/// @param dip the destination ip address (the requester ip) in network byte order
/// @param icmp_ident the ICMP identifier, copied from the echo request as it is, no byte order conversion
/// @param icmp_seq the ICMP sequence number, copied from the echo request as it is, no byte order conversion
/// @return 0 on success, the constructed packet is filled into msg
/// @note only the 8 bytes ICMP header is built here, the icmp data (payload) of the echo request is not copied back
static int ustack_encode_icmp_pkt(uint8_t *msg, uint8_t *dst_mac, uint32_t sip, uint32_t dip, uint16_t icmp_ident, uint16_t icmp_seq) {

    // ether header: source mac is the local mac, destination mac is the mac of the ping requester
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    rte_memcpy(eth->s_addr.addr_bytes, global_smac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(eth->d_addr.addr_bytes, dst_mac, RTE_ETHER_ADDR_LEN);
    eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);

    // ip header: no ip options here, so version_ihl is 0x45 (ihl * 4 = 20 bytes),
    // total_length = ipv4 header + icmp header, because the icmp data is not carried in this demo
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr*)(eth + 1); //msg + 
    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = htons(sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_icmp_hdr));
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64;
    ip->next_proto_id = IPPROTO_ICMP;
    ip->src_addr = sip;
    ip->dst_addr = dip;

    // The checksum field must be set to 0 by the caller.
    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    // icmp header: type 0 and code 0 mean echo reply,
    // the identifier and the sequence number must be kept the same as the echo request,
    // otherwise the sender (ping) can not match the reply with its request
    struct rte_icmp_hdr *icmp = (struct rte_icmp_hdr*)(ip + 1);
    icmp->icmp_type = RTE_IP_ICMP_ECHO_REPLY; // reply to the request
    icmp->icmp_code = 0;
    icmp->icmp_ident = icmp_ident;
    icmp->icmp_seq_nb = icmp_seq;
    
    // Caculate the ICMP checksum field.
    // Cautions: the icmp checksum is calculated over the entire ICMP message, including the ICMP header and data as TCP\IP checksum.
    // But there is no need to construct a pseundo header for ICMP checksum calculation.
    // The checksum field must be set to 0 before calculating the checksum. 
    // The checksum is then computed using the one's complement of the one's complement sum of all 16-bit words in the ICMP message. 
    // If the length of the ICMP message is odd, a padding byte of 0 is added to make it even.
    // There is no provided API for caculating the ICMP checksum in DPDK, so we need to implement it manually by given API.
    // For more about checksum:RFC 1071 - Computing the Internet Checksum
    uint8_t ip_hlen = (ip->version_ihl & 0x0F) * 4;
    uint16_t icmp_len = ntohs(ip->total_length) - ip_hlen; // + data_len
    icmp->icmp_cksum = 0;
    uint16_t sum = (uint16_t)~rte_raw_cksum(icmp, icmp_len);   // cover ICMP head + data
    icmp->icmp_cksum = sum ? sum : 0xffff;

    return 0;
}

#if 0
/// @brief this function is used to send UDP packet
/// @param mbuf_pool the mbuf memory pool used to allocate mbuf
/// @param data UDP send data
/// @param length the lenght of UDP payload
/// @return the construction of UDP packet, return the mbuf pointer 
struct rte_mbuf* ustack_send_udp(struct rte_mempool *mbuf_pool, unsigned char *data, uint16_t length) {

    // mempool ---> mbuf
    const unsigned total_len = length + 42;
    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
    if (!mbuf) {
        rte_exit(EXIT_FAILURE, "Error rte_pktmbuf_alloc\n");
    }

    mbuf->pkt_len = total_len;
    mbuf->data_len = total_len;

    uint8_t *msg = rte_pktmbuf_mtod(mbuf, uint8_t *);
    ustack_encode_udp_pkt(msg, data, total_len);
    return mbuf;
}

#endif
/* sip / dip 均为网络字节序；源 MAC 固定用 global_smac，不需要再传参 */
static struct rte_mbuf* ustack_send_arp(struct rte_mempool *mbuf_pool, uint16_t opcode, const uint8_t *dstmac, uint32_t sip, uint32_t dip) {

    /* ARP 帧只有 14 + 28 = 42 字节，短于以太网最小帧长
       （60 字节，不含 CRC）。部分 PMD 不会自动填充，短帧可能被丢弃。 */
    uint16_t total_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_arp_hdr);
    const uint16_t min_len = RTE_ETHER_MIN_LEN - RTE_ETHER_CRC_LEN;
    if (total_len < min_len)
        total_len = min_len;

    struct rte_mbuf* mbuf = rte_pktmbuf_alloc(mbuf_pool);
    if (!mbuf) {
        rte_exit(EXIT_FAILURE, "Error rte_pktmbuf_alloc\n");
    }

    mbuf->pkt_len = total_len;
    mbuf->data_len = total_len;

    uint8_t *pkt_data = rte_pktmbuf_mtod(mbuf, uint8_t *);
    memset(pkt_data, 0, total_len);          /* 清空 padding 区，避免发出脏数据 */
    ustack_encode_arp_pkt(pkt_data, opcode, dstmac, sip, dip);

    return mbuf;
}

/// @brief this function is used to send ICMP packet
/// @param mbufpool the mbuf memory pool used to allocate mbuf
/// @param dst_mac the destination mac address  
/// @param sip the source ip address
/// @param dip the destination ip address
/// @param icmp_ident the ICMP identifier
/// @param icmp_seq the ICMP sequence number
/// @return the construction of ICMP packet, return the mbuf pointer
static struct rte_mbuf* ustack_send_icmp(struct rte_mempool* mbufpool, uint8_t *dst_mac, uint32_t sip, uint32_t dip, uint16_t icmp_ident, uint16_t icmp_seq) {
    
    const unsigned total_len = sizeof(struct rte_ether_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_icmp_hdr);
    struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbufpool);
    if (!mbuf) {
        rte_exit(EXIT_FAILURE, "Error rte_pktmbuf_alloc\n");
    }

    mbuf->pkt_len = total_len;
    mbuf->data_len = total_len;

    uint8_t *msg = rte_pktmbuf_mtod(mbuf, uint8_t *);
    ustack_encode_icmp_pkt(msg, dst_mac, sip, dip, icmp_ident, icmp_seq);
    
    return mbuf;
}

/**
 * @brief Callback function for periodic ARP request scanning.
 * 
 * @details This timer callback periodically broadcasts ARP Requests across 
 * the entire local subnet (e.g., 192.168.139.1 to 192.168.139.254). 
 * 
 * The logic extracts the local network prefix (the first three octets) from 
 * `gLocalIp` using a bitwise AND operation with 0x00FFFFFF. It then iterates 
 * through all possible host IDs (1 to 254) and merges them back using a bitwise 
 * OR operation. 
 * 
 * The purpose of retaining the network prefix is to ensure the broadcast stays 
 * strictly within the local Layer-2 broadcast domain. ARP operates only within 
 * the same subnet; retaining the prefix guarantees that the L2 switches and 
 * hosts correctly receive and process the broadcast. If a different prefix were 
 * used, the packets would be dropped or ignored. This mechanism actively warms 
 * up and refreshes the local ARP cache table.
 *
 * @param[in] tim Pointer to the DPDK timer structure (unused).
 * @param[in] arg Pointer to the memory pool used for allocating Mbufs.
 */
static void arp_request_timer_cb(__attribute__((unused)) struct rte_timer *tim,
	  __attribute__((unused)) void *arg){
        
        struct rte_mempool *mbuf_pool = (struct rte_mempool*) arg;
        struct inout_ring *ring =  ringInstance();

        int i = 0;
        for (i = 1; i <= 254 ; i++) {
        
        uint32_t dstip = (gLocalIp & 0x00FFFFFF) | (0xFF000000 & (i << 24));
        uint8_t* dstmac = ng_get_dst_macaddr(dstip);
        
        struct rte_mbuf *arp_mbuf = NULL;

        if (dstmac == NULL) {

            // send broadcast mac frame
            arp_mbuf = ustack_send_arp(mbuf_pool, RTE_ARP_OP_REQUEST, gDefaultArpMac, gLocalIp, dstip);

        } else {

            arp_mbuf = ustack_send_arp(mbuf_pool, RTE_ARP_OP_REQUEST, dstmac, gLocalIp, dstip);

        }
        
        //if (rte_eth_tx_burst(gDpdkPortId, 0, &arp_mbuf, 1)!=1) 
        //    rte_pktmbuf_free(arp_mbuf);
        
       rte_ring_mp_enqueue_burst(ring->out ,(void**)&arp_mbuf, 1, NULL);

    }

}

/// @brief This is a UDP connection control block.
/// Data after processing in the TCP/IP stack would be store at sndbuf so that 
/// the application thread could recieve these data and process them
/// When application layer wants to send data, they coule store the data into the rcvbuf.
/// Every real connect will allocate a udp_sock object.
struct ustack_udp_sock { // tcp.c : localhost Linux : struct udp_sock (UDP tcb)

    // TODO: sockfd 做非阻塞、阻塞
    int fd;

    int bflags;
    uint32_t localip; // ip --> mac
    uint8_t localmac[RTE_ETHER_ADDR_LEN];
    uint16_t localport;

    uint8_t protocol;
    
    struct rte_ring *sndbuf;
    struct rte_ring *rcvbuf;

    struct ustack_udp_sock *prev;
    struct ustack_udp_sock *next;

    pthread_cond_t cond;
    pthread_mutex_t mutex;
};

// TODO: 需要像 TCP 协议一样建立一个全局的 table 来管理头指针以及 UDP 连接个数 
// Global singleton list head  
static struct ustack_udp_sock *lhost = NULL;

// UDP offload
struct ustack_offload {

    uint32_t sip;
    uint32_t dip;
    uint16_t sport, dport;

    int protocol;
    
    unsigned char* data;
    uint16_t length; // total length

};

/**
 * @brief Process the UDP datagram in the pkt_process main loop.
 * 
 * @param[in] udpmbuf Pointer to the UDP datagram.
 * @return 
 *  -  0: Success; offload enqueue._PROCESS
 *  - -1: udpmbuf would be free.
 *  - -2: datat could not allocate.
 */
static int udp_process(struct rte_mbuf *udpmbuf) {

                struct rte_ipv4_hdr *iphdr =  rte_pktmbuf_mtod_offset(udpmbuf, struct rte_ipv4_hdr *, 
				                                sizeof(struct rte_ether_hdr));
                
                uint16_t ip_hdr_len = (iphdr->version_ihl & 0x0f) * 4;

                 // Dynamic caculate ip headerlength :iphdr + 1 跳过 IP 头，指向 UDP 头 
                struct rte_udp_hdr *udphdr = (struct rte_udp_hdr *)((uint8_t*)iphdr + ip_hdr_len);

                uint16_t ip_total_len = ntohs(iphdr->total_length); // IP 包整体长度
                uint16_t dgram_len = ntohs(udphdr->dgram_len); //UDP 整体长度
                if (dgram_len < 8 ||
                    ip_hdr_len + dgram_len > ip_total_len ||
                    sizeof(struct rte_ether_hdr) + ip_total_len > rte_pktmbuf_pkt_len(udpmbuf)) {
                    rte_pktmbuf_free(udpmbuf);
                    return -1;
                }

                struct in_addr addr;
                addr.s_addr = iphdr->src_addr;
                printf("udp_process ---> src: %s:%d \n", inet_ntoa(addr), ntohs(udphdr->src_port));

                struct ustack_udp_sock *host = get_hostinfo_fromip_port(iphdr->dst_addr, udphdr->dst_port, iphdr->next_proto_id);
                if (host == NULL) {
                    rte_pktmbuf_free(udpmbuf);
                    return -3;
                }
                
                struct ustack_offload *ol = rte_malloc("offload", sizeof(struct ustack_offload), 0);
                if (ol == NULL) {
                    rte_pktmbuf_free(udpmbuf);
                    return -1;
                }

                ol->dip = iphdr->dst_addr;
                ol->sip = iphdr->src_addr;
                ol->sport = udphdr->src_port;
                ol->dport = udphdr->dst_port;

                ol->protocol = IPPROTO_UDP;
                ol->length = ntohs(udphdr->dgram_len); // total length.

                ol->data = rte_malloc("unsigned char*", ol->length - sizeof(struct rte_udp_hdr), 0);
                if (ol->data == NULL) {
                    rte_pktmbuf_free(udpmbuf);
                    rte_free(ol);
                    return -2;
                }
                rte_memcpy(ol->data, (unsigned char*)(udphdr + 1), ol->length - sizeof(struct rte_udp_hdr));

                // There is necessary to consider whether enqueue or not.
                if (rte_ring_mp_enqueue(host->rcvbuf, ol) != 0) {
                    rte_free(ol->data); 
                    rte_free(ol); 
                    rte_pktmbuf_free(udpmbuf);
                    return -5;
                }

                pthread_mutex_lock(&host->mutex);
                pthread_cond_signal(&host->cond);
                pthread_mutex_unlock(&host->mutex);

                rte_pktmbuf_free(udpmbuf);

                return 0;
}

int udp_out(struct rte_mempool *mbuf_pool) {

    struct ustack_udp_sock* host;
    for (host = lhost; host != NULL; host = host->next) {
        
        struct ustack_offload *ol;

        // 每一次只从 sndbuf 之中取一个数据包，sndbuf 积压 n个包需要pkt_process处理n轮循环才能取出来。
        int nb_snd= rte_ring_mc_dequeue(host->sndbuf, (void**)&ol);
        if (nb_snd < 0) continue;
        
        struct in_addr addr;
        addr.s_addr = ol->dip;
        printf("udp_out ---> src :%s:%d \n", inet_ntoa(addr), ntohs(ol->dport));

        struct inout_ring *ring = ringInstance();

        uint8_t *dstmac = ng_get_dst_macaddr(ol->dip);
        // 如果找不到目的 MAC 地址，构造 ARP request发送
        if(dstmac == NULL) {
            struct rte_mbuf *arpbuf = ustack_send_arp(mbuf_pool, RTE_ARP_OP_REQUEST, gDefaultArpMac,
                                                ol->sip, ol->dip);

            if (rte_ring_mp_enqueue_burst(ring->out, (void**)&arpbuf, 1, NULL) != 1) {
                rte_pktmbuf_free(arpbuf);
            }

             /* 数据先放回 sndbuf，等 ARP 表更新后再发 */
            rte_ring_mp_enqueue(host->sndbuf, ol);
            continue;
        } else {
            struct rte_mbuf *udpbuf = ustack_udp_pkt(mbuf_pool,
                                                      ol->sip, ol->dip, ol->sport, ol->dport, host->localmac,
                                                      dstmac, ol->data, ol->length);
            if (udpbuf != NULL && rte_ring_mp_enqueue_burst(ring->out, (void**)&udpbuf, 1, NULL) != 1) {
                rte_pktmbuf_free(udpbuf);
            }

            rte_free(ol->data);
            rte_free(ol);
        }
    }

    return 0;
}

// Multi thread
static int pkt_process(__attribute__((unused)) void *arg) {
    
    struct rte_mempool *mbuf_pool = (struct rte_mempool *)arg;
    struct inout_ring *ring = ringInstance();

    while (1) {

        struct rte_mbuf *mbufs[BURST_SIZE];
        unsigned num_recvd = rte_ring_mc_dequeue_burst(ring->in, (void**)mbufs, BURST_SIZE, NULL);

        for (unsigned i = 0; i < num_recvd ;i++) {

            struct rte_ether_hdr *ethhdr = rte_pktmbuf_mtod(mbufs[i], struct rte_ether_hdr*);

            // 处理 ARP 报文
            if (ethhdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP)) {

                // 长度校验：不足一个完整 ARP 帧则丢弃，防止越界读
                if (rte_pktmbuf_data_len(mbufs[i]) < sizeof(struct rte_ether_hdr) + sizeof(struct rte_arp_hdr)) {
                    rte_pktmbuf_free(mbufs[i]);
                    continue;
                }

                struct rte_arp_hdr *arphdr = rte_pktmbuf_mtod_offset(mbufs[i], struct rte_arp_hdr *, sizeof(struct rte_ether_hdr));

                // 只处理 “以太网 + IPv4” 的 ARP，其余格式忽略
                if (arphdr->arp_hardware != rte_cpu_to_be_16(RTE_ARP_HRD_ETHER) ||
                    arphdr->arp_protocol != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4) ||
                    arphdr->arp_hlen != RTE_ETHER_ADDR_LEN ||
                    arphdr->arp_plen != sizeof(uint32_t)) {
                    rte_pktmbuf_free(mbufs[i]);
                    continue;
                }

                const uint32_t arp_sip = arphdr->arp_data.arp_sip;   // 发送方 IP
                const uint32_t arp_tip = arphdr->arp_data.arp_tip;   // 被询问的 IP

                struct in_addr addr;
                char sip_str[INET_ADDRSTRLEN], tip_str[INET_ADDRSTRLEN];
                addr.s_addr = arp_sip;
                inet_ntop(AF_INET, &addr, sip_str, sizeof(sip_str));
                addr.s_addr = arp_tip;
                inet_ntop(AF_INET, &addr, tip_str, sizeof(tip_str));

                // 只处理与本机 IP 相关的 ARP：请求的目标 IP 或应答的目标 IP 必须是本机 IP
                if (arp_tip == gLocalIp) {

                    // 收到 ARP Request，打印请求信息并发送 ARP Reply  
                    if (arphdr->arp_opcode == rte_cpu_to_be_16(RTE_ARP_OP_REQUEST)) {

                        printf("ARP request: who has %s? tell %s\n", tip_str, sip_str);

                        // 单连接假设：把请求方 MAC 记为默认目的 MAC，只能记录一次对方 MAC 地址
                        // 真实的 MAC 地址需要处理 arp_table 和 arp_item 来处理多项 ARP 缓存表
                        rte_memcpy(global_dmac, arphdr->arp_data.arp_sha.addr_bytes, RTE_ETHER_ADDR_LEN);

                        printf("the opposite mac is %02x:%02x:%02x:%02x:%02x:%02x\n",
                               global_dmac[0], global_dmac[1], global_dmac[2],
                               global_dmac[3], global_dmac[4], global_dmac[5]);
                        
                        // 构造返回的 ARP reply
                        struct rte_mbuf *arp_mbuf = ustack_send_arp(mbuf_pool, RTE_ARP_OP_REPLY, 
                                                        arphdr->arp_data.arp_sha.addr_bytes,
                                                        gLocalIp, arp_sip);

                        //if (arp_mbuf && rte_eth_tx_burst(global_portid, 0, &arp_mbuf, 1) != 1)
                        //    rte_pktmbuf_free(arp_mbuf);

                            if (rte_ring_mp_enqueue_burst(ring->out, (void**)&arp_mbuf, 1, NULL) != 1) {
                                rte_pktmbuf_free(arp_mbuf);
                            }

                            // 收到 ARP Reply，进行动态记录：
                            // 1. 若表中不存在该 IP 映射，则新增一条记录。
                            // 2. 若表中已存在该 IP 映射：
                            //    - 如果 MAC 没变，刷新其老化时间（防止正在通信的缓存被误删）。
                            //    - 如果 MAC 变了（如虚拟机迁移），直接覆盖旧的 MAC 地址。
                    } else if (arphdr->arp_opcode == rte_cpu_to_be_16(RTE_ARP_OP_REPLY)) {
                        
                        ng_arp_entry_insert(arphdr->arp_data.arp_sip, arphdr->arp_data.arp_sha.addr_bytes);
                        
                        printf("ARP reply: %s is at %02x:%02x:%02x:%02x:%02x:%02x\n", sip_str,
                               arphdr->arp_data.arp_sha.addr_bytes[0], arphdr->arp_data.arp_sha.addr_bytes[1],
                               arphdr->arp_data.arp_sha.addr_bytes[2], arphdr->arp_data.arp_sha.addr_bytes[3],
                               arphdr->arp_data.arp_sha.addr_bytes[4], arphdr->arp_data.arp_sha.addr_bytes[5]);
                    }
                }

                rte_pktmbuf_free(mbufs[i]);
                continue;
            }

            // 处理 IPv4 报文
            if (ethhdr->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
                rte_pktmbuf_free(mbufs[i]);
                continue;
            }

            // 跳过 14 字节以太网头，定位 IPv4 头
            struct rte_ipv4_hdr *iphdr = rte_pktmbuf_mtod_offset(mbufs[i], struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));

            uint8_t version = iphdr->version_ihl >> 4;
            uint8_t ihl     = iphdr->version_ihl & RTE_IPV4_HDR_IHL_MASK; 
            
            // 兼容带 options 的 IPv4 头部
            if (version != 4 || ihl < RTE_IPV4_MIN_IHL) {
                rte_pktmbuf_free(mbufs[i]);
                continue;
            }
            uint16_t ip_hdr_len = ihl * RTE_IPV4_IHL_MULTIPLIER;

            if (ip_hdr_len > rte_pktmbuf_pkt_len(mbufs[i]) - sizeof(struct rte_ether_hdr)) {
                rte_pktmbuf_free(mbufs[i]);
                continue;
            }

            // 处理 ICMP 报文
            if (iphdr->next_proto_id == IPPROTO_ICMP) {

                struct rte_icmp_hdr *icmp_hdr = (struct rte_icmp_hdr *)((uint8_t *)iphdr + ip_hdr_len);
                if (icmp_hdr->icmp_type == RTE_IP_ICMP_ECHO_REQUEST) {

                    struct rte_mbuf *icmp_mbuf = ustack_send_icmp(mbuf_pool, ethhdr->s_addr.addr_bytes, iphdr->dst_addr, iphdr->src_addr
                                                , icmp_hdr->icmp_ident, icmp_hdr->icmp_seq_nb); 
                    
                //if (rte_eth_tx_burst(global_portid, 0, &icmp_mbuf, 1) != 1) 
                //    rte_pktmbuf_free(icmp_mbuf);
                

                if (rte_ring_mp_enqueue_burst(ring->out, (void**)&icmp_mbuf, 1, NULL) != 1){
                    rte_pktmbuf_free(icmp_mbuf);
                }
                    continue;
                }
            }

            // 处理 UDP 报文（此处假设 IP 头无 options，长度固定 20 字节）
            if(iphdr->next_proto_id == IPPROTO_UDP) {
                
                printf("udp_proceas\n");
                udp_process(mbufs[i]);

                // 处理 TCP 报文
            } else if (iphdr->next_proto_id == IPPROTO_TCP) {

                printf("ustack_tcp_process\n");
                ustack_tcp_process(mbufs[i]);
            
            }
        }
        // 应用层调用 sendto 时将数据塞进 sndbuf 里面的数据发送
        udp_out(mbuf_pool);

        tcp_out(mbuf_pool);
    }

    return 0;
}



#define DEFAULT_FD_NUM 3

#define MAX_FD_COUNT 1024

static unsigned char fd_table[MAX_FD_COUNT] __attribute__((unused)) = {0};

/**
 * @brief Allocates a new file descriptor (fd) from the global bitmap.
 * 
 * @details 
 * This function searches the global bitmap (`fd_table`) starting from 
 * `DEFAULT_FD_NUM` (typically 3, as 0, 1, 2 are reserved for stdin, stdout, 
 * and stderr). It scans for the first available bit (value 0), marks it as 
 * occupied (value 1), and returns the corresponding fd. This ensures that 
 * each socket call gets a unique and valid descriptor.
 * 
 * @note The search is bounded by `MAX_FD_COUNT`. If the bitmap is fully 
 * occupied, it returns -1 to indicate resource exhaustion.
 * 
 * @return The allocated fd on success, or -1 if no free fd is available.
 */
int get_fd_frombitmap(void) {

    int fd = DEFAULT_FD_NUM;
    return fd;
}

/**
 * @brief Releases an fd back to the bitmap, making it available for reuse.
 * 
 * @details 
 * Clears the corresponding bit in the global `fd_table`. It locates the 
 * target byte using `fd / 8`, creates a mask for the specific bit using 
 * `1 << (fd % 8)`, inverts the mask, and applies a bitwise AND to clear it.
 * 
 * @param[in] fd The file descriptor to release.
 * @return 0 on success, -1 if the fd is invalid or out of bounds.
 */
int set_fd_frombitmap(__rte_unused int fd) {

}

/**
 * @brief Retrieves the socket control block (TCB/UCB) associated with a given fd.
 * 
 * @details 
 * This function acts as a central dispatcher for the socket layer. Given an fd, 
 * it searches through the global registries to find the corresponding network 
 * context, mainly used for POSIX APIs complementation. It supports three types of objects:
 * 1. UDP/RAW connections stored in the `ustack_udp_sock` linked list.
 * 2. TCP connections stored in the `tcp_table` (if TCP is enabled).
 * 3. The epoll instance itself (if the fd belongs to an epoll object).
 * 
 * The search is currently implemented as an O(N) linear traversal, which is 
 * suitable for single-core or low-concurrency scenarios. For high-performance 
 * industrial stacks, replacing this with a hash table (e.g., `rte_hash`) 
 * keyed by fd is highly recommended which may be applied later.
 * 
 * @param[in] sockfd The file descriptor to look up.
 * @return void* A pointer to the control block (e.g., `struct ustack_udp_sock *`, 
 *               `struct ng_tcp_stream *`, or `struct eventpoll *`), or NULL if 
 *               the fd is not found.
 */
void* get_hostinfo_fromfd(int sockfd) {
    struct ustack_udp_sock *host;
    for (host = lhost; host != NULL; host = host->next) {
        if (sockfd == host->fd) {
            return host;
        }
    }
    return NULL;
}

/**
 * @brief 
 * 
 * @details This function mainly used between data from NICs and network stack.It will find
 * a `ustack_udp_sock` object by three demension network demension.In this workflow, there is 
 * no given file description which only used in application layer  
 * 
 * @param[in] ip 
 */
struct ustack_udp_sock * get_hostinfo_fromip_port(uint32_t dip, uint16_t port, uint8_t proto) {
    struct ustack_udp_sock *host;
    for (host = lhost; host != NULL; host = host->next) {
        if (dip == host->localip &&port == host->localport && proto == host->protocol) {
            return host;
        }
    }
    return NULL;
}

//TODO:后续需要解决和系统 glibc 链接提供的同名API出现的符号劫持问题
// 重点是需要利用 fd 来区分何时调用系统 API， 何时采用本协议栈为应用层提供的 API
// Poxis API
int socket(__rte_unused int domain, int type,__rte_unused int protocol) {
    
    int fd = get_fd_frombitmap();

    if (type == SOCK_DGRAM) {

        // Only call socket API could create a udp_cock object.
        struct ustack_udp_sock *host = rte_malloc("UDP tcb", sizeof(struct ustack_udp_sock), 0);
        if (host == NULL) {
            return -1;
        }

        memset(host, 0, sizeof(struct ustack_udp_sock));
        host->fd = fd;

        host->protocol = IPPROTO_UDP;

        host->rcvbuf = rte_ring_create("receive buffer", RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
        if (host->rcvbuf == NULL) {
            rte_free(host); // Cautions: remember to free the allocated host.
            return -1;
        }
    
        host->sndbuf = rte_ring_create("send buffer", RING_SIZE, rte_socket_id(), RING_F_SC_DEQ | RING_F_SP_ENQ);
        if (host->sndbuf == NULL) {

            rte_ring_free(host->rcvbuf); // Catutions: remember to release allocated memory
            rte_free(host);
            return -1;
        }

        pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
        rte_memcpy(&host->cond, &blank_cond, sizeof(pthread_cond_t));

        pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
        rte_memcpy(&host->mutex, &blank_mutex, sizeof(pthread_mutex_t));



        LL_ADD(host, lhost);
    } else if (type == SOCK_STREAM) {
        //^^^^^^
    }
    return fd;
}

int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    
    // Find the UDP control block
    struct ustack_udp_sock *host = get_hostinfo_fromfd(sockfd);
    if (host == NULL) return -1;

    if (addr == NULL || addrlen < sizeof(struct sockaddr_in)) return -1;

    // Notice that sockaddr_in is designed for IPv4 
    struct sockaddr_in *localaddr = (struct sockaddr_in*)addr;
    if (localaddr->sin_family != AF_INET) return -1;

    host->localport = localaddr->sin_port;

    rte_memcpy(&host->localip, &localaddr->sin_addr.s_addr, sizeof(uint32_t));
    rte_memcpy(host->localmac, global_smac, RTE_ETHER_ADDR_LEN);

    return 0;
}

 ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen) {

    struct ustack_udp_sock *host = get_hostinfo_fromfd(sockfd);
    if (host == NULL) return -1;

    const struct sockaddr_in *daddr = (const struct sockaddr_in *)dest_addr;
    struct ustack_offload *ol = rte_malloc("offload", sizeof(struct ustack_offload), 0);

    if (ol == NULL) return -1;

    ol->dip = daddr->sin_addr.s_addr;
    ol->dport = daddr->sin_port;
    ol->sip = host->localip;
    ol->sport = host->localport;
    ol->length = len;
    
    struct in_addr addr;
    addr.s_addr = ol->dip;
    RTE_LOG(INFO, USER1, "sendto dst ip: %s, dst port: %d\n", inet_ntoa(addr), ntohs(ol->dport));

    ol->data = rte_malloc("unsigned char *", len, 0);
    if (ol->data == NULL) {
        rte_free(ol);
        return -1;
    }

    rte_memcpy(ol->data, buf, len);
    
    if (rte_ring_mp_enqueue(host->sndbuf, ol) != 0){
        rte_free(ol->data);
        rte_free(ol);
    }

    return len;

} 

// TODO: more need to do.
ssize_t recvfrom(int sockfd, void *buf, size_t len, __rte_unused int flags,
                        struct sockaddr *src_addr, __rte_unused socklen_t *addrlen) {
        struct ustack_udp_sock* host = get_hostinfo_fromfd(sockfd);
        if (host == NULL) return -1;
         
        struct ustack_offload *ol = NULL;
        unsigned char *ptr = NULL;

        int nb = -1;

        pthread_mutex_lock(&host->mutex);
        // Blocking in the queue
        while ((nb = rte_ring_mc_dequeue(host->rcvbuf, (void **)&ol)) < 0) {

            //if (flags & MSG_DONTWAIT || (host->bflags & O_NONBLOCK)) {
            //pthread_mutex_unlock(&host->mutex);
            //errno = EAGAIN;
            //return -1;
        //}

            pthread_cond_wait(&host->cond, &host->mutex);
        }
        pthread_mutex_unlock(&host->mutex);

        struct sockaddr_in *saddr = (struct sockaddr_in*) src_addr;
        saddr->sin_port = ol->sport;
        rte_memcpy(&saddr->sin_addr.s_addr, &ol->sip, sizeof(uint32_t));


        // TODO: 不太符合 UDP 协议语义，如果 len 小于实际数据大小应该直接截断
        
        // 如果 len 小于offload里面实际数据大小，应该做一次截断，将指针向数据后方移动，再一次入队，等待下一次传递数据
        if (len < ol->length) {
            rte_memcpy(buf, ol->data, len);

            ptr = rte_malloc("unsigned char *", ol->length - len, 0);
            rte_memcpy(ptr, ol->data + len, ol->length - len);

            ol->length -= len;
            rte_free(ol->data);
            ol->data = ptr;

            rte_ring_mp_enqueue(host->rcvbuf, ol);
            
            return len;

        // 如果 len 大于等于offload里面实际数据大小，就一次拷贝过去。
        } else {
            uint16_t copy_len = (len < ol->length) ? len : ol->length;

            rte_memcpy(buf, ol->data, ol->length);

            rte_free(ol->data);
            rte_free(ol);
            // 不能直接返回ol-length，注意资源的管理：ol已经被释放了
            return copy_len;
        }
}

/**
 * @brief Closes a UDP socket and releases all associated resources.
 * 
 * @details 
 * This function terminates the socket connection and cleans up the underlying 
 * user-space resources. In a DPDK environment (like NitroStack), the operations 
 * include:
 * 1. Locating the socket control block (UCB) using the `fd`.
 * 2. Removing the UCB from the global socket management list to 
 *    prevent the protocol core from dispatching new packets to it.
 * 3. Freeing any pending `ustack_offload` structures in the `rcvbuf` and `sndbuf` 
 *    to prevent memory leaks.
 * 
 * @param[in] fd The file descriptor of the socket to be closed.
 * @return 0 on success, -1 on failure 
 */
int close(int fd) {
    struct ustack_udp_sock *host = get_hostinfo_fromfd(fd);
    if (host == NULL) return -1;

    // TODO: 解决与 Linux 系统的 glibc 产生的符号劫持问题
    /*
    if (host == NULL) {        // 不是我的 socket，交给真正的 libc close 
        extern int __close(int);   
        return __close(fd);
    */

    LL_REMOVE(host, lhost);

    if (host->rcvbuf) {
        rte_ring_free(host->rcvbuf);
    }
    if (host->sndbuf) {
        rte_ring_free(host->sndbuf);
    }

    rte_free(host);
    return 0;
}
 
#define UDP_APP_RECV_BUFFER_SIZE 128

/** 
 * @brief This function is used for an application thread in UDP services.
 *  
 * @details 
 * This function implements a standard UDP Echo server using POSIX Socket APIs. 
 * It creates a UDP socket, binds it to a certain port on all local interfaces (0.0.0.0), 
 * and enters an infinite loop to receive data from clients and echo it back.
 * 
 * @note This is the baseline implementation using standard Linux kernel networking APIs. 
 * However, this exact code can be seamlessly migrated to a DPDK user-space stack 
 * (like NitroStack) via POSIX API hooking (e.g., LD_PRELOAD). 
 * In the DPDK environment:
 * 1. `socket()` allocates a `struct ustack_udp_sock` control block instead of a kernel fd.
 * 2. `bind()` writes the port into the control block's `localport` field.
 * 3. `recvfrom()` will block on a `pthread_cond_t` and extract data from the `rcvbuf` 
 *    (rte_ring), completely bypassing the kernel network stack.
 * 
 * @param[in] arg The number of arguments pointer.
 * @return 0 on success, -1 on failure.
 **/ 
static int udp_server_entry(__rte_unused void *arg) {
        int connfd = socket(AF_INET, SOCK_DGRAM, 0);
        if (connfd == -1) {
            printf("sockfd failed\n");
            return -1;
        }

        struct sockaddr_in localaddr, clientaddr;
        memset(&localaddr, 0, sizeof(struct sockaddr_in));

        localaddr.sin_port = htons(8889);
        localaddr.sin_family = AF_INET;
        localaddr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0

        bind(connfd, (struct sockaddr*)&localaddr, sizeof(localaddr));
        
        char buffer[UDP_APP_RECV_BUFFER_SIZE] = {0};
        socklen_t addrlen = sizeof(clientaddr);

        while (1) {

            if (recvfrom(connfd, buffer, UDP_APP_RECV_BUFFER_SIZE, 0,
            (struct sockaddr*)&clientaddr, &addrlen) < 0) {
                continue;
            } else {
                printf("receive from %s:%d, data:%s\n", inet_ntoa(clientaddr.sin_addr),
                ntohs(clientaddr.sin_port), buffer);
                sendto(connfd, buffer, strlen(buffer), 0,
                (struct sockaddr*)&clientaddr, sizeof(clientaddr));
            }
        } 

        close(connfd);
}

/**
 * @brief 根据四元组或端口搜索对应的 TCP 控制块（TCB）
 * 
 * 在 DPDK 用户态协议栈中，网卡收到的每一个包都需要通过这个函数
 * 找到它所属的 TCP 连接（TCB），从而进行后续的状态机处理。
 * 
 * @param sip  local ip
 * @param dip  destinate IP address 
 * @param sport local port 
 * @param dport destinate port
 * @return struct ustack_tcp_stream* 
 *         - 找到匹配的连接：返回指向该 TCB 的指针
 *         - 没找到：返回 NULL
 */
static struct ustack_tcp_stream* ustack_tcp_stream_search (uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport) {

    struct ustack_tcp_table *table = tcpInstance();

    struct ustack_tcp_stream *iter;
    
    // 四元组匹配已经建立的连接 --> 直接返回相应的建立连接块
    for (iter = table->tcb_set; iter != NULL; iter = iter->next) { // search established

        if (iter->sip == sip && iter->dip == dip &&
            iter->sport == sport && iter->dport == dport) {
                return iter;
            }
    }

    // 如果没有连接，则查看是否有监听在外来包目的端口的监听控制块-->应用层的监听 sokcet
    for (iter = table->tcb_set; iter != NULL; iter =iter->next) {

        if (iter->sport == dport && iter->status == USTACK_TCP_STATUS_LISTEN) {
            return iter;
        }
    }

    return NULL;
}

static struct ustack_tcp_stream *ustack_tcp_stream_create(uint32_t sip, uint32_t dip, uint16_t sport, uint16_t dport) {

    struct ustack_tcp_stream *stream = rte_malloc("ustack_tcp_stream", sizeof(struct ustack_tcp_stream), 0);
    if (stream == NULL) return NULL;
    
    stream->sip = sip;
    stream->dip = dip;
    stream->sport = sport;
    stream->dport = dport;
    stream->protocol = IPPROTO_TCP;
    stream->fd = -1; //unused fd value

    stream->status = USTACK_TCP_STATUS_LISTEN;

    printf("ustack_tcp_stream_create\n");

    stream->sndbuf = rte_ring_create("sndbuf", RING_SIZE, rte_socket_id, 0);
    stream->rcvbuf = rte_ring_create("rcvbuf", RING_SIZE, rte_socket_id, 0);

    // TODO [FIXME]: 生产环境需替换为更安全的 ISN 生成算法，防止 TCP 伪造攻击。
    // uint64_t now_us = rte_get_timer_cycles() / (rte_get_timer_hz()/ 1000000ULL);
    // stream->snd_nxt = ustack_tcp_isn(sip, sport, dip, dport,gIsqSecret, now_us);
    // 其中 gIsqSecret 是全局秘钥，仅启动时生成一次且必须保密

    uint32_t next_seed = time(NULL);
    stream->snd_nxt = rand_r(&next_seed) % TCP_MAX_SEQ;
    rte_memcpy(stream->localmac, global_smac, RTE_ETHER_ADDR_LEN);

    pthread_cond_t blank_cond = PTHREAD_COND_INITIALIZER;
    rte_memcpy(&stream->cond, &blank_cond, sizeof(pthread_cond_t));

    pthread_mutex_t blank_mutex = PTHREAD_MUTEX_INITIALIZER;
    rte_memcpy(&stream->mutex, &blank_mutex, sizeof(pthread_mutex_t));
    
    struct ustack_tcp_table* table = tcpInstance();
    LL_ADD(stream, table->tcb_set);

    return stream;
}

/**
 * @brief Handle a segment that arrives on a connection in LISTEN state.
 *
 * @details
 * This is the server side entry of the three-way handshake, i.e. the
 * LISTEN -> SYN_RCVD transition of RFC 793:
 *
 *   LISTEN -- recv SYN --> SYN_RCVD
 *
 * A listening TCB carries no peer information yet (fd is not owned by the
 * application, dip/dport are not meaningful, always filling them with zero), 
 * so a SYN must NOT change the state of the listener itself - the listener 
 * has to stay in LISTEN to keep accepting other peers. Instead, the expected 
 * sequence of actions is:
 *
 *   1. create a NEW TCB for the 5-tuple of this SYN,
 *   2. record the peer ISN + 1 into the new TCB's recv_nxt (RCV.NXT),
 *   3. queue a SYN/ACK carrying the local ISN into the new TCB's sndbuf so
 *      that tcp_out() can serialise it later,
 *   4. set the new TCB status to SYN_RCVD and wait for the final ACK.
 *
 * @note A retransmitted SYN must be answered with the same SYN/ACK again, so
 *       nothing here may depend on the SYN arriving exactly once.
 *
 * @param[in,out] stream the TCB on which the segment was matched; on entry it
 *                       is the listening TCB of the local port
 * @param[in]     tcphdr the parsed TCP header of the received segment, already
 *                       bounds checked by the caller
 * @param[in]     iphdr  the parsed IPv4 header of the received segment, used to
 *                       obtain the peer address
 * @return 0 on success, -1 when a TCB or a fragment could not be allocated
 */
static int ustack_tcp_handle_listen (struct ustack_tcp_stream* stream, struct rte_tcp_hdr *tcphdr, struct rte_ipv4_hdr *iphdr) {
    
    if (tcphdr->tcp_flags & RTE_TCP_SYN_FLAG) {
        
        if (stream->status == USTACK_TCP_STATUS_LISTEN) {
            
            struct tcp_fragment *fragment = rte_malloc("ustack_tcp_fragment",sizeof(struct tcp_fragment), 0);
            struct ustack_tcp_stream *syn = ustack_tcp_stream_create(iphdr->dst_addr, iphdr->src_addr, tcphdr->dst_port, tcphdr->src_port);
            if (fragment == NULL) return -1;
            memset(fragment, 0, sizeof(struct tcp_fragment));

            fragment->sport = tcphdr->dst_port;
            fragment->dport = tcphdr->src_port;

            fragment->seqnum = syn->snd_nxt;
            fragment->acknum = ntohs(tcphdr->sent_seq) + 1;
            syn->rcv_nxt = fragment->acknum;
            
            fragment->tcp_flags = (RTE_TCP_SYN_FLAG | RTE_TCP_ACK_FLAG);
            fragment->win_len = TCP_INITIAL_WINDOW;
            fragment->hdrlen_off = 0x50;
            
            fragment->data = NULL;
            fragment->length = 0;

            rte_ring_mp_enqueue(syn->sndbuf, fragment);

            syn->status = USTACK_TCP_STATUS_SYN_RCVD;
        }
    
    }
    return 0;

}

/**
 * @brief Handle a segment that arrives on a connection in SYN_RCVD state.
 *
 * @details
 * SYN_RCVD is the second state of the server side three-way handshake. A TCB
 * enters this state right after it has answered a SYN with a SYN/ACK, so the
 * only segment that may complete the handshake here is the third one:
 *
 *   SYN_RCVD -- recv ACK --> ESTABLISHED
 *
 * The checks that have to be made before the state may change:
 *   1. the segment must carry the ACK flag, otherwise it is stale or hostile
 *      and must be dropped without touching the TCB,
 *   2. its acknowledgement number must equal our ISN + 1 (SND.NXT), which is
 *      the only proof that the peer really received our SYN/ACK. An ACK for
 *      an unexpected sequence number must be ignored,
 *   3. only after both checks pass may the TCB become ESTABLISHED and its fd
 *      be handed to the application by accept(), which requires the
 *      application thread blocked on stream->cond to be woken up,
 *   4. a retransmitted SYN must be answered with the same SYN/ACK again,
 *      otherwise a single lost SYN/ACK would stall the connection forever.
 *
 * @param[in,out] stream the TCB on which the segment was matched; its status
 *                       is SYN_RCVD on entry
 * @param[in]     tcphdr the parsed TCP header of the received segment
 * @param[in]     iphdr  the parsed IPv4 header of the received segment
 * @return 0 on success, -1 when the segment is invalid and must be dropped
 */
static int ustack_tcp_handle_syn_rcvd(struct ustack_tcp_stream *stream, struct rte_tcp_hdr *tcphdr, struct rte_ipv4_hdr *iphdr) {
        
        if (tcphdr->tcp_flags & RTE_TCP_ACK_FLAG) {

            if (stream->status == USTACK_TCP_STATUS_SYN_RCVD) {

                uint32_t acknum = ntohl(tcphdr->recv_ack);
                if (acknum == stream->snd_nxt + 1) {
                    //TODO: connect success
                    //if acknum != snd_nxt + 1 ,we should send a errno as server
                }

                // 对于监听 sokcet 需要考虑到 dip=0 sip=0/localip dport=0 sport=监听的端口
                struct ustack_tcp_stream *listner = ustack_tcp_stream_search(0, 0, 0, stream->sport);
                if (listner == NULL) {
                    rte_exit(EXIT_FAILURE, "ustack_tcp_stream_search failed\n");
                }

                stream->status = USTACK_TCP_STATUS_ESTABLISHED;
            }


        }
    return 0;
}

/**
 * @brief Handle a segment that arrives on an ESTABLISHED connection.
 *
 * @details
 * ESTABLISHED is the only steady state of a TCP connection: both directions
 * are open and every segment carries payload, an acknowledgement, or both.
 * This is the hot path of the stack, and it is responsible for:
 *
 *   1. sequence number checking - a segment is acceptable only when its
 *      sequence number falls inside the receive window, i.e. it lies in
 *      [RCV.NXT, RCV.NXT + window). Out of order segments are either queued
 *      for later reassembly or dropped; a segment that is already covered by
 *      the window is acknowledged again but not delivered twice,
 *   2. moving the payload to the application - the bytes following the TCP
 *      header, whose length is (IP total length - IP header - data offset),
 *      are copied into stream->rcvbuf as a tcp_fragment, and the application
 *      thread blocked in recv() is woken up through stream->cond,
 *   3. advancing RCV.NXT by exactly the number of accepted payload bytes, so
 *      that the ACK we send acknowledges nothing more and nothing less,
 *   4. sending an ACK for the newly received data (a full stack would delay
 *      it up to 500 ms or until a second full sized segment arrives),
 *   5. advancing SND.NXT from the peer acknowledgement number and, in a
 *      complete implementation, driving retransmission and congestion
 *      control from the same place,
 *   6. reacting to FIN - after the peer has closed its send direction the TCB
 *      moves to CLOSE_WAIT and the application is notified, which is why a
 *      handler for this state may not ignore the FIN flag.
 *
 * @note The checksum has already been verified by the caller, so the payload
 *       can be assumed intact when it reaches this function.
 *
 * @param[in,out] stream the TCB on which the segment was matched; its status
 *                       is ESTABLISHED on entry
 * @param[in]     tcphdr the parsed TCP header of the received segment
 * @param[in]     tcplen the length of TCP packet,including the header and data payload
 * @return 0 on success, -1 when the segment is out of window and must be
 *         dropped
 */
static int ustack_tcp_handle_established(struct ustack_tcp_stream *stream, struct rte_tcp_hdr *tcphdr, int tcplen) {

    //ESTABLISHE 状态下收到 SYN 包属于异常行为 
    //利用序列号检查来防御这种异常
    if (tcphdr->tcp_flags & RTE_TCP_SYN_FLAG) {

    }
    if (tcphdr->tcp_flags & RTE_TCP_PSH_FLAG) {

    }
    if (tcphdr->tcp_flags & RTE_TCP_ACK_FLAG) {

    }
    if (tcphdr->tcp_flags & RTE_TCP_FIN_FLAG) {
        
    }


    return 0;
}

/**
 * @brief Handle a segment that arrives on a connection in CLOSE_WAIT state.
 *
 * @details
 * CLOSE_WAIT means the peer has already sent its FIN and that FIN has been
 * acknowledged, but the local application has not called close() yet:
 *
 *   ESTABLISHED -- recv FIN, send ACK --> CLOSE_WAIT
 *   CLOSE_WAIT  -- local close() sends FIN --> LAST_ACK
 *
 * The connection is now half closed: the peer to local direction is finished
 * (the application reads the remaining buffered data and then sees EOF from
 * recv()), while the local to peer direction is still usable, so the
 * application is allowed to keep sending.
 *
 * A handler for this state therefore has to:
 *   1. still acknowledge payload that was already in flight when the FIN was
 *      sent, because a FIN only closes the peer send direction and does not
 *      invalidate the data that preceded it,
 *   2. ignore a repeated FIN from the peer - the peer has already closed once
 *      and acknowledging it twice would desynchronise the sequence numbers,
 *   3. send a FIN and move the TCB to LAST_ACK only when the application
 *      really calls close(), never autonomously,
 *   4. drop everything else, since a well behaved peer has nothing more to
 *      send in this direction.
 *
 * @note A TCB that stays in CLOSE_WAIT forever is a classic application bug:
 *       it means the peer closed and the local side never called close().
 *
 * @param[in,out] stream the TCB on which the segment was matched; its status
 *                       is CLOSE_WAIT on entry
 * @param[in]     tcphdr the parsed TCP header of the received segment
 * @param[in]     iphdr  the parsed IPv4 header of the received segment
 * @return 0 on success, -1 when the segment must be dropped
 */
static int ustack_tcp_handle_close_wait(struct ustack_tcp_stream *stream, struct rte_tcp_hdr *tcphdr, struct rte_ipv4_hdr *iphdr) {

    return 0;
}

/**
 * @brief Handle a segment that arrives on a connection in LAST_ACK state.
 *
 * @details
 * LAST_ACK is the last state of the local close sequence, entered after the
 * application called close() on a half closed connection:
 *
 *   CLOSE_WAIT -- local close() sends FIN --> LAST_ACK
 *   LAST_ACK   -- recv ACK covering our FIN --> CLOSED
 *
 * Very little may happen here, and that is the point:
 *   1. an ACK whose acknowledgement number covers our FIN (SND.NXT) proves
 *      that the peer has seen the FIN, so the TCB may be destroyed and the fd
 *      released back to the bitmap,
 *   2. any payload or FIN received now comes from a peer that has not seen
 *      our FIN yet, so our FIN has to be retransmitted instead of being
 *      silently ignored, otherwise the peer keeps the connection open until
 *      its own timer expires,
 *   3. the TCB must not be freed on anything except a valid ACK, otherwise a
 *      duplicated or delayed segment would destroy a live connection.
 *
 * @note Everything still referenced by the TCB must be released together with
 *       it - both rte_ring queues, every fragment still queued inside them,
 *       and the condition variable / mutex pair - otherwise the rings leak.
 *
 * @param[in,out] stream the TCB on which the segment was matched; its status
 *                       is LAST_ACK on entry, and it may be freed by this
 *                       function once the final ACK is seen
 * @param[in]     tcphdr the parsed TCP header of the received segment
 * @param[in]     iphdr  the parsed IPv4 header of the received segment
 * @return 0 on success, -1 when the segment must be dropped
 */
static int ustack_tcp_handle_last_ack(struct ustack_tcp_stream *stream, struct rte_tcp_hdr *tcphdr, struct rte_ipv4_hdr *iphdr) {
    
    return 0;
}


static int ustack_tcp_process (struct rte_mbuf *tcp_mbuf) {// process received mbuf
    
	struct rte_ipv4_hdr *iphdr =  rte_pktmbuf_mtod_offset(tcp_mbuf, struct rte_ipv4_hdr *, 
				sizeof(struct rte_ether_hdr));
    
    // 要根据头部的字段来动态计算下一个报文头部位置。
    uint8_t ip_hdr_len = (iphdr->version_ihl & 0x0f) * 4;
    struct rte_tcp_hdr *tcphdr = (struct rte_tcp_hdr*)((uint8_t*)iphdr + ip_hdr_len);
    
    uint16_t tcpcksum = tcphdr->cksum;
    tcphdr->cksum = 0;
    uint16_t cksum = rte_ipv4_udptcp_cksum(iphdr, tcphdr);

    if (cksum != tcpcksum) {
        printf("cksum: %x, tcp cksum: %x\n", cksum, tcpcksum);
        return -1;
    }

    struct ustack_tcp_stream *stream = ustack_tcp_stream_search(iphdr->dst_addr, iphdr->src_addr, 
            tcphdr->dst_port, tcphdr->src_port);

    if (stream == NULL) {
        return -2;
    }

    switch (stream->status)
    {
    case USTACK_TCP_STATUS_CLOSED: // client
        break;

    case USTACK_TCP_STATUS_LISTEN: //server
        break;
	case USTACK_TCP_STATUS_SYN_RCVD:// server
        break;
	case USTACK_TCP_STATUS_SYN_SENT:// client
	case USTACK_TCP_STATUS_ESTABLISHED:// server | client 
	case USTACK_TCP_STATUS_FIN_WAIT_1://~client
	case USTACK_TCP_STATUS_FIN_WAIT_2://~client
	case USTACK_TCP_STATUS_CLOSING:// ~client
	case USTACK_TCP_STATUS_TIMEWAIT://~client
	case USTACK_TCP_STATUS_CLOSE_WAIT://~server
	case USTACK_TCP_STATUS_LAST_ACK://~server

    default:
        break;
    }
}

int tcp_out (struct rte_mempool* mbuf_pool) {

    struct ustack_tcp_table *table = tcpInstance();
    struct ustack_tcp_stream *iter;

    for (iter = table->tcb_set; iter != NULL; iter = iter->next) {
        
        struct tcp_fragment *fragment = NULL;
        if (rte_ring_mc_dequeue(iter->sndbuf, (void**)&fragment) < 0) continue;

        uint8_t *dstmac = ng_get_dst_macaddr(iter->dip);// 查目的 IP 对应的 MAC 地址指针

        if (dstmac == NULL) {
            // 分支一：ARP 未命中。目的 IP 对应的 MAC 还不知道，这个描述符不能组包发出。
            // 第一步先广播一个 ARP Request 去问下一跳的 MAC，
            // gDefaultArpMac (ff:ff:ff:ff:ff:ff) 就是直接丢给广播地址的请求帧。
            struct rte_mbuf *arpbuf = ustack_send_arp(mbuf_pool, RTE_ARP_OP_REQUEST, gDefaultArpMac, 
                iter->sip, iter->dip);

            struct inout_ring *ring = ringInstance();
            if (rte_ring_mp_enqueue_burst(ring->out, (void **)&arpbuf, 1, NULL) != 1) {
                rte_pktmbuf_free(arpbuf);
            }

            // 第二步，描述符不能丢，原样塞回 sndbuf：等对端回 ARP Reply、ARP 表被更新之后，
            // 下一轮 tcp_out() 再把它取出来，那时就能命中 MAC 了。
            // 这里不做忙等重试，避免同一轮里反复空转。
            rte_ring_mp_enqueue(iter->sndbuf, fragment);
        
        } else {
            // 分支二：ARP 命中，目的 MAC 已知，可以真正组包并发出去了。

            // 把描述符序列化成完整的以太网帧：以太头 + IPv4 头 + TCP 固定头
            // + TCP 选项 + payload，并依次算好 IP 校验和与 TCP 校验和。
            struct rte_mbuf* tcpbuf = ustack_tcp_pkt(mbuf_pool, iter->sip, iter->dip, 
                iter->localmac, dstmac, fragment);

            struct inout_ring *ring = ringInstance();

            // 组好包后投递到 ring->out，由 IO 核的主循环用 rte_eth_tx_burst 真正打出去：
            // 协议核只负责“造包”，不直接碰网卡。
            // 入队失败说明 out ring 已满，此时 mbuf 还在本核手里，必须自己 free，
            // 否则每丢一个包就泄漏一个 mbuf（最终会把 mempool 耗尽）。
            if (tcpbuf != NULL &&
                rte_ring_mp_enqueue_burst(ring->out, (void **)&tcpbuf, 1, NULL) != 1) {
                    rte_pktmbuf_free(tcpbuf);
            }
            // 描述符已完成使命：先释放它额外持有的 payload 缓冲区，再释放描述符本身。
            // data 可能是 NULL（SYN/ACK、纯 ACK 这类控制报文不带 payload），所以要先判断再释放。
            if (fragment->data != NULL) {
                    rte_free(fragment->data);
            }

                rte_free(fragment);
        }
    }
    return 0;
}

static int tcp_server_entry(__rte_unused void* arg) {

}

int main(int argc, char *argv[]) {

    if (rte_eal_init(argc, argv) < 0) {
        rte_exit(EXIT_FAILURE, "Error with EAL init\n");
    }

    // 创建 mbuf 内存池：收包时每个报文都从这里分配缓冲区
    struct rte_mempool *mbuf_pool = rte_pktmbuf_pool_create("mbuf pool", NUM_MBUFS, 0, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());

    if (mbuf_pool == 0) {
        rte_exit(EXIT_FAILURE, "Could not create mbuf pool\n");
    }

    ustack_init_port(mbuf_pool);

    rte_eth_promiscuous_enable(global_portid); // 开启混杂模式，接收所有报文

    rte_timer_subsystem_init();

    struct rte_timer arp_timer;
    rte_timer_init(&arp_timer);

    uint64_t hz = rte_get_timer_hz();
    unsigned lcore_id = rte_lcore_id();

    // 在哪个核上，多久执行一次，执行对应的回调函数
    rte_timer_reset(&arp_timer, hz, PERIODICAL, lcore_id, arp_request_timer_cb, mbuf_pool); 

    // Initialize the inout ring buffer used to send and receive data
    struct inout_ring *ring = ringInstance();
    if (ring == NULL) {
        rte_exit(EXIT_FAILURE, "ring buffer init failed\n");
    } 

    if (ring->in == NULL) {
        ring->in = rte_ring_create("in ring", RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    }

    if (ring->out == NULL) {
        ring->out = rte_ring_create("out ring", RING_SIZE, rte_socket_id(), RING_F_SP_ENQ | RING_F_SC_DEQ);
    }

    // Each thread shoule bind to a certain thread.
    lcore_id = rte_get_next_lcore(lcore_id , 1, 0);
    // start a thread, pass the task to the logical core
    rte_eal_remote_launch(pkt_process, mbuf_pool, lcore_id);

    // Get the next thread id and bind the udp process logic to this thread.
    lcore_id = rte_get_next_lcore(lcore_id, 1, 0);
    rte_eal_remote_launch(udp_server_entry, mbuf_pool, lcore_id);

    // PMD 主循环：持续轮询网卡 RX 队列并处理收到的报文并做好 TX 队列发送工作
    while (1) {

        // rx queue: receive data from NICs, and put into in ring
        struct rte_mbuf *rx[BURST_SIZE];

        // 从 RX 环形队列批量收包，num_recvd 为本次实际收到的报文数
        uint16_t num_recvd = rte_eth_rx_burst(global_portid, 0, rx, BURST_SIZE);

        if (num_recvd > BURST_SIZE) {
            rte_exit(EXIT_FAILURE, "Error receive from eth\n");
        } else if (num_recvd > 0) {
            uint16_t nb_enq = rte_ring_sp_enqueue_burst(ring->in, (void**) rx, num_recvd, NULL);
            // free those did not enqueue package
            for (uint16_t k = nb_enq; k < num_recvd; k++) {
                rte_pktmbuf_free(rx[k]);
            }
        }
        
        // tx queue: send data to NICs from the out ring
        struct rte_mbuf *tx[BURST_SIZE];
        unsigned nb_deq = rte_ring_sc_dequeue_burst(ring->out, (void**)tx, BURST_SIZE, NULL);
        if (nb_deq > 0) {
            uint16_t nb_tx = rte_eth_tx_burst(global_portid, 0, tx, nb_deq);
            for (unsigned k = nb_tx;k < nb_deq;k++) {
                rte_pktmbuf_free(tx[k]);
            }
        }

        //做解耦工作， 这个主循环只做 rx tx的 IO 工作，其他的迁移
        #if 0
        int i = 0;
        for (; i < num_recvd; i++) {
            struct rte_ether_hdr *ethhdr = rte_pktmbuf_mtod(mbufs[i], struct rte_ether_hdr*);
            int sent = 0;

            // 处理 ARP 报文
            if (ethhdr->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP)) {

                // 长度校验：不足一个完整 ARP 帧则丢弃，防止越界读
                if (rte_pktmbuf_data_len(mbufs[i]) < sizeof(struct rte_ether_hdr) + sizeof(struct rte_arp_hdr)) {
                    rte_pktmbuf_free(mbufs[i]);
                    continue;
                }

                struct rte_arp_hdr *arphdr = rte_pktmbuf_mtod_offset(mbufs[i], struct rte_arp_hdr *, sizeof(struct rte_ether_hdr));

                // 只处理 “以太网 + IPv4” 的 ARP，其余格式忽略
                if (arphdr->arp_hardware != rte_cpu_to_be_16(RTE_ARP_HRD_ETHER) ||
                    arphdr->arp_protocol != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4) ||
                    arphdr->arp_hlen != RTE_ETHER_ADDR_LEN ||
                    arphdr->arp_plen != sizeof(uint32_t)) {
                    rte_pktmbuf_free(mbufs[i]);
                    continue;
                }

                const uint32_t arp_sip = arphdr->arp_data.arp_sip;   // 发送方 IP
                const uint32_t arp_tip = arphdr->arp_data.arp_tip;   // 被询问的 IP

                struct in_addr addr;
                char sip_str[INET_ADDRSTRLEN], tip_str[INET_ADDRSTRLEN];
                addr.s_addr = arp_sip;
                inet_ntop(AF_INET, &addr, sip_str, sizeof(sip_str));
                addr.s_addr = arp_tip;
                inet_ntop(AF_INET, &addr, tip_str, sizeof(tip_str));

                // 只处理与本机 IP 相关的 ARP：请求的目标 IP 或应答的目标 IP 必须是本机 IP
                if (arp_tip == gLocalIp) {

                    // 收到 ARP Request，打印请求信息并发送 ARP Reply  
                    if (arphdr->arp_opcode == rte_cpu_to_be_16(RTE_ARP_OP_REQUEST)) {

                        printf("ARP request: who has %s? tell %s\n", tip_str, sip_str);

                        // 单连接假设：把请求方 MAC 记为默认目的 MAC，只能记录一次对方 MAC 地址
                        // 真实的 MAC 地址需要处理 arp_table 和 arp_item 来处理多项 ARP 缓存表
                        rte_memcpy(global_dmac, arphdr->arp_data.arp_sha.addr_bytes, RTE_ETHER_ADDR_LEN);

                        printf("the opposite mac is %02x:%02x:%02x:%02x:%02x:%02x\n",
                               global_dmac[0], global_dmac[1], global_dmac[2],
                               global_dmac[3], global_dmac[4], global_dmac[5]);
                        
                        // 构造返回的 ARP reply
                        struct rte_mbuf *arp_mbuf = ustack_send_arp(mbuf_pool, RTE_ARP_OP_REPLY, 
                                                        arphdr->arp_data.arp_sha.addr_bytes,
                                                        gLocalIp, arp_sip);

                        if (arp_mbuf && rte_eth_tx_burst(global_portid, 0, &arp_mbuf, 1) != 1)
                            rte_pktmbuf_free(arp_mbuf);

                            // 收到 ARP Reply，进行动态记录：
                            // 1. 若表中不存在该 IP 映射，则新增一条记录。
                            // 2. 若表中已存在该 IP 映射：
                            //    - 如果 MAC 没变，刷新其老化时间（防止正在通信的缓存被误删）。
                            //    - 如果 MAC 变了（如虚拟机迁移），直接覆盖旧的 MAC 地址。
                    } else if (arphdr->arp_opcode == rte_cpu_to_be_16(RTE_ARP_OP_REPLY)) {
                        
                        ng_arp_entry_insert(arphdr->arp_data.arp_sip, arphdr->arp_data.arp_sha.addr_bytes);
                        
                        printf("ARP reply: %s is at %02x:%02x:%02x:%02x:%02x:%02x\n", sip_str,
                               arphdr->arp_data.arp_sha.addr_bytes[0], arphdr->arp_data.arp_sha.addr_bytes[1],
                               arphdr->arp_data.arp_sha.addr_bytes[2], arphdr->arp_data.arp_sha.addr_bytes[3],
                               arphdr->arp_data.arp_sha.addr_bytes[4], arphdr->arp_data.arp_sha.addr_bytes[5]);
                    }
                }

                rte_pktmbuf_free(mbufs[i]);
                continue;
            }

            // 处理 IPv4 报文
            if (ethhdr->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4)) {
                rte_pktmbuf_free(mbufs[i]);
                continue;
            }

            // 跳过 14 字节以太网头，定位 IPv4 头
            struct rte_ipv4_hdr *iphdr = rte_pktmbuf_mtod_offset(mbufs[i], struct rte_ipv4_hdr *, sizeof(struct rte_ether_hdr));

            uint8_t version = iphdr->version_ihl >> 4;
            uint8_t ihl     = iphdr->version_ihl & RTE_IPV4_HDR_IHL_MASK; 
            
            // 兼容带 options 的 IPv4 头部
            if (version != 4 || ihl < RTE_IPV4_MIN_IHL) {
                rte_pktmbuf_free(mbufs[i]);
                continue;
            }
            uint16_t ip_hdr_len = ihl * RTE_IPV4_IHL_MULTIPLIER;

            if (ip_hdr_len > rte_pktmbuf_pkt_len(mbufs[i]) - sizeof(struct rte_ether_hdr)) {
                rte_pktmbuf_free(mbufs[i]);
                continue;
            }

            // 处理 ICMP 报文
            if (iphdr->next_proto_id == IPPROTO_ICMP) {

                struct rte_icmp_hdr *icmp_hdr = (struct rte_icmp_hdr *)((uint8_t *)iphdr + ip_hdr_len);
                if (icmp_hdr->icmp_type == RTE_IP_ICMP_ECHO_REQUEST) {

                    struct rte_mbuf *icmp_mbuf = ustack_send_icmp(mbuf_pool, ethhdr->s_addr.addr_bytes, iphdr->dst_addr, iphdr->src_addr
                                                , icmp_hdr->icmp_ident, icmp_hdr->icmp_seq_nb); 
                    rte_eth_tx_burst(global_portid, 0, &icmp_mbuf, 1);
                    rte_pktmbuf_free(icmp_mbuf);

                    rte_pktmbuf_free(mbufs[i]);
                    continue;
                }
            }

            // 处理 UDP 报文（此处假设 IP 头无 options，长度固定 20 字节）
            if(iphdr->next_proto_id == IPPROTO_UDP) {
                // iphdr + 1 跳过 IP 头，指向 UDP 头
                struct rte_udp_hdr *udphdr = (struct rte_udp_hdr *)(iphdr + 1);
#if ENABLE_SEND
                rte_memcpy(global_smac, ethhdr->d_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
                rte_memcpy(global_dmac, ethhdr->s_addr.addr_bytes, RTE_ETHER_ADDR_LEN);

                rte_memcpy(&global_sip, &iphdr->dst_addr, sizeof(uint32_t));
                rte_memcpy(&global_dip, &iphdr->src_addr, sizeof(uint32_t));

                rte_memcpy(&global_sport, &udphdr->dst_port, sizeof(uint16_t));
                rte_memcpy(&global_dport, &udphdr->src_port, sizeof(uint16_t));
                
                struct in_addr addr;
                addr.s_addr = iphdr->src_addr;
                printf("Source IP : %s:%d -->", inet_ntoa(addr), ntohs(udphdr->src_port));

                addr.s_addr = iphdr->dst_addr;
                printf("Destinate IP %s:%d -->", inet_ntoa(addr), ntohs(udphdr->dst_port));
                
                uint16_t length = ntohs(udphdr->dgram_len) - sizeof(struct rte_udp_hdr);

                struct rte_mbuf *udp_mbuf = ustack_send_udp(mbuf_pool, (uint8_t *)(udphdr + 1), length);

                if (rte_eth_tx_burst(global_portid, 0, &udp_mbuf, 1) != 1)
                    rte_pktmbuf_free(udp_mbuf);

#endif  
                printf("udp : %s\n", (char*)(udphdr + 1));

                // 处理 TCP 报文
            } else if (iphdr->next_proto_id == IPPROTO_TCP) {
                struct rte_tcp_hdr *tcphdr = (struct rte_tcp_hdr *)(iphdr + 1);

                rte_memcpy(global_smac, ethhdr->d_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
				rte_memcpy(global_dmac, ethhdr->s_addr.addr_bytes, RTE_ETHER_ADDR_LEN);

				rte_memcpy(&global_sip, &iphdr->dst_addr, sizeof(uint32_t));
				rte_memcpy(&global_dip, &iphdr->src_addr, sizeof(uint32_t));

				rte_memcpy(&global_sport, &tcphdr->dst_port, sizeof(uint16_t));
				rte_memcpy(&global_dport, &tcphdr->src_port, sizeof(uint16_t));

                global_flags = tcphdr->tcp_flags;
                global_seqnum = ntohl(tcphdr->sent_seq);
                global_seqnum = ntohl(tcphdr->recv_ack);

                struct in_addr addr;
                addr.s_addr = iphdr->src_addr;
                printf("TCP packet source IP %s:%d", inet_ntoa(addr), ntohs(tcphdr->src_port));

                addr.s_addr = iphdr->dst_addr;
                printf("Destinate IP %s:%d, flags: %x, seqnum: %d, acknum: %d\n",
                    inet_ntoa(addr), ntohs(tcphdr->src_port), global_flags, global_seqnum, global_acknum);

                if (global_flags & RTE_TCP_SYN_FLAG) {

                    if (tcp_status == USTACK_TCP_STATUS_LISTEN) {
                        uint16_t total_len = sizeof(struct rte_tcp_hdr) + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_ether_hdr);

                        // lock-free malloc from rte_mempool
                        struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
                        if (!mbuf) {
                            rte_exit(EXIT_FAILURE, "Error rte_pktmbnuf_malloc\n");
                        }
                        mbuf->pkt_len = total_len;
						mbuf->data_len = total_len;
                        
                        uint8_t *msg = rte_pktmbuf_mtod(mbuf, uint8_t *);
                        ustack_encode_tcp_pkt(msg, total_len);
                        rte_eth_tx_burst(global_portid, 0, &mbuf, 1);
                        tcp_status = USTACK_TCP_STATUS_SYN_RCVD;
                    }
                }

                if (global_flags & RTE_TCP_ACK_FLAG) {

                    if (tcp_status == USTACK_TCP_STATUS_SYN_RCVD) {

                        printf("enter established\n");
                        tcp_status = USTACK_TCP_STATUS_ESTABLISHED;
                    }

                }
                if (global_flags & RTE_TCP_PSH_FLAG) {

                    printf("enter established: %d\n", tcp_status);
                    if (tcp_status == USTACK_TCP_STATUS_ESTABLISHED) {

						uint8_t hdrlen = (tcphdr->data_off >> 4) * sizeof(uint32_t);

						uint8_t *data = ((uint8_t*)tcphdr + hdrlen);

						printf("tcp data: %s\n", data);
					}

                }
            }

            // 将报文原样从同一端口回发；发送成功后 mbuf 归驱动所有，应用不能释放
            if (rte_eth_tx_burst(global_portid, 0, mbufs + i, 1) == 1)
                sent = 1;

            // 未成功发送的报文由应用释放，避免 mbuf 泄漏
            if (!sent)
                rte_pktmbuf_free(mbufs[i]);
        }

#endif
        // 定时器 keep until
        static uint64_t prev_tsc = 0, cur_tsc;
        uint64_t diff_tsc;
        
        cur_tsc = rte_rdtsc();
		diff_tsc = cur_tsc - prev_tsc;
		if (diff_tsc > TIMER_RESOLUTION_CYCLES) {
			rte_timer_manage();
			prev_tsc = cur_tsc;
		}


    }

    printf("hello dpdk\n");

}