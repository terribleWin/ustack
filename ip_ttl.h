/*
 * RFC 791 - Internet Protocol (IPv4)
 *
 * Time to Live (TTL) is an 8-bit field that limits the lifetime of a
 * datagram in the network. Every router that forwards the datagram MUST
 * decrement the TTL by at least one. If the TTL reaches zero before the
 * datagram arrives at its destination, the router drops the datagram
 * and returns an ICMP Time Exceeded message (Type 11, Code 0) to the
 * source.
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Time to Live |    Protocol   |         Header Checksum       |
 * |    (8bit)     |    (8bit)     |            (16bit)            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Typical initial values:
 *   - 64  : common default on many systems.
 *   - 128 : Windows hosts.
 *   - 255 : many routers and Unix systems.
 *
 * When forwarding, the header checksum MUST be recomputed or adjusted
 * after decrementing TTL.
 */
#ifndef __NG_IP_TTL_H__
#define __NG_IP_TTL_H__


#endif
