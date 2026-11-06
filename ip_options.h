/*
 * RFC 791 - Internet Protocol (IPv4)
 *
 * IPv4 options follow the fixed 20-byte header when IHL > 5. Each
 * option begins with a type byte that encodes copied/option-class/number,
 * followed by a length byte for variable-length options.
 *
 * Option type byte:
 *   - bit 7     : copied flag (1 = copy into all fragments).
 *   - bits 6-5  : option class (0 = control, 2 = debugging/measurement).
 *   - bits 4-0  : option number.
 *
 * Common options:
 *   - End of Option List (EOL) : type 0, single byte.
 *   - No Operation (NOP)       : type 1, single byte, used for padding.
 *   - Record Route             : type 7, records visited routers.
 *   - Internet Timestamp       : type 68, records timestamps per hop.
 *   - Loose Source Route       : type 131, specifies a loose route.
 *   - Strict Source Route      : type 137, specifies an exact route.
 *
 * Options are processed in order; unknown options with the copied flag
 * set are copied into fragments, others are skipped. Options that
 * require strict handling but are not implemented cause the datagram to
 * be discarded and an ICMP Parameter Problem to be sent.
 */
#ifndef __NG_IP_OPTIONS_H__
#define __NG_IP_OPTIONS_H__


#endif
