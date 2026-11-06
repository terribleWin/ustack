/*
 * RFC 8684 - TCP Extensions for Multipath Operation with Multiple Addresses
 *
 * Multipath TCP (MPTCP) allows a single connection to use multiple IP
 * addresses and paths simultaneously, providing resilience and higher
 * throughput. It is a TCP option-based extension that is backward
 * compatible with regular TCP middleboxes.
 *
 * Key components:
 *   - Path Manager     : creates/destroys subflows across available addresses.
 *   - Scheduler        : decides which subflow sends the next segment.
 *   - Sequence Numbers : 64-bit data sequence number (DSN) across subflows,
 *                        plus per-subflow sequence numbers.
 *   - Congestion Ctrl  : coupled congestion control (e.g., LIA, OLIA, BALIA).
 *
 * MPTCP options (simplified):
 *   - MP_CAPABLE : negotiated in SYN/SYN-ACK to indicate MPTCP support.
 *   - MP_JOIN    : creates additional subflows using tokens and nonces.
 *   - DSS        : Data Sequence Signal maps subflow sequence space to DSN.
 *   - ADD_ADDR / REMOVE_ADDR : announces or withdraws available addresses.
 *
 * Integration:
 *   - Deeply modifies the TCP connection model; implement only after the
 *     basic TCP stack is stable.
 *   - Depends on `ip_route.h` for path availability.
 */
#ifndef __NG_MPTCP_H__
#define __NG_MPTCP_H__


#endif
