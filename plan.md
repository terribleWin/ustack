# ustack 模块实现规划

按依赖关系从底层到上层、从基础设施到协议特性依次实现。同一层内先实现基础数据面，再实现高级特性。

| 顺序 | 文件 | 所属层次 | 具体功能 |
|------|------|----------|----------|
| 1 | `dpdk_multicore.h` | 基础设施 | DPDK 多核包处理：RSS 分流、每核 RX/TX 队列、per-lcore 协议状态、跨核通信 ring、主核与工作核分工。 |
| 2 | `stats.h` | 基础设施/监控 | 全栈统计计数器：以太网、ARP、IP、ICMP、TCP、UDP、Socket 各层收发包数、错误、重传、连接状态等；支持文本/JSON 导出。 |
| 3 | `ip_route.h` | IP 层 | IPv4 路由表：最长前缀匹配、下一跳、出接口、路由缓存、RTF_UP/GATEWAY/HOST 标志。 |
| 4 | `ip_ttl.h` | IP 层 | TTL 处理：转发时 TTL 递减、TTL=0 时丢弃并回复 ICMP Time Exceeded、首部校验和更新。 |
| 5 | `ip_options.h` | IP 层 | IPv4 选项解析与构造：EOL/NOP、记录路由、时间戳、宽松/严格源路由；未知选项处理。 |
| 6 | `ip_fragment.h` | IP 层 | IP 分片与重组：Identification、Flags/Fragment Offset 解析、DF/MF 处理、重组队列与超时。 |
| 7 | `ipv6.h` | IP 层 | IPv6 基础协议：40 字节固定首部、Next Header/Hop Limit、128 位地址、扩展首部预留接口。 |
| 8 | `tcp_retransmit.h` | TCP 层 | 可靠传输与丢包恢复：累计 ACK、RTO 计算（RFC 6298）、Fast Retransmit、Fast Recovery。 |
| 9 | `tcp_sack.h` | TCP 层 | 选择性确认：SACK 选项格式与协商、SACK 块管理、DSACK 检测重复报文。 |
| 10 | `tcp_nagle.h` | TCP 层 | 小包与确认优化：Nagle 算法、Delayed ACK、TCP_NODELAY、TCP_QUICKACK、TCP_CORK。 |
| 11 | `tcp_cc.h` | TCP 层 | 拥塞控制与流量控制：Slow Start、Congestion Avoidance、CUBIC、BBR、Receive Window。 |
| 12 | `tcp_keepalive.h` | TCP 层 | TCP 保活：空闲探测、探测间隔与次数、SO_KEEPALIVE、连接异常断开检测。 |
| 13 | `posix_api.h` | 应用层 | POSIX Socket API：socket/bind/listen/accept/connect/send/recv/close/setsockopt 等，与 fd.h 事件循环集成。 |

## 说明

- 已存在模块（`ethernet.h`、`arp.h`、`ipv4.h`、`icmp.h`、`tcp.h`、`udp.h`、`epoll.h`、`fd.h`）作为本规划的下层依赖，不再重复实现。
- `tcp_cc.h` 与 `tcp_retransmit.h` 有交叉：Fast Retransmit/Fast Recovery 的触发会修改 `cwnd`/`ssthresh`，实现时建议先完成 `tcp_retransmit.h` 再细化 `tcp_cc.h`。
- `tcp_sack.h` 与 `tcp_retransmit.h` 有交叉：SACK 信息用于更精确的重传决策。
- `tcp_nagle.h` 与 `tcp_cc.h` 有交叉：Nagle/Delayed ACK 影响发送时机，但不影响拥塞窗口计算。
- `ipv6.h` 暂时只做基础解析与转发占位，ICMPv6 ND（邻居发现）可在后续扩展。

## 高级协议与扩展（后续）

在基础 TCP/IP 栈稳定后，可逐步引入以下传输层、网络层与链路层高级特性。范围限定在 TCP/IP 协议栈以内，不涵盖 HTTP/TLS/DHCP 等应用层协议。

