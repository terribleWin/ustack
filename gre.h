/*
 * RFC 2784 - Generic Routing Encapsulation (GRE)
 * RFC 2890 - Key and Sequence Number Extensions to GRE
 *
 * GRE is a simple, stateless tunneling protocol that encapsulates a wide
 * variety of network layer protocols inside IP packets. It uses IP
 * protocol number 47.
 *
 * GRE header (with optional Key field):
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |C|R|R|K|S|R|R|R|R|R|R|R|R|R|R|  Ver  |         Protocol Type   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |           Checksum (optional) |        Offset (optional)      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         Key (optional)                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Sequence Number (optional)                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Field definitions:
 *   1.bit  (C)         : Checksum Present.
 *   1.bit  (K)         : Key Present.
 *   1.bit  (S)         : Sequence Number Present.
 *   3.bit  (Ver)       : Version, 0 for GRE.
 *   16.bit (Protocol)  : EtherType of encapsulated payload (0x0800 IPv4, 0x86DD IPv6).
 *   32.bit (Key)       : Tunnel key used for demultiplexing.
 *
 * GRE is lighter than VXLAN (no UDP header) but lacks built-in
 * multipath/ECMP friendliness without additional keys.
 */
#ifndef __NG_GRE_H__
#define __NG_GRE_H__


#endif
