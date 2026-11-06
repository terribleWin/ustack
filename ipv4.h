/*
 * RFC 791 - Internet Protocol (IPv4)
 * 
 * The IPv4 header is 20 bytes when no options are carried, that is IHL == 5.
 * Total Length counts the header and the payload together.
 * 
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |Version|  IHL  |Type of Service|          Total Length         |
 * | (4bit)| (4bit)|     (8bit)    |            (16bit)            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         Identification        |Flags|      Fragment Offset    |
 * |            (16bit)            | (3b)|         (13bit)         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Time to Live |    Protocol   |         Header Checksum       |
 * |    (8bit)     |    (8bit)     |            (16bit)            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Source Address                          |
 * |                           (32bit)                             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Destination Address                        |
 * |                           (32bit)                             |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Options (variable length)                  |
 * |                           (variable)                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 
 * Only IHL == 5 is handled here, so no options are parsed and the header is
 * always 20 bytes. The header checksum covers the IPv4 header only, while the
 * TCP/UDP checksum covers the payload plus a pseudo header built from it.
 * 
 * Field definitions:
 *  4.bit  (version)         : Always 4.
 *  4.bit  (ihl)             : Header length in 4-byte words. 5 when no options.
 *  8.bit  (type_of_service) : Rarely used, set to 0 here.
 *  16.bit (total_length)    : IPv4 header + payload, counted in bytes.
 *  16.bit (packet_id)       : Used to reassemble the fragments.
 *  3.bit  (flags)           : Don't Fragment / More Fragments.
 *  13.bit (fragment_offset) : Offset of this fragment in 8-byte units.
 *  8.bit  (time_to_live)    : Decreased by 1 on each hop, dropped at 0.
 *  8.bit  (next_proto_id)   : 1 ICMP, 6 TCP, 17 UDP.
 *  16.bit (hdr_checksum)    : Checksum of the IPv4 header only.
 *  32.bit (src_addr)        : Source IP address.
 *  32.bit (dst_addr)        : Destination IP address.
 */