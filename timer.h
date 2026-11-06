/*
 * TCP Timer Wheel
 *
 * TCP maintains several per-connection timers to handle retransmissions,
 * delayed acknowledgments, keepalives, and connection teardown. This
 * module provides a hierarchical or sorted timer wheel that can schedule
 * thousands of timers with coarse (millisecond) granularity.
 *
 * Timers required by a TCP implementation:
 *
 *   - Retransmission Timer (RTO)
 *       Started when data is sent and no ACK is received. If it expires,
 *       the oldest unacknowledged segment is retransmitted and cwnd/ssthresh
 *       are adjusted. RTO is computed from SRTT and RTTVAR per RFC 6298.
 *
 *   - Delayed ACK Timer
 *       Started when a full-sized segment is received and an immediate ACK
 *       is not required. It fires after ~40 ms (or up to 500 ms per RFC 1122)
 *       to force an ACK if no outgoing data is available to piggyback on.
 *
 *   - Keepalive Timer
 *       Started on idle connections that have SO_KEEPALIVE set. After the
 *       idle time expires, probes are sent at fixed intervals until the
 *       peer responds or the probe limit is reached.
 *
 *   - TIME-WAIT Timer
 *       Started when a connection enters TIME-WAIT after the final ACK.
 *       It ensures that any delayed segments from the old connection do
 *       not confuse a new incarnation using the same 4-tuple. Typical
 *       duration is 2 * MSL (Maximum Segment Lifetime), often 60 seconds.
 *
 *   - Persist Timer
 *       Started when the peer advertises a zero receive window. It probes
 *       the window at increasing intervals to avoid deadlock if the ACK
 *       that would reopen the window is lost.
 *
 * The wheel is driven by a monotonic clock (DPDK rte_get_timer_cycles or
 * rte_rdtsc). Each connection holds a small set of timer handles, and the
 * per-lcore worker periodically expires timers in batch.
 */
#ifndef __NG_TIMER_H__
#define __NG_TIMER_H__


#endif
