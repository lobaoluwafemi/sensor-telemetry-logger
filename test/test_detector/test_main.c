#include <unity.h>

#include "detector.h"

#define ON 300
#define OFF 290
#define COOL 250
#define HOT 320
#define BETWEEN 295
#define GOOD_RAW 700u
#define LOW_RAW 50u
#define SATURATED_RAW 1023u
#define DEBOUNCE 3u
#define STUCK_LIMIT 5u

static detector_t det;
static uint16_t next_raw;

/* A valid ADC code that differs each call, so tests don't trip the stuck fault. */
static uint16_t varying_raw(void)
{
    next_raw = (next_raw == GOOD_RAW) ? (uint16_t)(GOOD_RAW + 1u) : (uint16_t)GOOD_RAW;
    return next_raw;
}

static uint8_t feed(int16_t deci_c, unsigned times)
{
    uint8_t flags = 0;
    for (unsigned i = 0; i < times; i++) {
        flags = detector_update(&det, varying_raw(), deci_c);
    }
    return flags;
}

static uint8_t feed_raw(uint16_t raw, unsigned times)
{
    uint8_t flags = 0;
    for (unsigned i = 0; i < times; i++) {
        flags = detector_update(&det, raw, COOL);
    }
    return flags;
}

void setUp(void)
{
    const detector_config_t cfg = {
        .alert_on_deci_c = ON,
        .alert_off_deci_c = OFF,
        .min_valid_raw = 93u,
        .max_valid_raw = 1022u,
        .debounce_samples = DEBOUNCE,
        .stuck_samples = STUCK_LIMIT,
    };
    detector_init(&det, &cfg);
    next_raw = 0;
}

void tearDown(void) {}

static void test_starts_with_no_flags(void)
{
    TEST_ASSERT_EQUAL_HEX8(0, feed(COOL, 1));
}

static void test_alert_waits_for_debounce_count(void)
{
    TEST_ASSERT_EQUAL_HEX8(0, feed(HOT, DEBOUNCE - 1u));
    TEST_ASSERT_EQUAL_HEX8(DETECTOR_FLAG_ALERT, feed(HOT, 1));
}

static void test_alert_sets_exactly_at_threshold(void)
{
    TEST_ASSERT_EQUAL_HEX8(DETECTOR_FLAG_ALERT, feed(ON, DEBOUNCE));
}

static void test_one_below_threshold_does_not_alert(void)
{
    TEST_ASSERT_EQUAL_HEX8(0, feed(ON - 1, DEBOUNCE * 2u));
}

static void test_single_spike_is_rejected(void)
{
    feed(HOT, DEBOUNCE - 1u);
    feed(COOL, 1);
    TEST_ASSERT_EQUAL_HEX8(0, feed(HOT, DEBOUNCE - 1u));
}

static void test_alert_holds_inside_hysteresis_band(void)
{
    feed(HOT, DEBOUNCE);
    TEST_ASSERT_EQUAL_HEX8(DETECTOR_FLAG_ALERT, feed(BETWEEN, 50));
}

static void test_alert_clears_at_off_threshold_after_debounce(void)
{
    feed(HOT, DEBOUNCE);
    TEST_ASSERT_EQUAL_HEX8(DETECTOR_FLAG_ALERT, feed(OFF, DEBOUNCE - 1u));
    TEST_ASSERT_EQUAL_HEX8(0, feed(OFF, 1));
}

static void test_low_reading_sets_range_fault_after_debounce(void)
{
    TEST_ASSERT_EQUAL_HEX8(0, feed_raw(LOW_RAW, DEBOUNCE - 1u));
    TEST_ASSERT_EQUAL_HEX8(DETECTOR_FLAG_FAULT_RANGE, feed_raw(LOW_RAW, 1));
}

static void test_saturated_reading_sets_range_fault(void)
{
    uint8_t flags = feed_raw(SATURATED_RAW, DEBOUNCE);
    TEST_ASSERT_BITS_HIGH(DETECTOR_FLAG_FAULT_RANGE, flags);
}

static void test_valid_range_limits_are_inclusive(void)
{
    uint8_t flags = 0;
    for (unsigned i = 0; i < DEBOUNCE * 2u; i++) {
        /* Alternating codes stop the stuck check from firing. */
        flags = detector_update(&det, (i % 2u) ? 93u : 1022u, COOL);
    }
    TEST_ASSERT_EQUAL_HEX8(0, flags);
}

static void test_range_fault_clears_after_debounce(void)
{
    feed_raw(LOW_RAW, DEBOUNCE);
    TEST_ASSERT_BITS_HIGH(DETECTOR_FLAG_FAULT_RANGE, feed(COOL, DEBOUNCE - 1u));
    TEST_ASSERT_BITS_LOW(DETECTOR_FLAG_FAULT_RANGE, feed(COOL, 1));
}

