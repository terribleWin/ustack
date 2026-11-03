/* ustack：This program is a simple user-space TCP/IP stack demo based on DPDK, which is used for educational purposes.
   This program implements a minimal DPDK single-threaded polling loop, global variables, and assumes a single connection.
   It performs SYN-ACK and prints data, currently supporting IPv4/UDP/TCP packet reception, printing, and echoing back the original data.
   It also implements basic ARP reply and ICMP echo reply functionality. 
   This program will develop into a well-structured user-space TCP/IP stack in the future base on this demo, 
   with supporting multiple connections, multi-threading, and more features.
   Construct the running environment and compile the program. */

#include <stdio.h>
#include <string.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <arpa/inet.h>

#include "arp.h"

struct rte_mbuf* ustack_send_udp(struct rte_mempool *mbuf_pool, unsigned char *data, uint16_t length);

int global_portid = 0;   /* 使用的端口号（对应第一个可用的 ethdev） */

#define NUM_MBUFS 4096   /* mbuf 内存池容量，决定可同时在途的收包缓冲区数量 */
#define BURST_SIZE 128   /* 单次收包批量上限（rte_eth_rx_burst 一次最多收取的报文数） */

#define ENABLE_SEND 1
#define ENABLE_TCP 1

#define TCP_INIT_WINDOWS 14600

#if ENABLE_SEND

#define MAKE_IPV4_ADDR(a, b, c, d) (a + (b<<8) + (c<<16) + (d<<24))

static uint32_t gLocalIp = MAKE_IPV4_ADDR(192, 168, 139, 137); // 192168139137

uint8_t global_smac[RTE_ETHER_ADDR_LEN];
uint8_t global_dmac[RTE_ETHER_ADDR_LEN];

static uint32_t global_sip;
static uint32_t global_dip;

uint16_t global_sport;
uint16_t global_dport;

#endif

#if ENABLE_TCP

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

uint8_t tcp_status = USTACK_TCP_STATUS_LISTEN;

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

// construct UDP msg
static int ustack_encode_udp_pkt (uint8_t *msg, uint8_t *data, uint16_t total_len) {

    // ether header
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    rte_memcpy(eth->d_addr.addr_bytes, global_dmac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(eth->s_addr.addr_bytes, global_smac, RTE_ETHER_ADDR_LEN);
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
    ip->src_addr = global_sip;
    ip->dst_addr = global_dip;

    // The checksum field must be set to 0 by the caller.
    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);
    
    // udp header
    struct rte_udp_hdr *udp = (struct rte_udp_hdr*)(ip + 1);
    udp->src_port = global_sport;
    udp->dst_port = global_dport;
    uint16_t payload_len = total_len - sizeof(struct rte_ether_hdr) - sizeof(struct rte_ipv4_hdr) - sizeof(struct rte_udp_hdr);
    udp->dgram_len = htons(payload_len + sizeof(struct rte_udp_hdr));

    rte_memcpy((uint8_t *)(udp + 1), data, payload_len);
    udp->dgram_cksum = 0;
    udp->dgram_cksum = rte_ipv4_udptcp_cksum(ip, udp);

    return 0;
}

