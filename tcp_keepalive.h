/*
 * RFC 1122 - Requirements for Internet Hosts
 * Section 4.2.3.6 - TCP Keep-Alives
 *
 * Keepalive probes detect dead peers on otherwise idle connections.
 * After the connection has been idle for a configurable amount of time
 * (tcp_keepalive_time), the stack starts sending probes.
 *
 * Each probe is a TCP segment with SEG.SEQ = SND.UNA - 1, forcing the
 * peer to respond with an ACK even if its receive window is zero. If no
 * response is received after tcp_keepalive_probes attempts, separated by
 * tcp_keepalive_intvl, the connection is aborted.
 *
 * Parameters (per-connection or global defaults):
 *   - keepalive_time   : seconds of idle time before probing starts.
 *   - keepalive_intvl  : seconds between consecutive probes.
 *   - keepalive_probes : maximum number of unanswered probes before abort.
 *
 * Keepalives are disabled by default and are only sent when the
 * SO_KEEPALIVE socket option is set.
 */
#ifndef __NG_TCP_KEEPALIVE_H__
#define __NG_TCP_KEEPALIVE_H__


#endif
