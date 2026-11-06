#include "tmp36.h"

#define TMP36_OFFSET_MV 500

uint16_t adc_raw_to_mv(uint16_t raw, uint16_t vref_mv)
{
    /* The 32-bit intermediate avoids overflow: 1023 * 1100 does not fit in 16 bits. */
    return (uint16_t)(((uint32_t)raw * vref_mv) >> 10);
}

int16_t tmp36_mv_to_deci_c(uint16_t mv)
{
    return (int16_t)((int16_t)mv - TMP36_OFFSET_MV);
}
