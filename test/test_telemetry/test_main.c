#include <stdint.h>
#include <string.h>
#include <unity.h>

#include "telemetry.h"

void setUp(void) {}
void tearDown(void) {}

/*
 * The same line is asserted in host/tests/test_telemetry_logger.py, which pins
 * the wire format: changing it on one side fails the other side's tests.
 */
static void test_matches_shared_golden_line(void)
{
    const telemetry_sample_t sample = {
        .seq = 42u, .timestamp_ms = 420u, .raw = 700u, .deci_c = 251, .flags = 1u};
    char buf[TELEMETRY_MAX_LINE];

    size_t len = telemetry_format(buf, sizeof(buf), &sample);

    TEST_ASSERT_EQUAL_STRING("$T,42,420,700,251,1*78\n", buf);
    TEST_ASSERT_EQUAL_size_t(strlen(buf), len);
}

static void test_negative_temperature(void)
{
    const telemetry_sample_t sample = {
        .seq = 0u, .timestamp_ms = 0u, .raw = 50u, .deci_c = -447, .flags = 2u};
    char buf[TELEMETRY_MAX_LINE];

    TEST_ASSERT_GREATER_THAN(0, telemetry_format(buf, sizeof(buf), &sample));
    TEST_ASSERT_NOT_NULL(strstr(buf, ",-447,"));
}

static void test_widest_values_fit_in_max_line(void)
{
    const telemetry_sample_t sample = {.seq = UINT32_MAX,
                                       .timestamp_ms = UINT32_MAX,
                                       .raw = UINT16_MAX,
                                       .deci_c = INT16_MIN,
                                       .flags = UINT8_MAX};
    char buf[TELEMETRY_MAX_LINE];

    size_t len = telemetry_format(buf, sizeof(buf), &sample);

    TEST_ASSERT_GREATER_THAN(0, len);
    TEST_ASSERT_LESS_THAN(TELEMETRY_MAX_LINE, len);
    TEST_ASSERT_EQUAL_CHAR('\n', buf[len - 1u]);
}

static void test_exact_size_buffer_succeeds(void)
{
    const telemetry_sample_t sample = {
        .seq = 42u, .timestamp_ms = 420u, .raw = 700u, .deci_c = 251, .flags = 1u};
    char buf[TELEMETRY_MAX_LINE];
    size_t len = telemetry_format(buf, sizeof(buf), &sample);

    char exact[TELEMETRY_MAX_LINE];
    TEST_ASSERT_EQUAL_size_t(len, telemetry_format(exact, len + 1u, &sample));
    TEST_ASSERT_EQUAL_STRING(buf, exact);
}

static void test_one_byte_short_buffer_returns_zero(void)
{
    const telemetry_sample_t sample = {
        .seq = 42u, .timestamp_ms = 420u, .raw = 700u, .deci_c = 251, .flags = 1u};
    char buf[TELEMETRY_MAX_LINE];
    size_t len = telemetry_format(buf, sizeof(buf), &sample);

    TEST_ASSERT_EQUAL_size_t(0, telemetry_format(buf, len, &sample));
}

static void test_tiny_buffer_returns_zero(void)
{
    const telemetry_sample_t sample = {0};
    char buf[5];

    TEST_ASSERT_EQUAL_size_t(0, telemetry_format(buf, sizeof(buf), &sample));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_matches_shared_golden_line);
    RUN_TEST(test_negative_temperature);
    RUN_TEST(test_widest_values_fit_in_max_line);
    RUN_TEST(test_exact_size_buffer_succeeds);
    RUN_TEST(test_one_byte_short_buffer_returns_zero);
    RUN_TEST(test_tiny_buffer_returns_zero);
    return UNITY_END();
}
