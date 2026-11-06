/*
 * Sensor telemetry logger for the ATmega328P (Arduino Uno).
 *
 * Timer1 triggers the ADC in hardware, so sampling has no software jitter. The
 * ADC interrupt runs detection and sets the alert pin straight away, which keeps
 * detection latency independent of serial traffic. Samples then pass through a
 * queue to the main loop, which does the slow work of formatting and sending them.
 *
 * Pins:
 *   A0 (PC0)  TMP36 output
 *   D8 (PB0)  alert output: high on alert or fault
 *   D9 (PB1)  timing probe: high while the ADC interrupt runs
 *   D1 (PD1)  UART TX, via the Uno's USB-serial chip
 */
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>
#include <util/delay.h>

#include "config.h"
#include "detector.h"
#include "telemetry.h"
#include "tmp36.h"
#include "uart.h"

#define ALERT_PIN PB0
#define PROBE_PIN PB1

#define SAMPLE_QUEUE_SIZE 16u /* must be a power of two */
#define SAMPLE_QUEUE_MASK (SAMPLE_QUEUE_SIZE - 1u)

static const detector_config_t DETECTOR_CONFIG = {
    .alert_on_deci_c = ALERT_ON_DECI_C,
    .alert_off_deci_c = ALERT_OFF_DECI_C,
    .min_valid_raw = MIN_VALID_RAW,
    .max_valid_raw = MAX_VALID_RAW,
    .debounce_samples = DEBOUNCE_SAMPLES,
    .stuck_samples = STUCK_SAMPLES,
};

static detector_t detector;

/* Written by the ADC interrupt, read by the main loop. */
static telemetry_sample_t sample_queue[SAMPLE_QUEUE_SIZE];
static volatile uint8_t queue_head;
static volatile uint8_t queue_tail;

static void gpio_init(void)
{
    DDRB |= _BV(ALERT_PIN) | _BV(PROBE_PIN);
}

static void adc_init(void)
{
    ADMUX = _BV(REFS1) | _BV(REFS0);  /* internal 1.1 V reference, channel ADC0 */
    DIDR0 = _BV(ADC0D);               /* disable the digital input buffer on A0 */
    ADCSRA = _BV(ADEN) | _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0); /* 16 MHz / 128 = 125 kHz */

    /* Give the reference time to settle, then discard the first conversion. */
    _delay_ms(10);
    ADCSRA |= _BV(ADSC);
    while (ADCSRA & _BV(ADSC)) {
    }

    ADCSRB = _BV(ADTS2) | _BV(ADTS0); /* auto-trigger source: Timer1 compare match B */
    ADCSRA |= _BV(ADATE) | _BV(ADIE);
}

static void timer1_init(void)
{
    OCR1A = TIMER1_TOP;
    OCR1B = TIMER1_TOP;
    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS11); /* CTC mode with TOP = OCR1A, clock / 8 */
}

ISR(ADC_vect)
{
    static uint32_t seq;

    PORTB |= _BV(PROBE_PIN);

    /* The trigger is edge-sensitive, and nothing else clears this compare flag. */
    TIFR1 = _BV(OCF1B);

    const uint16_t raw = ADC;
    const int16_t deci_c = tmp36_mv_to_deci_c(adc_raw_to_mv(raw, ADC_VREF_MV));
    const uint8_t flags = detector_update(&detector, raw, deci_c);

    if (detector_output_active(flags)) {
        PORTB |= _BV(ALERT_PIN);
    } else {
        PORTB &= (uint8_t)~_BV(ALERT_PIN);
    }

    /* If the queue is full this sample is dropped; the host sees a sequence gap. */
    const uint8_t head = queue_head;
    if ((uint8_t)(head - queue_tail) < SAMPLE_QUEUE_SIZE) {
        sample_queue[head & SAMPLE_QUEUE_MASK] = (telemetry_sample_t){
            .seq = seq,
            .timestamp_ms = seq * SAMPLE_PERIOD_MS,
            .raw = raw,
            .deci_c = deci_c,
            .flags = flags,
        };
        queue_head = (uint8_t)(head + 1u);
    }
    seq++;

    PORTB &= (uint8_t)~_BV(PROBE_PIN);
}

int main(void)
{
    static const char BOOT_MESSAGE[] = "# sensor-telemetry-logger boot\n";

    detector_init(&detector, &DETECTOR_CONFIG);
    gpio_init();
    uart_init();
    adc_init();
    timer1_init();
    sei();

    uart_write(BOOT_MESSAGE, sizeof(BOOT_MESSAGE) - 1u);

    for (;;) {
        const uint8_t tail = queue_tail;
        if (tail == queue_head) {
            continue;
        }

        char line[TELEMETRY_MAX_LINE];
        const size_t len =
            telemetry_format(line, sizeof(line), &sample_queue[tail & SAMPLE_QUEUE_MASK]);
        queue_tail = (uint8_t)(tail + 1u); /* the slot stays valid until this point */
        uart_write(line, len);
    }
}
