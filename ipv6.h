/*
 * RFC 8200 - Internet Protocol, Version 6 (IPv6) Specification
 *
 * The IPv6 fixed header is 40 bytes. Options and extension headers are
 * placed between the fixed header and the upper-layer protocol.
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |Version| Traffic Class |           Flow Label                  |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         Payload Length        |  Next Header  |   Hop Limit   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                                                               +
 * |                         Source Address                        |
 * +                         (16 Bytes)                            +
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                                                               +
 * |                      Destination Address                      |
 * +                         (16 Bytes)                            +
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Field definitions:
 *   4.bit   (version)       : Always 6.
 *   8.bit   (traffic_class) : DSCP + ECN.
 *   20.bit  (flow_label)    : Identifies a flow (may be 0).
 *   16.bit  (payload_len)   : Length of payload + extension headers.
 *   8.bit   (next_header)   : Identifies next header (TCP=6, UDP=17,
 *                             ICMPv6=58, extension headers).
 *   8.bit   (hop_limit)     : Decremented by each forwarding node.
 *   128.bit (src_addr)      : Source IPv6 address.
 *   128.bit (dst_addr)      : Destination IPv6 address.
 *
 * Extension headers currently out of scope: Hop-by-Hop Options,
 * Destination Options, Routing, Fragment, Authentication, ESP.
 */
#ifndef __NG_IPV6_H__
#define __NG_IPV6_H__


#endif
