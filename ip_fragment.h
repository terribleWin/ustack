/*
 * RFC 791 - Internet Protocol (IPv4)
 *
 * When a datagram is larger than the MTU of the outgoing link, it may
 * be split into fragments. Each fragment carries the same Identification
 * field, a fragment offset measured in 8-byte units, and the More
 * Fragments (MF) flag.
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         Identification        |Flags|      Fragment Offset    |
 * |            (16bit)            | (3b)|         (13bit)         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Flags:
 *   - bit 0: reserved, must be zero.
 *   - bit 1: Don't Fragment (DF). If set and MTU is too small, send ICMP
 *            "Fragmentation Needed" and drop the packet.
 *   - bit 2: More Fragments (MF). Cleared on the last fragment.
 *
 * Reassembly:
 *   Fragments are queued by the 4-tuple (src_addr, dst_addr, protocol,
 *   identification). The first fragment has offset 0; the last has
 *   MF == 0. Overlapping fragments are discarded or trimmed per RFC 815.
 *   An incomplete datagram is timed out and its fragments discarded.
 */
#ifndef __NG_IP_FRAGMENT_H__
#define __NG_IP_FRAGMENT_H__


#endif
