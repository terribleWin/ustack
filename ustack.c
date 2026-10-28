/* ustack：基于 DPDK 的用户态协议栈实验程序（当前实现 IPv4/UDP 收包打印与原样回发） */

#include <stdio.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ip.h>
#include <arpa/inet.h>

int global_portid = 0;   /* 使用的端口号（对应第一个可用的 ethdev） */

#define NUM_MBUFS 4096   /* mbuf 内存池容量，决定可同时在途的收包缓冲区数量 */
#define BURST_SIZE 128   /* 单次收包批量上限（rte_eth_rx_burst 一次最多收取的报文数） */
#define ENABLE_SEND 1

#if ENABLE_SEND

uint8_t global_smac[RTE_ETHER_ADDR_LEN];
uint8_t global_dmac[RTE_ETHER_ADDR_LEN];

static uint32_t global_sip;
static uint32_t global_dip;

uint16_t global_sport;
uint16_t global_dport;

#endif

/* 端口默认配置：普通以太网帧，最大接收长度 1518 字节 */
static const struct rte_eth_conf port_conf_default = {
    .rxmode = { .max_rx_pkt_len = RTE_ETHER_MAX_LEN }
};

static int ustack_init_port(struct rte_mempool *mbuf_pool) {
    // 统计当前可用的以太网端口数（仅打印信息，本程序只使用 global_portid）
    uint16_t nb_sys_ports = rte_eth_dev_count_avail();
    if (nb_sys_ports == 0) {
        rte_exit(EXIT_FAILURE, "No supported eth found\n");
    }

    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(global_portid, &dev_info);

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
    uint16_t udplen = total_len - sizeof(struct rte_ether_hdr) - sizeof(struct rte_ipv4_hdr);
    udp->dgram_len = htons(udplen);

    rte_memcpy((uint8_t *)(udp + 1), data, udplen);
    udp->dgram_cksum = 0;
    udp->dgram_cksum = rte_ipv4_udptcp_cksum(ip, udp);

    return 0;
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

    // 主循环：持续轮询网卡 RX 队列并处理收到的报文 PMD
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

            // 只处理 IPv4 报文，其它协议（ARP、IPv6 等）直接丢弃
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

            // 只处理 UDP 报文（此处假设 IP 头无 options，长度固定 20 字节）
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
                
                uint16_t length = ntohs(udphdr->dgram_len);
                // UPD datagram + IPv4 header + ethernet header 
                uint16_t total_len = length + sizeof(struct rte_ipv4_hdr) + sizeof(struct rte_ether_hdr);

                struct rte_mbuf *mbuf = rte_pktmbuf_alloc(mbuf_pool);
                if (!mbuf) {
                    rte_exit(EXIT_FAILURE, "Error rte_pktmbuf_alloc\n");
                }

                mbuf->pkt_len = total_len;
                mbuf->data_len = total_len;

                uint8_t *msg = rte_pktmbuf_mtod(mbuf, uint8_t *);
                ustack_encode_udp_pkt(msg, (uint8_t *)(udphdr + 1), total_len);

                rte_eth_tx_burst(global_portid, 0, &mbuf, 1);

#endif  
                printf("udp : %s\n", (char*)(udphdr + 1));

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