/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include "esp_log.h"
#include "esp_check.h"
#include "clarke_park_private.h"

#define TAG "clarke_park_cordic"

#if CLARKE_PARK_WITH_CORDIC
#include "driver/cordic.h"

#define CLARKE_PARK_CORDIC_ITERATIONS 6
/* round(2^32 / π). θ_q15 * this >> 32 is (θ/π) in Q15. */
#define CLARKE_PARK_CORDIC_INV_PI_Q32 INT64_C(1367130551)

static cordic_engine_handle_t s_cordic_engine;

/*
 * IQmath Q15 stores radians as θ·2^15 (int32). CORDIC Cos takes θ/π as a
 * 16-bit Q15, wrapping every 2 (one full turn).
 */
static uint32_t clarke_park_cordic_arg_q15(_iq15 theta_rad)
{
    const int64_t half = INT64_C(1) << 31;
    int64_t prod = (int64_t)theta_rad * CLARKE_PARK_CORDIC_INV_PI_Q32;
    int64_t rounded = (prod + (prod < 0 ? -half : half)) >> 32;
    return (uint16_t)rounded;
}

void clarke_park_cordic_sincos_q15(_iq15 theta_rad, _iq15 *sin_theta, _iq15 *cos_theta)
{
    uint32_t arg = clarke_park_cordic_arg_q15(theta_rad);
    uint32_t res_cos;
    uint32_t res_sin;
    cordic_calculate_config_t calc_cfg = {
        .function = ESP_CORDIC_FUNC_COS,
        .iq_format = ESP_CORDIC_FORMAT_Q15,
        .iteration_count = CLARKE_PARK_CORDIC_ITERATIONS,
        .scale_exp = 0,
    };
    cordic_input_buffer_desc_t input = {
        .p_data_arg1 = &arg,
        .p_data_arg2 = NULL,
    };
    cordic_output_buffer_desc_t output = {
        .p_data_res1 = &res_cos,
        .p_data_res2 = &res_sin,
    };

    if (cordic_calculate_polling(s_cordic_engine, &calc_cfg, &input, &output, 1) == ESP_OK) {
        /* HAL already leaves a 16-bit Q15 in each word; int16_t sign-extends it into IQmath's int32 Q15. */
        *cos_theta = (int16_t)res_cos;
        *sin_theta = (int16_t)res_sin;
        return;
    }

    /* Fall back if the hardware calculation fails. */
    *sin_theta = CLARKE_PARK_IQ_SIN(15, theta_rad);
    *cos_theta = CLARKE_PARK_IQ_COS(15, theta_rad);
}

esp_err_t clarke_park_enable_cordic(void)
{
    if (s_cordic_engine == NULL) {
        cordic_engine_config_t engine_cfg = {
            .clock_source = CORDIC_CLK_SRC_DEFAULT,
        };
        ESP_RETURN_ON_ERROR(cordic_acquire_engine(&engine_cfg, &s_cordic_engine), TAG, "Failed to acquire CORDIC engine");
    }

    clarke_park_sincos_set_func(clarke_park_cordic_sincos_q15);
    return ESP_OK;
}

esp_err_t clarke_park_disable_cordic(void)
{
    clarke_park_sincos_set_func(NULL);
    if (s_cordic_engine != NULL) {
        cordic_release_engine(s_cordic_engine);
        s_cordic_engine = NULL;
    }
    return ESP_OK;
}

#else

esp_err_t clarke_park_enable_cordic(void)
{
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t clarke_park_disable_cordic(void)
{
    /* No engine, dispatch already on IQmath. Cleanup/reset must stay ESP_OK. */
    return ESP_OK;
}

#endif
