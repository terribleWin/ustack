/*
 * RFC 3022 - Traditional IP Network Address Translator (Traditional NAT)
 * RFC 2663 - IP Network Address Translator (NAT) Terminology and Considerations
 *
 * Network Address Translation rewrites IP addresses and/or transport
 * ports in transit packets, allowing multiple private hosts to share a
 * single public address.
 *
 * NAT modes:
 *   - SNAT (Source NAT) : rewrites the source address/port of outgoing
 *                         packets so replies come back to the NAT gateway.
 *   - DNAT (Destination NAT) : rewrites the destination address/port of
 *                              incoming packets to forward them to an internal host.
 *   - NAPT/MASQUERADE   : combines address and port translation.
 *
 * Connection tracking:
 *   A NAT implementation needs a session table keyed by the original 5-tuple
 *   (protocol, src_ip, src_port, dst_ip, dst_port) and the translated 5-tuple.
 *   Sessions time out after a period of inactivity (TCP/UDP/ICMP timeouts differ).
 *
 * Integration:
 *   - Depends on `ip_route.h` for forwarding decisions.
 *   - Uses `stats.h` for session count and translation counters.
 */
#ifndef __NG_NAT_H__
#define __NG_NAT_H__


#endif
