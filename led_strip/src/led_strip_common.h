/*
 * SPDX-FileCopyrightText: 2022-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LED_STRIP_TRANS_IDLE,
    LED_STRIP_TRANS_INFLIGHT,
    LED_STRIP_TRANS_LOCKED,
} led_strip_trans_state_t;

typedef atomic_int led_strip_trans_state_atomic_t;

static inline bool led_strip_trans_state_try_set(led_strip_trans_state_atomic_t *state, led_strip_trans_state_t expected, led_strip_trans_state_t desired)
{
    int expected_state = expected;
    return atomic_compare_exchange_strong(state, &expected_state, desired);
}

#ifdef __cplusplus
}
#endif