| 顺序 | 文件 | 所属层次 | 具体功能 |
|------|------|----------|----------|
| 14 | `ecn.h` | TCP/IP 层 | Explicit Congestion Notification：IP ECN 字段、TCP ECE/CWR 标志、拥塞控制联动。 |
| 15 | `tcp_fastopen.h` | TCP 层 | TCP Fast Open（TFO）：通过 SYN 携带数据减少一次 RTT，需要 cookie 缓存。 |
| 16 | `mptcp.h` | TCP 层 | Multipath TCP：在多条路径上建立子流、联合拥塞控制、路径管理。 |
| 17 | `quic.h` | 传输层 | QUIC 协议：基于 UDP 的多路复用可靠传输，0-RTT/1-RTT 握手、连接迁移。 |
| 18 | `icmpv6_ndp.h` | IP 层/ICMPv6 | IPv6 邻居发现协议（NDP）：地址解析、无状态自动配置、路由器请求/通告。 |
| 19 | `igmp.h` | IP 层 | Internet Group Management Protocol：IPv4 组播组成员管理、Join/Leave/Query。 |
| 20 | `ipsec.h` | IP 层 | IPsec（AH/ESP）：认证头与封装安全载荷、SA 数据库、SPI 匹配。 |
| 21 | `nat.h` | IP 层/控制面 | 连接跟踪与 NAT：SNAT/DNAT、会话表、五元组匹配、超时回收。 |
| 22 | `vlan.h` | 链路层 | IEEE 802.1Q VLAN：Tag 插入/剥离、VID 隔离、优先级字段。 |
| 23 | `vxlan.h` | 链路层/覆盖网络 | VXLAN 隧道：基于 UDP/4789 的二层 overlay、VNI 隔离、封装/解封装。 |
| 24 | `gre.h` | 链路层/隧道 | Generic Routing Encapsulation：IP-in-IP 通用隧道、GRE 头部、Key/Checksum 标志。 |

## 说明

- 已存在模块（`ethernet.h`、`arp.h`、`ipv4.h`、`icmp.h`、`tcp.h`、`udp.h`、`epoll.h`、`fd.h`）作为本规划的下层依赖，不再重复实现。
- `tcp_cc.h` 与 `tcp_retransmit.h` 有交叉：Fast Retransmit/Fast Recovery 的触发会修改 `cwnd`/`ssthresh`，实现时建议先完成 `tcp_retransmit.h` 再细化 `tcp_cc.h`。
- `tcp_sack.h` 与 `tcp_retransmit.h` 有交叉：SACK 信息用于更精确的重传决策。
- `tcp_nagle.h` 与 `tcp_cc.h` 有交叉：Nagle/Delayed ACK 影响发送时机，但不影响拥塞窗口计算。
- `quic.h` 不依赖 TCP 模块，但依赖 UDP 与 socket API；它属于传输层扩展，仍在 TCP/IP 栈范围内。
- `mptcp.h`  deeply 改动 TCP 连接模型，建议在基础 TCP 完全稳定后再引入。
- `nat.h` 需要 `ip_route.h` 和连接表，通常作为网关/代理场景扩展。
- `vlan.h`、`vxlan.h`、`gre.h` 作用于链路层/隧道；实现时需要与 `ethernet.h` 联动处理 EtherType 和封装外层头部。



下面按"对你这个教学协议栈的价值"从高到低，列出**还没用到、但可以融进来**的 DPDK 能力。所有 API 都在你这份 **DPDK 19.08** 里存在（个别标注了版本限制）。

## 一、查表结构：把链表换成哈希/LPM（收益最大、改动最小）

| API | 作用 | 融入点 |
|---|---|---|
| `rte_hash_create()` / `rte_hash_add_key_data()` / `rte_hash_lookup_data()` | 无锁可读的 cuckoo 哈希表 | 替换 `arp.h` 的单链表：O(1) 查 ARP；将来 `ng_tcp_stream_search()` 的四元组查找也用它 |
| `rte_lpm_create()` / `rte_lpm_add()` / `rte_lpm_lookup()` | 最长前缀匹配路由表 | 你已规划了 `ip_route.h`——直接用 LPM 实现"目的 IP → 下一跳/出接口"，别再手写前缀树 |
| `rte_acl_create()` / `rte_acl_classify()` | 多字段分类（五元组规则引擎） | 做包过滤/防火墙 demo，一次 classify 匹配多条规则 |
| `rte_jhash()` / `rte_hash_crc()` | 哈希函数 | 自己建哈希桶时的 hash 函数；`rte_hash_crc` 还能硬件加速（CRC32 指令） |

