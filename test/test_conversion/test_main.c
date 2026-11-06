#include <unity.h>

#include "tmp36.h"

void setUp(void) {}
void tearDown(void) {}

static void test_raw_zero_is_zero_mv(void)
{
    TEST_ASSERT_EQUAL_UINT16(0, adc_raw_to_mv(0, 1100));
}

static void test_full_scale_does_not_overflow(void)
{
    /* 1023 * 1100 / 1024 = 1098.9, truncated. A 16-bit intermediate would wrap. */
    TEST_ASSERT_EQUAL_UINT16(1098, adc_raw_to_mv(1023, 1100));
}

static void test_mid_scale(void)
{
    TEST_ASSERT_EQUAL_UINT16(550, adc_raw_to_mv(512, 1100));
}

static void test_tmp36_datasheet_points(void)
{
    TEST_ASSERT_EQUAL_INT16(0, tmp36_mv_to_deci_c(500));   /*   0.0 degC */
    TEST_ASSERT_EQUAL_INT16(250, tmp36_mv_to_deci_c(750)); /*  25.0 degC */
    TEST_ASSERT_EQUAL_INT16(-400, tmp36_mv_to_deci_c(100)); /* -40.0 degC */
}

static void test_tmp36_zero_mv_goes_negative(void)
{
    TEST_ASSERT_EQUAL_INT16(-500, tmp36_mv_to_deci_c(0));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_raw_zero_is_zero_mv);
    RUN_TEST(test_full_scale_does_not_overflow);
    RUN_TEST(test_mid_scale);
    RUN_TEST(test_tmp36_datasheet_points);
    RUN_TEST(test_tmp36_zero_mv_goes_negative);
    return UNITY_END();
}
