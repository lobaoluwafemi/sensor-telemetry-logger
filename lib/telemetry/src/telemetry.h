/*
 * Telemetry line format, one sample per line:
 *
 *   $T,<seq>,<ms>,<raw>,<deci_c>,<flags>*<checksum>\n
 *
 * The checksum is the XOR of every byte between '$' and '*', written as two
 * uppercase hex digits (the same scheme as NMEA GPS sentences). The sequence
 * number lets the host spot dropped lines; the checksum lets it spot corrupt ones.
 */
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stddef.h>
#include <stdint.h>

/* Longest possible line, with every field at its widest, plus the terminator. */
#define TELEMETRY_MAX_LINE 48u

typedef struct {
    uint32_t seq;
    uint32_t timestamp_ms;
    uint16_t raw;
    int16_t deci_c;
    uint8_t flags;
} telemetry_sample_t;

/*
 * Writes one NUL-terminated line into buf. Returns the line length excluding the
 * NUL, or 0 if buf is too small (in which case the contents of buf are unusable).
 */
size_t telemetry_format(char *buf, size_t size, const telemetry_sample_t *sample);

#endif /* TELEMETRY_H */
