/*
 * RFC 7348 - Virtual eXtensible Local Area Network (VXLAN)
 *
 * VXLAN is a Layer 2 overlay scheme over a Layer 3 network. It encapsulates
 * Ethernet frames inside UDP packets (destination port 4789) and uses a
 * 24-bit VNI (VXLAN Network Identifier) to isolate tenants.
 *
 * VXLAN header (8 bytes) after the UDP header:
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |R|R|R|R|I|R|R|R|          Reserved             |    VNI (24)   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          VNI (cont.)          |    Reserved   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Encapsulation:
 *   Outer Ethernet + Outer IP + Outer UDP/4789 + VXLAN Header + Inner Ethernet Frame
 *
 * Use cases:
 *   - Data center overlay networks.
 *   - Multi-tenant virtual switching.
 *   - Extending L2 segments across L3 underlay.
 *
 * This module handles VXLAN encapsulation/decapsulation and VNI-based
 * forwarding/learning.
 */
#ifndef __NG_VXLAN_H__
#define __NG_VXLAN_H__


#endif