static void test_range_fault_does_not_clear_active_alert(void)
{
    feed(HOT, DEBOUNCE);
    uint8_t flags = feed_raw(LOW_RAW, DEBOUNCE);
    TEST_ASSERT_BITS_HIGH(DETECTOR_FLAG_ALERT, flags);
    TEST_ASSERT_BITS_HIGH(DETECTOR_FLAG_FAULT_RANGE, flags);
}

static void test_out_of_range_samples_cannot_raise_alert(void)
{
    uint8_t flags = 0;
    for (unsigned i = 0; i < DEBOUNCE * 2u; i++) {
        flags = detector_update(&det, SATURATED_RAW, HOT);
    }
    TEST_ASSERT_BITS_LOW(DETECTOR_FLAG_ALERT, flags);
}

static void test_stuck_fault_sets_at_limit(void)
{
    TEST_ASSERT_BITS_LOW(DETECTOR_FLAG_FAULT_STUCK, feed_raw(GOOD_RAW, STUCK_LIMIT - 1u));
    TEST_ASSERT_BITS_HIGH(DETECTOR_FLAG_FAULT_STUCK, feed_raw(GOOD_RAW, 1));
}

static void test_stuck_fault_clears_on_change(void)
{
    feed_raw(GOOD_RAW, STUCK_LIMIT);
    TEST_ASSERT_BITS_LOW(DETECTOR_FLAG_FAULT_STUCK, feed_raw(GOOD_RAW + 1u, 1));
}

static void test_first_sample_of_zero_is_not_counted_as_repeat(void)
{
    /* last_raw starts at 0, so a first reading of 0 must not look like a repeat. */
    uint8_t flags = 0;
    for (unsigned i = 0; i < STUCK_LIMIT - 1u; i++) {
        flags = detector_update(&det, 0u, COOL);
    }
    TEST_ASSERT_BITS_LOW(DETECTOR_FLAG_FAULT_STUCK, flags);
}

static void test_stuck_check_can_be_disabled(void)
{
    const detector_config_t cfg = {
        .alert_on_deci_c = ON,
        .alert_off_deci_c = OFF,
        .min_valid_raw = 93u,
        .max_valid_raw = 1022u,
        .debounce_samples = DEBOUNCE,
        .stuck_samples = 0u,
    };
    detector_init(&det, &cfg);
    TEST_ASSERT_BITS_LOW(DETECTOR_FLAG_FAULT_STUCK, feed_raw(GOOD_RAW, 1000u));
}

static void test_debounce_counter_does_not_wrap(void)
{
    /* 300 samples overflow a uint8_t count; the alert must stay set throughout. */
    TEST_ASSERT_EQUAL_HEX8(DETECTOR_FLAG_ALERT, feed(HOT, 300u));
}

static void test_output_is_active_for_any_flag(void)
{
    TEST_ASSERT_FALSE(detector_output_active(0));
    TEST_ASSERT_TRUE(detector_output_active(DETECTOR_FLAG_ALERT));
    TEST_ASSERT_TRUE(detector_output_active(DETECTOR_FLAG_FAULT_RANGE));
    TEST_ASSERT_TRUE(detector_output_active(DETECTOR_FLAG_FAULT_STUCK));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_with_no_flags);
    RUN_TEST(test_alert_waits_for_debounce_count);
    RUN_TEST(test_alert_sets_exactly_at_threshold);
    RUN_TEST(test_one_below_threshold_does_not_alert);
    RUN_TEST(test_single_spike_is_rejected);
    RUN_TEST(test_alert_holds_inside_hysteresis_band);
    RUN_TEST(test_alert_clears_at_off_threshold_after_debounce);
    RUN_TEST(test_low_reading_sets_range_fault_after_debounce);
    RUN_TEST(test_saturated_reading_sets_range_fault);
    RUN_TEST(test_valid_range_limits_are_inclusive);
    RUN_TEST(test_range_fault_clears_after_debounce);
    RUN_TEST(test_range_fault_does_not_clear_active_alert);
    RUN_TEST(test_out_of_range_samples_cannot_raise_alert);
    RUN_TEST(test_stuck_fault_sets_at_limit);
    RUN_TEST(test_stuck_fault_clears_on_change);
    RUN_TEST(test_first_sample_of_zero_is_not_counted_as_repeat);
    RUN_TEST(test_stuck_check_can_be_disabled);
    RUN_TEST(test_debounce_counter_does_not_wrap);
    RUN_TEST(test_output_is_active_for_any_flag);
    return UNITY_END();
}
