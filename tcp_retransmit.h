/*
 * RFC 793  - Transmission Control Protocol
 * RFC 1122 - Requirements for Internet Hosts
 * RFC 6298 - Computing TCP's Retransmission Timer
 *
 * Reliable Transmission and Loss Recovery
 *
 * TCP uses sequence numbers, cumulative acknowledgments, and timers to
 * provide reliable, ordered delivery over an unreliable network.
 *
 * Cumulative ACK
 * --------------
 * ACK number N confirms receipt of all bytes with sequence numbers
 * less than N. Gaps indicate missing segments.
 *
 * Fast Retransmit
 * ---------------
 * After receiving 3 duplicate ACKs for the same sequence number, the
 * sender retransmits the segment following the acknowledged sequence
 * without waiting for the RTO to expire.
 *
 * Fast Recovery
 * -------------
 * Instead of collapsing the window after fast retransmit, the sender
 * inflates cwnd to keep the pipe full while retransmitting the missing
 * segment. It exits fast recovery when a full ACK arrives.
 *
 * Retransmission Timeout (RTO)
 * ----------------------------
 * RTO is derived from the smoothed RTT (SRTT) and RTT variance (RTTVAR):
 *
 *   RTO = SRTT + max(G, K * RTTVAR)
 *
 * where G is the clock granularity and K is traditionally 4. The RTO
 * is bounded below to avoid spurious retransmissions.
 */
#ifndef __NG_TCP_RETRANSMIT_H__
#define __NG_TCP_RETRANSMIT_H__


#endif
