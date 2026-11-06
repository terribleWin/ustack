/*
 * RFC 9000 - QUIC: A UDP-Based Multiplexed and Secure Transport
 *
 * QUIC runs over UDP and provides a reliable, multiplexed, encrypted
 * transport in user space. It integrates TLS 1.3 for handshake and
 * record protection, eliminating head-of-line blocking at the transport
 * layer and supporting connection migration across network paths.
 *
 * QUIC Long Header (used during handshake and for retry):
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |1|1| T | Version | DCIL(4)| SCIL(4)| Destination Connection ID   |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                 Destination Connection ID (cont.)               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                   Source Connection ID (len SCIL)               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         Length (varint)                         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Packet Number (varint)                       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          Payload (*)                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * QUIC Short Header (used in 1-RTT packets):
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |0|1|S|R|R|K|P|P| Destination Connection ID (*)                 |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                         Packet Number                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                          Payload (*)                            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Core concepts:
 *   - Connection ID : identifies a connection independently of 4-tuple.
 *   - Streams       : unidirectional or bidirectional byte streams.
 *   - Frames        : the unit inside a QUIC packet (STREAM, ACK, MAX_DATA, etc.).
 *   - 0-RTT / 1-RTT : resumption and initial handshake modes.
 */
#ifndef __NG_QUIC_H__
#define __NG_QUIC_H__


#endif
