/*
 * RFC 2236 - Internet Group Management Protocol, Version 2
 * RFC 3376 - Internet Group Management Protocol, Version 3
 *
 * IGMP is used by IPv4 hosts and routers to manage multicast group
 * membership on a local link. It runs directly over IP (protocol 2)
 * and is analogous to ICMPv6 MLD.
 *
 * IGMPv2 message format:
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Type         |  Max Resp Time|           Checksum            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         Group Address                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Message types:
 *   - 0x11 : Membership Query (general or group-specific).
 *   - 0x16 : Membership Report v2.
 *   - 0x17 : Leave Group.
 *   - 0x12 : Membership Report v1 (for backward compatibility).
 *
 * This module handles IGMP join/leave state on local interfaces and
 * generates/report processes IGMP messages for IPv4 multicast groups.
 */
#ifndef __NG_IGMP_H__
#define __NG_IGMP_H__


#endif
