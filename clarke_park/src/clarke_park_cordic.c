/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <math.h>
#include <sys/lock.h>
#include "clarke_park.h"

/* Number of CORDIC iterations. The hardware precision field is 4 bits wide and
 * the driver writes `iteration_count - 1` into it, so the valid range is 1..15
 * (CORDIC_LL_PRECISION_MAX == 0xF, checked by cordic_calculate_polling()).
 * 15 iterations give a residual of 2^-14 for Q15 sin/cos, which is the best
 * the hardware can do - and already below the Q15 quantization step. */
#define CLARKE_PARK_CORDIC_ITERATIONS 15

/* Guard against an out-of-range value: cordic_calculate_polling() rejects it
 * with ESP_ERR_INVALID_ARG at runtime, which would silently disable the
 * hardware acceleration. */
#if CLARKE_PARK_CORDIC_ITERATIONS < 1 || CLARKE_PARK_CORDIC_ITERATIONS > 15
#error "CLARKE_PARK_CORDIC_ITERATIONS must be in the range 1..15 (4-bit CORDIC precision field)"
#endif

/* The CORDIC peripheral has only one hardware unit, and the driver allows only
 * one engine to be created at a time (a second `cordic_new_engine()` call
 * returns ESP_ERR_NOT_FOUND). The clarke_park API is stateless (no handle), so
 * the engine handle has to live somewhere global. It is kept here as a
 * process-wide singleton. Other parts of the application can share the CORDIC
 * unit through `clarke_park_cordic_get_engine()`.
 *
 * Thread-safety: `cordic_calculate_polling()` writes the arguments to the
 * hardware registers and polls for the result without any internal locking, so
 * concurrent calls would corrupt each other's results. A single lock therefore
 * protects both the engine lifecycle (create/delete) and every calculation.
 * As a consequence the Park transforms are thread-safe, but they must not be
 * called from an ISR while the lock is taken from a task (the lock is not
 * ISR safe): either do all Park transforms from task context only, or make
 * sure no task holds the CORDIC engine when the ISR runs. */
static cordic_engine_handle_t s_cordic_engine = NULL;
static _lock_t s_cordic_lock;

/* Caller must hold s_cordic_lock. */
static esp_err_t clarke_park_cordic_ensure_engine_locked(void)
{
    esp_err_t ret = ESP_OK;
    if (s_cordic_engine == NULL) {
        cordic_engine_config_t engine_cfg = {
            .clock_source = CORDIC_CLK_SRC_DEFAULT,
        };
        ret = cordic_new_engine(&engine_cfg, &s_cordic_engine);
    }
    return ret;
}

esp_err_t clarke_park_cordic_init(void)
{
    esp_err_t ret;
    _lock_acquire(&s_cordic_lock);
    ret = clarke_park_cordic_ensure_engine_locked();
    _lock_release(&s_cordic_lock);
    return ret;
}

esp_err_t clarke_park_cordic_deinit(void)
{
    esp_err_t ret = ESP_OK;
    _lock_acquire(&s_cordic_lock);
    if (s_cordic_engine != NULL) {
        ret = cordic_delete_engine(s_cordic_engine);
        s_cordic_engine = NULL;
    }
    _lock_release(&s_cordic_lock);
    return ret;
}

cordic_engine_handle_t clarke_park_cordic_get_engine(void)
{
    cordic_engine_handle_t engine;
    _lock_acquire(&s_cordic_lock);
    engine = s_cordic_engine;
    _lock_release(&s_cordic_lock);
    return engine;
}

#if CLARKE_PARK_GLOBAL_IQ == 15
/**
 * @brief Compute sin(theta) and cos(theta) with the CORDIC accelerator
 *
 * The CORDIC hardware works in Q15 and expects the angle normalized to
 * [-1, 1) i.e. angle_in_radians / pi. A Q15 angle is always within [-1, 1)
 * radians, so no additional range reduction is needed.
 *
 * @param[in] theta_rad Angle in radians, Q15
 * @param[out] sin_theta Sine of the angle, Q15
 * @param[out] cos_theta Cosine of the angle, Q15
 */
