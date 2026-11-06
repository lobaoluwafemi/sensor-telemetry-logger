/* Build-time settings for the firmware. */
#ifndef CONFIG_H
#define CONFIG_H

/* Sampling. Timer1 runs at F_CPU / 8, so at 16 MHz it counts in 0.5 us steps. */
#define SAMPLE_RATE_HZ 100u
#define SAMPLE_PERIOD_MS (1000u / SAMPLE_RATE_HZ)
#define TIMER1_TOP ((F_CPU / 8u / SAMPLE_RATE_HZ) - 1u)

/*
 * ADC reference: the internal 1.1 V bandgap gives about 1.07 mV per code, or
 * 0.1 degC per code, compared with 0.5 degC per code using the 5 V supply.
 * The bandgap is only accurate to +/-10% between chips, so for accurate absolute
 * temperatures, measure the AREF pin with a multimeter and enter the value here.
 */
#define ADC_VREF_MV 1100u

/* Detection, in 0.1 degC units: 30.0 degC is reachable by holding the sensor. */
#define ALERT_ON_DECI_C 300
#define ALERT_OFF_DECI_C 290
#define DEBOUNCE_SAMPLES 3u

/*
 * Valid ADC codes. Below 93 (100 mV, the TMP36's -40 degC output) suggests a
 * shorted sensor; 1023 means the input is at or above the reference.
 */
#define MIN_VALID_RAW 93u
#define MAX_VALID_RAW 1022u

/* A live sensor shows some ADC noise, so 60 s with no change at all is suspect. */
#define STUCK_SAMPLES (60u * SAMPLE_RATE_HZ)

/* Serial link. 16 MHz with U2X gives 117647 baud, 2.1% fast and within tolerance. */
#define UART_UBRR_VALUE 16u

#endif /* CONFIG_H */
