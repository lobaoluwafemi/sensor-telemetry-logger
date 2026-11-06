/*
 * Threshold and fault detection. Contains no hardware access, so the same code
 * runs in the firmware's ADC interrupt and in the native unit tests.
 */
#ifndef DETECTOR_H
#define DETECTOR_H

#include <stdbool.h>
#include <stdint.h>

/* Bits returned by detector_update() and sent in the telemetry flags field. */
#define DETECTOR_FLAG_ALERT 0x01u       /* temperature over threshold */
#define DETECTOR_FLAG_FAULT_RANGE 0x02u /* ADC reading outside the sensor's valid range */
#define DETECTOR_FLAG_FAULT_STUCK 0x04u /* ADC reading has not changed for too long */

typedef struct {
    int16_t alert_on_deci_c;  /* alert sets at or above this (0.1 degC units) */
    int16_t alert_off_deci_c; /* alert clears at or below this (hysteresis) */
    uint16_t min_valid_raw;   /* lowest ADC code treated as a plausible reading */
    uint16_t max_valid_raw;   /* highest ADC code treated as a plausible reading */
    uint8_t debounce_samples; /* consecutive samples needed to change a state */
    uint16_t stuck_samples;   /* identical samples before a stuck fault; 0 disables */
} detector_config_t;

typedef struct {
    detector_config_t cfg;
    uint8_t flags;
    uint8_t over_count;
    uint8_t under_count;
    uint8_t range_bad_count;
    uint8_t range_good_count;
    uint16_t last_raw;
    uint16_t same_count;
} detector_t;

void detector_init(detector_t *d, const detector_config_t *cfg);

/* Feeds one sample and returns the current DETECTOR_FLAG_* bits. */
uint8_t detector_update(detector_t *d, uint16_t raw, int16_t deci_c);

/*
 * True when the alert output should be driven. Faults also drive it, so a
 * broken sensor fails safe instead of silently reading as "no alert".
 */
bool detector_output_active(uint8_t flags);

#endif /* DETECTOR_H */
