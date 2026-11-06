/*
 * RFC 768 - User Datagram Protocol
 * 
 * UDP is a connectionless and unreliable transport protocol. The header is
 * only 8 bytes long and is followed by the payload.
 * 
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Source Port          |       Destination Port        |
 * |          (src_port)           |          (dst_port)           |
 * |           (2 Bytes)           |           (2 Bytes)           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |            Length             |           Checksum            |
 * |          (dgram_len)          |         (dgram_cksum)         |
 * |           (2 Bytes)           |           (2 Bytes)           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                     Data (variable length)                    +
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 
 * The checksum is not computed over the UDP header alone. A pseudo header
 * built from the IPv4 header is prepended, so that a packet delivered to
 * the wrong address or protocol is detected by the receiver.
 * 
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Source Address                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     Destination Address                       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |     Zero      |   Protocol    |          UDP Length           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 
 * In this stack only the ports and the payload are parsed, and an echo
 * packet is built with the source and destination fields swapped.
 * 
 * Field definitions:
 *  16.bit (src_port)    : Source port.
 *  16.bit (dst_port)    : Destination port.
 *  16.bit (dgram_len)   : UDP header + data, in bytes. The minimum is 8.
 *  16.bit (dgram_cksum) : Checksum of the pseudo header + UDP header + data.
 *                         0 means the sender did not compute it.
 *  nbytes (data)        : Payload carried by this datagram.
 */
