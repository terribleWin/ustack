/*
 * IEEE 802.1Q - Virtual Bridged Local Area Networks
 *
 * VLAN tags are inserted between the Ethernet source MAC and the
 * EtherType to isolate broadcast domains on a single physical link.
 *
 * Ethernet frame with 802.1Q tag:
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
 * |   TPID = 0x8100   | PCP |D|  VLAN ID (12 bits)               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          EtherType            |                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               |
 * |                          Payload                              |
 * |                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Field definitions:
 *   16.bit (TPID)    : Tag Protocol Identifier, always 0x8100 for 802.1Q.
 *   3.bit  (PCP)     : Priority Code Point, 802.1p priority.
 *   1.bit  (DEI/CFI) : Drop Eligible Indicator.
 *   12.bit (VID)     : VLAN Identifier, 1..4094 (0 and 4095 reserved).
 *
 * This module handles VLAN tag insertion, removal, and VID-based
 * interface/bridge selection.
 */
#ifndef __NG_VLAN_H__
#define __NG_VLAN_H__


#endif