static void clarke_park_cordic_sincos_q15(_iq15 theta_rad, _iq15 *sin_theta, _iq15 *cos_theta)
{
    int16_t arg_q15 = (int16_t)_IQ15mpy(theta_rad, _IQ15(1.0f / (float)M_PI));
    uint32_t arg = (uint32_t)(uint16_t)arg_q15;
    uint32_t res1 = 0;
    uint32_t res2 = 0;

    _lock_acquire(&s_cordic_lock);
    if (clarke_park_cordic_ensure_engine_locked() == ESP_OK) {
        cordic_calculate_config_t calc_cfg = {
            .function = ESP_CORDIC_FUNC_COS,   /* res1 = cos(theta), res2 = sin(theta) */
            .iq_format = ESP_CORDIC_FORMAT_Q15,
            .iteration_count = CLARKE_PARK_CORDIC_ITERATIONS,
            .scale_exp = 0,
        };
        cordic_input_buffer_desc_t input = {
            .p_data_arg1 = &arg,
            .p_data_arg2 = NULL,
        };
        cordic_output_buffer_desc_t output = {
            .p_data_res1 = &res1,
            .p_data_res2 = &res2,
        };

        if (cordic_calculate_polling(s_cordic_engine, &calc_cfg, &input, &output, 1) == ESP_OK) {
            *cos_theta = (int16_t)(res1 & 0xFFFF);
            *sin_theta = (int16_t)(res2 & 0xFFFF);
            _lock_release(&s_cordic_lock);
            return;
        }
    }
    _lock_release(&s_cordic_lock);

    /* The CORDIC unit is already occupied by someone else or the calculation
     * failed: fall back to the software IQmath implementation. */
    *sin_theta = _IQ15sin(theta_rad);
    *cos_theta = _IQ15cos(theta_rad);
}
#endif // CLARKE_PARK_GLOBAL_IQ == 15

void clarke_park_park_iq(_iq theta_rad, const clarke_park_ab_iq_t *ab, clarke_park_dq_iq_t *dq)
{
#if CLARKE_PARK_GLOBAL_IQ == 15
    _iq15 sin_theta;
    _iq15 cos_theta;
    clarke_park_cordic_sincos_q15((_iq15)theta_rad, &sin_theta, &cos_theta);

    dq->d = _IQ15mpy((_iq15)ab->alpha, cos_theta) + _IQ15mpy((_iq15)ab->beta, sin_theta);
    dq->q = -_IQ15mpy((_iq15)ab->alpha, sin_theta) + _IQ15mpy((_iq15)ab->beta, cos_theta);
#else
    /* The CORDIC accelerator only supports Q15 (and Q31, which is not
     * supported by IQmath). All other fixed-point formats fall back to the
     * software IQmath implementation. */
    _iq sin_theta = _IQsin(theta_rad);
    _iq cos_theta = _IQcos(theta_rad);

    dq->d = _IQmpy(ab->alpha, cos_theta) + _IQmpy(ab->beta, sin_theta);
    dq->q = -_IQmpy(ab->alpha, sin_theta) + _IQmpy(ab->beta, cos_theta);
#endif
}

void clarke_park_ipark_iq(_iq theta_rad, const clarke_park_dq_iq_t *dq, clarke_park_ab_iq_t *ab)
{
#if CLARKE_PARK_GLOBAL_IQ == 15
    _iq15 sin_theta;
    _iq15 cos_theta;
    clarke_park_cordic_sincos_q15((_iq15)theta_rad, &sin_theta, &cos_theta);

    ab->alpha = _IQ15mpy((_iq15)dq->d, cos_theta) - _IQ15mpy((_iq15)dq->q, sin_theta);
    ab->beta = _IQ15mpy((_iq15)dq->d, sin_theta) + _IQ15mpy((_iq15)dq->q, cos_theta);
#else
    _iq sin_theta = _IQsin(theta_rad);
    _iq cos_theta = _IQcos(theta_rad);

    ab->alpha = _IQmpy(dq->d, cos_theta) - _IQmpy(dq->q, sin_theta);
    ab->beta = _IQmpy(dq->d, sin_theta) + _IQmpy(dq->q, cos_theta);
#endif
}
