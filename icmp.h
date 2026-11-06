/*
 * RFC 792 - Internet Control Message Protocol
 * 
 * Although ICMP has many message types (Destination Unreachable, Redirect, etc.), 
 * their basic 4-byte headers are identical. The layouts diverge starting from 
 * the 5th byte depending on the Type field.
 * 
 * For this DPDK user-space stack, we ONLY implement Echo Request (Type 8) and 
 * Echo Reply (Type 0) currently. All other ICMP message types are considered out of scope 
 * and will be dropped.
 * 
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |      Type     |      Code     |           Checksum            |
 * |  (icmp_type)  |  (icmp_code)  |         (icmp_cksum)          |
 * |    (1 Byte)   |    (1 Byte)   |           (2 Bytes)           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Identifier           |        Sequence Number        |
 * |         (icmp_ident)          |         (icmp_seq_nb)         |
 * |           (2 Bytes)           |           (2 Bytes)           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                        Data (variable length)                 +
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 
 * Field definitions:
 *  8.bit  (icmp_type)   : Message type. 8 = Echo Request, 0 = Echo Reply.
 *  8.bit  (icmp_code)   : Code. Always 0 for Echo/Reply.
 *  16.bit (icmp_cksum)  : Checksum of the ENTIRE ICMP message (header + data).
 *  16.bit (icmp_ident)  : Identifier. Used by ping to match replies to requests.
 *  16.bit (icmp_seq_nb) : Sequence number. Incremented for each request.
 *  nbytes (data)        : Payload. Must be echoed back unchanged.
 */