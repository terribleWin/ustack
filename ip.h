/*
 * IP Layer Umbrella Header
 *
 * This header pulls together the IPv4/IPv6 protocol implementation and
 * the related IP-layer modules. It is intended to be the single include
 * used by upper-layer code (TCP/UDP) that needs IP services.
 *
 * Included modules:
 *   - ipv4.h        : IPv4 fixed header parsing and checksum.
 *   - ipv6.h        : IPv6 fixed header parsing and extension header hooks.
 *   - ip_route.h    : longest-prefix-match routing table.
 *   - ip_fragment.h : IPv4 fragment reassembly and fragmentation.
 *   - ip_ttl.h      : TTL handling and ICMP Time Exceeded generation.
 *   - ip_options.h  : IPv4 option parsing and source routing.
 */
#ifndef __NG_IP_H__
#define __NG_IP_H__

#include "ipv4.h"
#include "ipv6.h"
#include "ip_route.h"
#include "ip_fragment.h"
#include "ip_ttl.h"
#include "ip_options.h"

#endif
