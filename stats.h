/*
 * Network Stack Statistics and Monitoring
 *
 * This module exposes counters and gauges useful for debugging,
 * profiling, and operational monitoring. Counters are per-core where
 * applicable and aggregated on read.
 *
 * Categories:
 *   - Ethernet : frames received/transmitted, dropped, errors, CRC.
 *   - ARP      : requests/replies received/transmitted, table hits/misses.
 *   - IP       : packets received/transmitted, forwarded, dropped,
 *                fragmented, reassembled, checksum errors.
 *   - ICMP     : echo requests/replies, errors.
 *   - TCP      : segments received/transmitted, retransmits, accepts,
 *                connects, resets, timeouts, state counts.
 *   - UDP      : datagrams received/transmitted.
 *   - Sockets  : open sockets, buffer usage, errors.
 *
 * A simple text or JSON dump routine can be used by the CLI/monitoring
 * tool to inspect live state. Counters use DPDK rte_atomic64_t or
 * per-lcore arrays to avoid cache-line bouncing.
 */
#ifndef __NG_STATS_H__
#define __NG_STATS_H__


#endif
