/*
 * RFC 896  - Congestion Control in IP/TCP Internetworks
 * RFC 1122 - Requirements for Internet Hosts, Section 4.2.3.4
 *
 * Nagle Algorithm
 * ---------------
 * When a connection has unacknowledged data in flight, the sender MUST
 * not transmit a small segment (smaller than one SMSS) unless all
 * previously sent data has been acknowledged. This reduces the number
 * of tinygrams on the network at the cost of added latency for small
 * writes. It is enabled by default and can be disabled with TCP_NODELAY.
 *
 * Delayed ACK
 * -----------
 * RFC 1122 recommends that a receiver should not delay an ACK for more
 * than 500 ms and should generate an ACK for at least every second
 * full-sized segment. Delayed ACKs reduce ACK traffic but interact with
 * Nagle: a sender waiting for an ACK may stall if the receiver delays
 * too long while the Nagle condition is active.
 *
 * Socket Options
 * --------------
 *   - TCP_NODELAY  : disable the Nagle algorithm; send small segments
 *                    immediately.
 *   - TCP_QUICKACK : disable delayed ACKs; acknowledge every packet
 *                    immediately. The kernel may re-enable delayed ACKs
 *                    automatically after this flag is consumed.
 *   - TCP_CORK     : prevent partial frames from being sent; useful for
 *                    protocols that build a logical record from multiple
 *                    writes. Implied Nagle-like behavior but explicit
 *                    uncork (TCP_CORK = 0 or close) is required.
 *
 * State per connection:
 *   - nagle_enabled   : 1 to enable the Nagle algorithm.
 *   - delayack_enabled: 1 to enable delayed ACKs.
 *   - cork_enabled    : 1 when TCP_CORK is active.
 *   - last_ack_sent   : sequence number of the last ACK generated.
 *   - ack_delay_timer : time when the current ACK was deferred.
 */
#ifndef __NG_TCP_NAGLE_H__
#define __NG_TCP_NAGLE_H__


#endif
