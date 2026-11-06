/*
 * RFC 793 - Transmission Control Protocol
 * 
 * The TCP header is 20 bytes when no options are carried, that is Data Offset == 5.
 * 
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Source Port          |       Destination Port        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                        Sequence Number                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Acknowledgment Number                      |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Data |       |C|E|U|A|P|R|S|F|                               |
 * | Offset| Rsrvd |W|C|R|C|S|S|Y|I|            Window             |
 * |       |       |R|E|G|K|H|T|N|N|                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |           Checksum            |         Urgent Pointer        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                           [Options]                           |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                                                               :
 * :                             Data                              :
 * :                                                               |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * 
 * What we handle currently:
 *   - Three-way handshake: SYN -> SYN/ACK -> ACK
 *   - Data transfer (echo back what we receive)
 *   - Connection close from either side: FIN -> ACK -> FIN -> ACK
 * 
 * What we handle in the future:
 *   - Retransmits, sliding window beyond a fixed-size receive buffer,
 *     congestion control, Nagle, SACK, window scaling, fast retransmit,
 *     timestamps, MSS option negotiation (we use a fixed MSS), urgent data
 *   - Multiple simultaneous connections
 *   - Anything resembling reasonable timer behavior
 * 
 * State machine:
 *   LISTEN ─── recv SYN, send SYN/ACK ──> SYN_RECEIVED
 *   SYN_RECEIVED ── recv ACK ────────────> ESTABLISHED
 *   ESTABLISHED ── recv FIN, send ACK ──> CLOSE_WAIT
 *   CLOSE_WAIT  ── (we send FIN)        ─> LAST_ACK
 *   LAST_ACK    ── recv ACK              ─> CLOSED
 */