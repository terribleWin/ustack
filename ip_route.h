/*
 * RFC 1812 - Requirements for IP Version 4 Routers
 *
 * The routing table maps destination IP addresses to next-hop
 * information. For each destination the stack selects the route with
 * the longest prefix match.
 *
 * A route entry contains:
 *   - destination : network or host address.
 *   - prefix_len  : number of leading 1 bits in the netmask (0..32).
 *   - netmask     : derived from prefix_len.
 *   - gateway     : next-hop IP address, or 0 for directly attached.
 *   - iface       : output interface / port index.
 *   - flags       : e.g., RTF_UP, RTF_GATEWAY, RTF_HOST.
 *   - metric      : administrative cost used when selecting among routes.
 *   - refcount    : number of sockets or flows using this route.
 *
 * Lookup order:
 *   1. Host route (prefix_len == 32).
 *   2. Subnet route.
 *   3. Default route (prefix_len == 0).
 *
 * The table is protected by a lock or RCU because it is read by the
 * packet processing path and updated by control-plane/configuration code.
 */
#ifndef __NG_IP_ROUTE_H__
#define __NG_IP_ROUTE_H__


#endif