## 二、网卡高级能力：offload 与多队列

| API / 特性 | 作用 | 融入点 |
|---|---|---|
| `RTE_ETH_TX_OFFLOAD_IPV4_CKSUM` / `UDP_CKSUM` / `TCP_CKSUM` + `rte_ipv4_phdr_cksum()` | 校验和交给网卡算 | 你现在全是软件算；开 offload 后只填伪首部校验和，CPU 省一大截（注意：开了就**不能**再手算） |
| `RTE_ETH_TX_OFFLOAD_TCP_TSO` + `PKT_TX_TCP_SEG` | 硬件 TSO 分段 | 大 TCP 包让网卡切，配合你未来的 `tcp_retransmit.h` |
| `rte_eth_dev_rss_hash_update()` / `rte_eth_dev_rss_reta_update()` | RSS 多队列分流 | 你已规划 `dpdk_multicore.h`：按五元组把流散到多个 RX 队列，每队列一个核 |
| `rte_eth_rx_queue_setup()` 多队列 + `rte_eth_dev_socket_id()` | NUMA 亲和 | 队列/mbuf pool 建在本 socket，跨 socket 访问是大忌 |
| `rte_flow_validate()` / `rte_flow_create()` | 硬件流表：把指定五元组直接导到指定队列 | 比 RSS 更精确的"流→核"绑定 |
| scatter/gather：`rte_pktmbuf_append/adj/trim/prepend`、链式 mbuf（`nb_segs`） | 多段 mbuf 处理 | 支持 jumbo frame / 零拷贝拼接；注意你现在的 `(uint8_t*)iphdr + len` 写法只适用单段，要先 `rte_pktmbuf_linearize()` |
| `rte_pktmbuf_alloc_bulk()` | 批量分配 | 比循环单个 alloc 快 |
| `rte_pktmbuf_clone()` / `rte_pktmbuf_attach()` | 引用计数共享数据 | 做镜像/抓包/重传缓存时不拷数据 |

## 三、分片与聚合（对应你已规划的 `ip_fragment.h`）

| API | 作用 |
|---|---|
| `rte_ipv4_fragment_packet()` | 发送侧按 MTU 分片 |
| `rte_ipv4_frag_reassemble_packet()` + `rte_ipv4_frag_tbl_create()` | 接收侧重组（带超时表） |
| `rte_gro_reassemble_burst()` / `rte_gso_segment()` | 软件 GRO/GSO：把多个小包合成大包再送协议栈，减少 per-packet 开销 |

有了分片，你的 ICMP 才能响应 `ping -s 2000`；现在超 MTU 的包是直接丢的。

## 四、同步与每核数据（多线程化的地基）

| API | 作用 | 融入点 |
|---|---|---|
| `rte_spinlock_*` / `rte_rwlock_*` | 比 pthread 锁轻（忙等，适合临界区极短） | 替换 `arp.h` 里的 `pthread_spinlock`（DPDK 风格统一） |
| `rte_atomic32/64_*` | 原子计数 | 统计计数器 |
| `RTE_DEFINE_PER_LCORE()` / `RTE_PER_LCORE()` | 每核私有变量，无伪共享 | **每核统计**（收包数/丢包数），这是 `stats.h` 最该用的东西 |
| `rte_pause()` | 忙等循环里的 CPU 让步指令 | 所有自旋循环里加，省电且让超线程兄弟核 |
| `rte_mempool` 带 cache：`rte_pktmbuf_pool_create(..., cache_size=256, ...)` | per-lcore 缓存，免锁分配 | 你现在 cache_size 传的是 0，多线程后必须开 |

## 五、统计与可观测性（`stats.h` 的直接素材）

