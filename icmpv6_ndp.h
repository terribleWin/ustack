/*
 * RFC 4861 - Neighbor Discovery for IP Version 6 (IPv6)
 * RFC 4862 - IPv6 Stateless Address Autoconfiguration
 *
 * IPv6 does not use ARP; instead it uses ICMPv6 Neighbor Discovery (NDP)
 * to resolve link-layer addresses, discover routers, and perform
 * stateless autoconfiguration.
 *
 * ICMPv6 Neighbor Solicitation message (simplified):
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |     Type      |     Code      |          Checksum             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Reserved             |            Target Address     |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                  Target Address (cont., 16 bytes)               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                         Source Link-Layer Address Option      +
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Message types:
 *   - Router Solicitation   (Type 133) : host requests routers to advertise.
 *   - Router Advertisement  (Type 134) : router announces prefixes and flags.
 *   - Neighbor Solicitation (Type 135) : resolves target link-layer address.
 *   - Neighbor Advertisement(Type 136) : replies with target link-layer address.
 *   - Redirect              (Type 137) : router suggests a better first hop.
 *
 * This module depends on `ipv6.h` and replaces ARP for IPv6.
 */
#ifndef __NG_ICMPV6_NDP_H__
#define __NG_ICMPV6_NDP_H__


#endif
