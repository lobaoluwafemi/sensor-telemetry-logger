/*
 * TMP36 conversion using integer maths only, because the ATmega328P has no FPU.
 * The sensor outputs 500 mV at 0 degC and 10 mV per degC, so one millivolt above
 * the offset is exactly 0.1 degC.
 */
#ifndef TMP36_H
#define TMP36_H

#include <stdint.h>

/* Converts a 10-bit ADC code to millivolts for the given reference voltage. */
uint16_t adc_raw_to_mv(uint16_t raw, uint16_t vref_mv);

/* Converts TMP36 output millivolts to tenths of a degree Celsius. */
int16_t tmp36_mv_to_deci_c(uint16_t mv);

#endif /* TMP36_H */
