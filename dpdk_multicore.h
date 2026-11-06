/*
 * DPDK Multi-core Packet Processing
 *
 * To scale with multiple CPU cores, the stack assigns one or more RX/TX
 * queues to each lcore. Incoming packets are distributed by RSS (Receive
 * Side Scaling) using a hash of (src_ip, dst_ip, src_port, dst_port,
 * protocol) so that all packets of a single flow land on the same core.
 *
 * Per-core resources:
 *   - dedicated mbuf pools and ring buffers,
 *   - independent protocol stack state (ARP table, connection table),
 *   - per-core timer wheels for TCP timers.
 *
 * Cross-core communication:
 *   - rte_ring for passing packets/flows between cores,
 *   - atomic counters and RCU-style tables for shared read-mostly data.
 *
 * A main lcore handles initialization, port configuration, and control
 * plane; worker lcores run the packet processing loop.
 */
#ifndef __NG_DPDK_MULTICORE_H__
#define __NG_DPDK_MULTICORE_H__


#endif
