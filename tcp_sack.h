/*
 * RFC 2018 - TCP Selective Acknowledgment Options
 * RFC 2883 - An Extension to the SACK Option for SACK
 *
 * SACK allows the receiver to inform the sender about non-contiguous
 * blocks of received data, so the sender only retransmits the missing
 * segments instead of everything after the first gap.
 *
 * The SACK-Permitted option is negotiated in SYN/SYN-ACK:
 *
 *    0                   1
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   | Kind = 4      | Length = 2    |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * The SACK option carries up to 4 blocks in a single segment:
 *
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   | Kind = 5      | Length        | Left Edge of 1st Block        |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   | Left Edge of 1st Block (cont.)| Right Edge of 1st Block       |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   | Right Edge of 1st Block (cont.)| Left Edge of 2nd Block       |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   | Right Edge of 2nd Block       | ...                           |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Each edge is a 32-bit sequence number. A SACK block [Left, Right)
 * indicates bytes up to but not including Right have been received.
 *
 * DSACK (Duplicate SACK)
 * ----------------------
 * The first SACK block in a DSACK option reports a duplicate segment,
 * i.e., a block that the receiver has already acknowledged. DSACK helps
 * the sender detect spurious retransmissions and unnecessary fast
 * retransmits, enabling algorithms such as F-RTO and Eifel.
 *
 * Field definitions:
 *   8.bit  (kind)   : 4 for SACK-Permitted, 5 for SACK/DSACK.
 *   8.bit  (length) : Option length in bytes (2 for Permitted, 10 + 8*N for SACK).
 *   32.bit (left)   : First sequence number of a received block.
 *   32.bit (right)  : Sequence number immediately after the block.
 */
#ifndef __NG_TCP_SACK_H__
#define __NG_TCP_SACK_H__


#endif
