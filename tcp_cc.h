/*
 * RFC 5681 - TCP Congestion Control
 * RFC 8312 - CUBIC: A New TCP-Friendly High-Speed TCP Variant
 * RFC 8312 / draft-cardwell-iccrg-bbr-congestion-control - BBR
 *
 * TCP congestion control and flow control cooperate to decide how much
 * data the sender may have outstanding. Congestion control probes for
 * available bandwidth; flow control respects the receiver's buffer.
 *
 * Slow Start
 * ----------
 * cwnd starts small (typically 2-10 MSS) and doubles every RTT until
 * ssthresh is reached or a loss is detected.
 *
 * Congestion Avoidance
 * --------------------
 * Once cwnd >= ssthresh, cwnd grows by roughly one MSS per RTT
 * (additive increase) to gently probe for more bandwidth.
 *
 * CUBIC
 * -----
 * CUBIC replaces the linear congestion avoidance phase with a cubic
 * function of elapsed time since the last congestion event. It performs
 * well on high-bandwidth long-delay networks while remaining TCP-friendly.
 *
 * BBR
 * ---
 * BBR models the bottleneck bandwidth and minimum RTT directly instead
 * of inferring congestion from loss. It paces packets and cycles through
 * probing bandwidth and probing minimum RTT.
 *
 * Receive Window (Flow Control)
 * -----------------------------
 * The sender is also limited by the window advertised by the peer
 * (snd_wnd). The effective window is min(cwnd, snd_wnd, flight size).
 *
 * State kept per TCP connection:
 *   - cwnd     : congestion window, in bytes or MSS units.
 *   - ssthresh : slow start threshold.
 *   - snd_wnd  : sender window advertised by the peer.
 *   - snd_una  : oldest unacknowledged sequence number.
 *   - flight   : amount of unacknowledged data.
 *   - srtt/rttvar : smoothed RTT and variance, used for RTO.
 */
#ifndef __NG_TCP_CC_H__
#define __NG_TCP_CC_H__


#endif
