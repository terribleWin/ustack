/*
 * IEEE 802.3 / DIX Ethernet II - Ethernet Frame
 * 
 * The frame starts with the 6 bytes destination MAC and the 6 bytes source MAC,
 * followed by a 2 bytes EtherType which tells the upper layer protocol to use.
 * 
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                     Destination MAC (6 Bytes)                 +
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               |
 * +                        Source MAC (6 Bytes)                   +
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          EtherType            |                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
 * |                          Payload (46..1500 B)                 |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 
 * The minimum frame on the wire is 64 bytes including the 4 bytes FCS,
 * so a short payload is padded up to 46 bytes by the sender.
 * 
 * Field definitions:
 *  16.bit (ether_type) : 0x0800 IPv4, 0x0806 ARP, 0x86DD IPv6.
 *  48.bit (d_addr)     : Destination MAC. ff:ff:ff:ff:ff:ff for broadcast.
 *  48.bit (s_addr)     : Source MAC. Always the MAC address of the sender.
 *  nbytes (payload)    : Upper layer packet carried by this frame.
 */