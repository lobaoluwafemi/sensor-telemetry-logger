#include "detector.h"

/* Increments a counter, holding at the maximum instead of wrapping to zero. */
static uint8_t saturating_inc(uint8_t count)
{
    return (count < UINT8_MAX) ? (uint8_t)(count + 1u) : count;
}

static void update_range_fault(detector_t *d, bool in_range)
{
    if (in_range) {
        d->range_bad_count = 0;
        d->range_good_count = saturating_inc(d->range_good_count);
        if (d->range_good_count >= d->cfg.debounce_samples) {
            d->flags &= (uint8_t)~DETECTOR_FLAG_FAULT_RANGE;
        }
    } else {
        d->range_good_count = 0;
        d->range_bad_count = saturating_inc(d->range_bad_count);
        if (d->range_bad_count >= d->cfg.debounce_samples) {
            d->flags |= DETECTOR_FLAG_FAULT_RANGE;
        }
    }
}

static void update_stuck_fault(detector_t *d, uint16_t raw)
{
    if (d->cfg.stuck_samples == 0u) {
        return;
    }

    if (d->same_count != 0u && raw == d->last_raw) {
        if (d->same_count < UINT16_MAX) {
            d->same_count++;
        }
    } else {
        d->same_count = 1u;
        d->last_raw = raw;
        d->flags &= (uint8_t)~DETECTOR_FLAG_FAULT_STUCK;
    }

    if (d->same_count >= d->cfg.stuck_samples) {
        d->flags |= DETECTOR_FLAG_FAULT_STUCK;
    }
}

static void update_alert(detector_t *d, int16_t deci_c)
{
    if (deci_c >= d->cfg.alert_on_deci_c) {
        d->under_count = 0;
        d->over_count = saturating_inc(d->over_count);
        if (d->over_count >= d->cfg.debounce_samples) {
            d->flags |= DETECTOR_FLAG_ALERT;
        }
    } else if (deci_c <= d->cfg.alert_off_deci_c) {
        d->over_count = 0;
        d->under_count = saturating_inc(d->under_count);
        if (d->under_count >= d->cfg.debounce_samples) {
            d->flags &= (uint8_t)~DETECTOR_FLAG_ALERT;
        }
    } else {
        /* Inside the hysteresis band: keep the state, restart both counts. */
        d->over_count = 0;
        d->under_count = 0;
    }
}

void detector_init(detector_t *d, const detector_config_t *cfg)
{
    *d = (detector_t){.cfg = *cfg};
}

uint8_t detector_update(detector_t *d, uint16_t raw, int16_t deci_c)
{
    const bool in_range = (raw >= d->cfg.min_valid_raw) && (raw <= d->cfg.max_valid_raw);

    update_range_fault(d, in_range);
    update_stuck_fault(d, raw);

    /* An out-of-range sample says nothing about temperature, so the alert holds. */
    if (in_range) {
        update_alert(d, deci_c);
    }

    return d->flags;
}

bool detector_output_active(uint8_t flags)
{
    return flags != 0u;
}
