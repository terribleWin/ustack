/*
 * RFC 4302 - IP Authentication Header (AH)
 * RFC 4303 - IP Encapsulating Security Payload (ESP)
 *
 * IPsec provides confidentiality, integrity, and authentication at the
 * IP layer. It is used for VPNs and secure host-to-host communication.
 *
 * Security Associations (SAs):
 *   - An SA is a one-way logical connection identified by the SPI
 *     (Security Parameters Index), destination IP, and security protocol
 *     (AH or ESP).
 *   - The SAD (Security Association Database) stores active SAs.
 *   - The SPD (Security Policy Database) decides whether traffic must be
 *     bypassed, discarded, or protected.
 *
 * ESP header (simplified, transport mode):
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                 Security Parameters Index (SPI)               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                     Sequence Number                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Payload Data (variable)                    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Padding (0-255 bytes)                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Pad Length   | Next Header   |         Integrity Check Value   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Modes:
 *   - Transport mode : protects the payload, keeps original IP header.
 *   - Tunnel mode    : encapsulates the entire original IP packet.
 *
 * This header defines SA/SPD structures and the AH/ESP encapsulation
 * interface. Crypto operations are delegated to an external library.
 */
#ifndef __NG_IPSEC_H__
#define __NG_IPSEC_H__


#endif
