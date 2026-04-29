/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "unity.h"
#include "pid_ctrl.h"
#include "IQmathLib.h"

TEST_CASE("float positional PID (explicit _f API)", "[pid_ctrl]")
{
    pid_ctrl_config_f_t cfg = {
        .init_param = {
            .kp = 1.0f,
            .ki = 0.0f,
            .kd = 0.0f,
            .max_output = 100.0f,
            .min_output = -100.0f,
            .max_integral = 100.0f,
            .min_integral = -100.0f,
            .cal_type = PID_CAL_TYPE_POSITIONAL,
        },
    };
    pid_ctrl_block_f_handle_t h = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, pid_new_control_block_f(&cfg, &h));
    float out = 0.0f;
    TEST_ASSERT_EQUAL(ESP_OK, pid_compute_f(h, 0.1f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.1f, out);
    TEST_ASSERT_EQUAL(ESP_OK, pid_del_control_block_f(h));
}

TEST_CASE("float incremental PID (explicit _f API)", "[pid_ctrl]")
{
    /*
     * du = (e-e1)*Kp + (e-2e1+e2)*Kd + e*Ki + u_{k-1}; first: e1=e2=0, u=0, e=0.1
     *    = 0.1*2 + 0.1*0.5 + 0.1*1 = 0.2 + 0.05 + 0.1 = 0.35
     */
    pid_ctrl_config_f_t cfg = {
        .init_param = {
            .kp = 2.0f,
            .ki = 1.0f,
            .kd = 0.5f,
            .max_output = 100.0f,
            .min_output = -100.0f,
            .max_integral = 100.0f,
            .min_integral = -100.0f,
            .cal_type = PID_CAL_TYPE_INCREMENTAL,
        },
    };
    pid_ctrl_block_f_handle_t h = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, pid_new_control_block_f(&cfg, &h));
    float out = 0.0f;
    TEST_ASSERT_EQUAL(ESP_OK, pid_compute_f(h, 0.1f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.35f, out);
    TEST_ASSERT_EQUAL(ESP_OK, pid_del_control_block_f(h));
}

TEST_CASE("IQ positional PID (explicit _iq API)", "[pid_ctrl][iq]")
{
    pid_ctrl_config_iq_t cfg = {
        .init_param = {
            .kp = _IQ(1.0f),
            .ki = _IQ(0.0f),
            .kd = _IQ(0.0f),
            .max_output = _IQ(100.0f),
            .min_output = _IQ(-100.0f),
            .max_integral = _IQ(100.0f),
            .min_integral = _IQ(-100.0f),
            .cal_type = PID_CAL_TYPE_POSITIONAL,
        },
    };
    pid_ctrl_block_iq_handle_t h = NULL;
    TEST_ASSERT_EQUAL(ESP_OK, pid_new_control_block_iq(&cfg, &h));
    _iq out = 0;
    TEST_ASSERT_EQUAL(ESP_OK, pid_compute_iq(h, _IQ(0.1f), &out));
    TEST_ASSERT_FLOAT_WITHIN(2e-3f, 0.1f, _IQtoF(out));
    TEST_ASSERT_EQUAL(ESP_OK, pid_del_control_block_iq(h));
}

TEST_CASE("C _Generic: unsuffixed API with float and IQ handles", "[pid_ctrl][iq][generic]")
{
    {
        pid_ctrl_config_t cfg = {
            .init_param = {
                .kp = 1.0f,
                .ki = 0.0f,
                .kd = 0.0f,
                .max_output = 100.0f,
                .min_output = -100.0f,
                .max_integral = 100.0f,
                .min_integral = -100.0f,
                .cal_type = PID_CAL_TYPE_POSITIONAL,
            },
        };
        pid_ctrl_block_handle_t h = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, pid_new_control_block(&cfg, &h));
        float out = 0.0f;
        TEST_ASSERT_EQUAL(ESP_OK, pid_compute(h, 0.1f, &out));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.1f, out);
        TEST_ASSERT_EQUAL(ESP_OK, pid_del_control_block(h));
    }
    {
        pid_ctrl_config_iq_t cfg = {
            .init_param = {
                .kp = _IQ(1.0f),
                .ki = _IQ(0.0f),
                .kd = _IQ(0.0f),
                .max_output = _IQ(100.0f),
                .min_output = _IQ(-100.0f),
                .max_integral = _IQ(100.0f),
                .min_integral = _IQ(-100.0f),
                .cal_type = PID_CAL_TYPE_POSITIONAL,
            },
        };
        pid_ctrl_block_iq_handle_t h = NULL;
        TEST_ASSERT_EQUAL(ESP_OK, pid_new_control_block(&cfg, &h));
        _iq out = 0;
        TEST_ASSERT_EQUAL(ESP_OK, pid_compute(h, _IQ(0.1f), &out));
        TEST_ASSERT_FLOAT_WITHIN(2e-3f, 0.1f, _IQtoF(out));
        TEST_ASSERT_EQUAL(ESP_OK, pid_del_control_block(h));
    }
}
