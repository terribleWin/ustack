/*
 * RFC 3168 - The Addition of Explicit Congestion Notification (ECN) to IP
 *
 * ECN allows routers to signal congestion without dropping packets.
 * It uses the two least significant bits of the IPv4/IPv6 Traffic Class
 * field and two TCP flags (ECE, CWR).
 *
 * IP ECN codepoints:
 *   - 00 : Not-ECT (not ECN capable)
 *   - 10 : ECT(0)
 *   - 01 : ECT(1)
 *   - 11 : CE (Congestion Experienced)
 *
 * TCP ECN negotiation and signaling:
 *   - SYN     : sender sets ECE and CWR to indicate ECN capability.
 *   - SYN-ACK : receiver sets ECE only if it supports ECN.
 *   - Data    : receiver sets ECE upon seeing CE mark; sender reduces cwnd
 *               and sets CWR to acknowledge.
 *
 * Integration:
 *   - `ipv4.h` / `ipv6.h` must parse and preserve the ECN field.
 *   - `tcp_cc.h` reacts to ECE by entering congestion recovery.
 */
#ifndef __NG_ECN_H__
#define __NG_ECN_H__


#endif
