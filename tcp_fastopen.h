/*
 * RFC 7413 - TCP Fast Open
 *
 * TCP Fast Open (TFO) allows data to be carried in the SYN and SYN-ACK
 * packets, reducing connection establishment latency by one RTT.
 *
 * Operation:
 *   - Initial handshake : client requests a TFO cookie with a SYN containing
 *                         the TCP Fast Open option.
 *   - Server            : generates and returns a cookie in the SYN-ACK.
 *   - Subsequent connects : client includes the cached cookie and application
 *                           data in the SYN. The server validates the cookie
 *                           and may accept the data immediately.
 *
 * TCP Fast Open option format:
 *
 *    0                   1                   2                   3
 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   | Kind=34       | Length        |                               |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                               +
 *   |           TFO Cookie (0 or more bytes, up to 16)              |
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * State:
 *   - Per-listener cookie key (rotated periodically for security).
 *   - Per-client cookie cache (server IP + port -> cookie).
 *   - SYN-data queue on the server until the final ACK validates it.
 */
#ifndef __NG_TCP_FASTOPEN_H__
#define __NG_TCP_FASTOPEN_H__


#endif