| API | 作用 |
|---|---|
| `rte_eth_stats_get()` / `rte_eth_xstats_get()` | 网卡层收/发/错/丢计数 |
| `rte_metrics_*` | 把指标注册成名字→值的字典 |
| `rte_latencystats_*` / `rte_bitratestats_*` | 时延分布、比特率统计（官方库直接给） |
| `rte_log_register()` + `RTE_LOG()` / `rte_log_set_level()` | 分级日志，替换裸 `printf`（你现在每包 printf 是性能杀手） |
| `rte_pdump`（库）/ `dpdk-pdump` 工具 | 运行时抓包到 pcap，不用改代码就能 wireshark 分析你的栈 |

## 六、配置与交互（让 demo 变成"可玩的程序"）

| API | 作用 |
|---|---|
| `rte_kvargs_parse()` | 解析自定义 `--key=value` 参数（比如 `--ip=192.168.139.200`，替换硬编码 `gLocalIp`） |
| `rte_cfgfile_load()` | 读 ini 配置文件 |
| `rte_cmdline`（`cmdline_*`） | 运行时交互命令行：在线看 ARP 表、加路由、改日志级别 |

## 七、限速与 QoS（对应你 `ENABLE_DDOS_DETECT` 的开关）

| API | 作用 |
|---|---|
| `rte_meter`（`rte_srtcm` / `rte_trtcm`） | 令牌桶测速：对每个流/每个 IP 限速，超限丢包——DDoS 防护 demo 的核心 |
| `rte_sched` | 完整 HQoS 调度器（队列+权重+优先级） |
| `rte_approx` | 把"目标速率"换算成令牌桶参数 |

## 八、加密与安全（对应 `ipsec.h`）

| API | 作用 |
|---|---|
| `rte_cryptodev`（`rte_crypto_op` / AES-GCM 等） | 硬件/软件加解密；ESP 封装 |
| `rte_ipsec`（19.08 已有，实验性） | 直接给你 SA 处理：`rte_ipsec_sa_init/process` |
| `rte_security` | 网卡内联 IPsec（inline crypto offload） |

## 九、架构级替换（进阶方向）

| API | 作用 | 何时考虑 |
|---|---|---|
| `rte_distributor` | 官方"分发器"：自动把包按流分给多个 worker 核 | 不想自己写 ring 分发逻辑时 |
| `rte_eventdev` | 事件驱动模型：包=事件，硬件/软件调度器自动负载均衡 | 想从"轮询"升级到"事件驱动"架构时 |
| `rte_pipeline` | table→action 的可编程转发流水线 | 想做"规则驱动的转发面" |
| `rte_vhost` | 做 vhost-user 后端，让虚拟机直接连你的用户态栈 | 想替代 KNI/桥接，做 VM↔栈 高速通道 |
| `rte_bpf` | 在数据路径里跑 eBPF 字节码做过滤/改写 | 想让用户"在线注入过滤逻辑" |

（注意：`rte_graph` 是 20.05+ 才有的，19.08 用不了。）

## 十、协议层之外的"小工具"

| API | 作用 |
|---|---|
| `rte_get_tsc_hz()` / `rte_cycles` 已有；补 `rte_timer_pending()` | 查询定时器是否挂起 |
| `rte_eth_led_on/off` | 点网口灯（调试物理接线很好玩） |
| `rte_eth_dev_set_mtu()` | 运行时改 MTU |
| `rte_vlan_insert()` / `rte_vlan_strip`（mbuf offload 字段 `vlan_tci`） | 对应你规划的 `vlan.h` |
| `rte_ether_unformat_addr()` | 字符串→MAC 解析（配 CLI 时用） |

## 建议的落地顺序

1. **`rte_hash` 换 ARP 链表** + **`rte_log` 换 printf**（半天工作量，收益立竿见影）；
2. **多队列 RSS + per-lcore 统计**（把 `dpdk_multicore.h` 和 `stats.h` 做实）；
3. **校验和 offload**（先验证 `rte_eth_dev_info_get().tx_offload_capa` 再开）；
4. **`rte_lpm` 路由 + `rte_ipv4_frag` 分片**（让栈能"跨网段"和"扛大包"）；
5. **`rte_meter` 限速**（DDoS demo）；
6. 最后再碰 eventdev / vhost / ipsec 这些大块头。