// construct TCP msg
static int ustack_encode_tcp_pkt(uint8_t *msg, uint16_t total_len) {
    
    // ether header
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    rte_memcpy(eth->d_addr.addr_bytes, global_dmac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(eth->s_addr.addr_bytes, global_smac, RTE_ETHER_ADDR_LEN);
    eth->ether_type = htons(RTE_ETHER_TYPE_IPV4);

    // ip header
    struct rte_ipv4_hdr *ip = (struct rte_ipv4_hdr*)(eth + 1); //msg + 
    ip->version_ihl = 0x45;
    ip->type_of_service = 0;
    ip->total_length = htons(total_len - sizeof(struct rte_ether_hdr));
    ip->packet_id = 0;
    ip->fragment_offset = 0;
    ip->time_to_live = 64;
    ip->next_proto_id = IPPROTO_TCP;
    ip->src_addr = global_sip;
    ip->dst_addr = global_dip;
    
    // The checksum field must be set to 0 by the caller before caculate the checksum.
    ip->hdr_checksum = 0;
    ip->hdr_checksum = rte_ipv4_cksum(ip);

    // tcp header
    struct rte_tcp_hdr *tcp = (struct rte_tcp_hdr*)(ip + 1);
    tcp->src_port = global_sport;
    tcp->dst_port = global_dport;
    tcp->sent_seq = htonl(12345); // 硬编码确定本端选定的初始序列号 ISN， 12346 为第一个数据字节序
    tcp->recv_ack = htonl(global_seqnum + 1);
    tcp->data_off = ((sizeof(struct rte_tcp_hdr) / 4) << 4); // = 0x50，高四位位tcp头部大小 中三位保留 最后一位为 NS（也可以被当做保留位）
    tcp->tcp_flags = RTE_TCP_SYN_FLAG | RTE_TCP_ACK_FLAG;

    tcp->rx_win = TCP_INIT_WINDOWS; //htons(4096);  // rmem
	tcp->cksum = 0;
	tcp->cksum = rte_ipv4_udptcp_cksum(ip, tcp);

    return 0;
}

// construct a ARP responce packet
static int ustack_encode_arp_pkt(uint8_t *msg, const uint8_t *dst_mac, uint32_t sip, uint32_t dip) {

    // ether header
    struct rte_ether_hdr *eth = (struct rte_ether_hdr *)msg;
    rte_memcpy(eth->s_addr.addr_bytes, global_smac, RTE_ETHER_ADDR_LEN);
    rte_memcpy(eth->d_addr.addr_bytes, dst_mac, RTE_ETHER_ADDR_LEN);
    eth->ether_type = htons(RTE_ETHER_TYPE_ARP);

    // arp header
    struct rte_arp_hdr *arp = (struct rte_arp_hdr *)(eth + 1);
    arp->arp_hardware = htons(RTE_ARP_HRD_ETHER);
    arp->arp_protocol = htons(RTE_ETHER_TYPE_IPV4);
    arp->arp_hlen = RTE_ETHER_ADDR_LEN; // 
    arp->arp_plen = sizeof(uint32_t);
    arp->arp_opcode = htons(RTE_ARP_OP_REPLY);
    
    // aha 源 MAC 地址
    rte_memcpy(arp->arp_data.arp_sha.addr_bytes, global_smac, RTE_ETHER_ADDR_LEN);
    // tha 目标 MAC 地址
    rte_memcpy(arp->arp_data.arp_tha.addr_bytes, dst_mac, RTE_ETHER_ADDR_LEN);

    arp->arp_data.arp_sip = sip;
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

/* sip / dip 均为网络字节序；源 MAC 固定用 global_smac，不需要再传参 */
static struct rte_mbuf* ustack_send_arp(struct rte_mempool *mbuf_pool, const uint8_t *dstmac, uint32_t sip, uint32_t dip) {

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
    ustack_encode_arp_pkt(pkt_data, dstmac, sip, dip);

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

    // PMD 主循环：持续轮询网卡 RX 队列并处理收到的报文
    while (1) {
        struct rte_mbuf *mbufs[BURST_SIZE] = {0};

        // 从 RX 环形队列批量收包，num_recvd 为本次实际收到的报文数
        uint16_t num_recvd = rte_eth_rx_burst(global_portid, 0, mbufs, BURST_SIZE);

        if (num_recvd > BURST_SIZE) {
            rte_exit(EXIT_FAILURE, "Error receive from eth\n");
        }

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

                        // ARP 应答的 SPA 必须是本机 IP，TPA 是请求方 IP
                        struct rte_mbuf *arp_mbuf = ustack_send_arp(mbuf_pool,
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
                        
                        uint8_t *hwmac = ng_get_dst_macaddr(arphdr->arp_data.arp_sip);
                        
                        if (hwmac == NULL) {
                            struct arp_table *table = arp_table_instance();
                            struct arp_entry *entry = rte_malloc("arp_entry", sizeof(struct arp_entry), 0);
                            
                            if (entry) {
                                memset(entry, 0, sizeof(struct arp_entry));
                                entry->ip = arphdr->arp_data.arp_sip;

                                // 6 字节 mac 地址拷贝到 arp_entry 结构体中 并且改变成状态
                                rte_memcpy(entry->hwaddr, arphdr->arp_data.arp_sha.addr_bytes, RTE_ETHER_ADDR_LEN);
                                entry->type = ARP_ENTRY_STATUS_DYNAMIC;

                                LL_ADD(entry, table->entries);
                                table->count ++;

                            }
                        
                        }

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

    }

    printf("hello dpdk\n");

}