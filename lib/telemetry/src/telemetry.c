#include "telemetry.h"

#include <stdio.h>

/* "*XX\n" plus the terminating NUL. */
#define TRAILER_SIZE 5u

size_t telemetry_format(char *buf, size_t size, const telemetry_sample_t *sample)
{
    if (size <= TRAILER_SIZE) {
        return 0;
    }

    const int body_len = snprintf(buf, size - TRAILER_SIZE + 1u, "$T,%lu,%lu,%u,%d,%u",
                                  (unsigned long)sample->seq,
                                  (unsigned long)sample->timestamp_ms,
                                  (unsigned)sample->raw,
                                  (int)sample->deci_c,
                                  (unsigned)sample->flags);

    if (body_len < 0 || (size_t)body_len > size - TRAILER_SIZE) {
        return 0;
    }

    uint8_t checksum = 0;
    for (int i = 1; i < body_len; i++) { /* skips the leading '$' */
        checksum ^= (uint8_t)buf[i];
    }

    const int trailer_len = snprintf(buf + body_len, TRAILER_SIZE, "*%02X\n", (unsigned)checksum);

    return (size_t)(body_len + trailer_len);
}
