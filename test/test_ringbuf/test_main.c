#include <unity.h>

#include "ringbuf.h"

#define CAPACITY 8u

static uint8_t storage[CAPACITY];
static ringbuf_t rb;

void setUp(void)
{
    ringbuf_init(&rb, storage, CAPACITY);
}

void tearDown(void) {}

static void test_get_on_empty_returns_false(void)
{
    uint8_t byte = 0xAA;
    TEST_ASSERT_FALSE(ringbuf_get(&rb, &byte));
    TEST_ASSERT_EQUAL_HEX8(0xAA, byte);
}

static void test_bytes_come_out_in_order(void)
{
    uint8_t byte;
    ringbuf_put(&rb, 1);
    ringbuf_put(&rb, 2);

    TEST_ASSERT_TRUE(ringbuf_get(&rb, &byte));
    TEST_ASSERT_EQUAL_UINT8(1, byte);
    TEST_ASSERT_TRUE(ringbuf_get(&rb, &byte));
    TEST_ASSERT_EQUAL_UINT8(2, byte);
    TEST_ASSERT_FALSE(ringbuf_get(&rb, &byte));
}

static void test_holds_full_capacity_then_rejects(void)
{
    for (uint8_t i = 0; i < CAPACITY; i++) {
        TEST_ASSERT_TRUE(ringbuf_put(&rb, i));
    }
    TEST_ASSERT_FALSE(ringbuf_put(&rb, 0xFF));

    /* The rejected byte must not have overwritten the oldest one. */
    uint8_t byte;
    TEST_ASSERT_TRUE(ringbuf_get(&rb, &byte));
    TEST_ASSERT_EQUAL_UINT8(0, byte);
}

static void test_survives_index_rollover(void)
{
    /* 1000 bytes rolls the 8-bit indices over several times. */
    for (unsigned i = 0; i < 1000u; i++) {
        uint8_t out;
        TEST_ASSERT_TRUE(ringbuf_put(&rb, (uint8_t)i));
        TEST_ASSERT_TRUE(ringbuf_get(&rb, &out));
        TEST_ASSERT_EQUAL_UINT8((uint8_t)i, out);
    }
}

static void test_full_check_works_across_rollover(void)
{
    uint8_t out;
    for (unsigned i = 0; i < 253u; i++) {
        ringbuf_put(&rb, 0);
        ringbuf_get(&rb, &out);
    }
    for (uint8_t i = 0; i < CAPACITY; i++) {
        TEST_ASSERT_TRUE(ringbuf_put(&rb, i));
    }
    TEST_ASSERT_FALSE(ringbuf_put(&rb, 0xFF));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_get_on_empty_returns_false);
    RUN_TEST(test_bytes_come_out_in_order);
    RUN_TEST(test_holds_full_capacity_then_rejects);
    RUN_TEST(test_survives_index_rollover);
    RUN_TEST(test_full_check_works_across_rollover);
    return UNITY_END();
}
