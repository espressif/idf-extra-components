/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "unity.h"
#include "pid_ctrl.h"
#include "IQmathLib.h"

static void make_pos_f_cfg(pid_ctrl_config_f_t *cfg,
                           float kp, float ki, float kd,
                           float max_out, float min_out,
                           float max_int, float min_int)
{
    cfg->init_param.kp = kp;
    cfg->init_param.ki = ki;
    cfg->init_param.kd = kd;
    cfg->init_param.max_output = max_out;
    cfg->init_param.min_output = min_out;
    cfg->init_param.max_integral = max_int;
    cfg->init_param.min_integral = min_int;
    cfg->init_param.cal_type = PID_CAL_TYPE_POSITIONAL;
}

static void make_inc_f_cfg(pid_ctrl_config_f_t *cfg,
                           float kp, float ki, float kd,
                           float max_out, float min_out,
                           float max_int, float min_int)
{
    cfg->init_param.kp = kp;
    cfg->init_param.ki = ki;
    cfg->init_param.kd = kd;
    cfg->init_param.max_output = max_out;
    cfg->init_param.min_output = min_out;
    cfg->init_param.max_integral = max_int;
    cfg->init_param.min_integral = min_int;
    cfg->init_param.cal_type = PID_CAL_TYPE_INCREMENTAL;
}

TEST_CASE("float positional PID: P-only", "[pid_ctrl][float][positional]")
{
    pid_ctrl_config_f_t cfg;
    make_pos_f_cfg(&cfg, 1.0f, 0.0f, 0.0f, 100.0f, -100.0f, 100.0f, -100.0f);
    pid_ctrl_block_handle_f_t h = NULL;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));
    TEST_ASSERT_NOT_NULL(h);

    float out = 0.0f;
    TEST_ESP_OK(pid_compute(h, 0.1f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.1f, out);

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("float positional PID: integral accumulation", "[pid_ctrl][float][positional][integral]")
{
    pid_ctrl_config_f_t cfg;
    make_pos_f_cfg(&cfg, 0.0f, 1.0f, 0.0f, 100.0f, -100.0f, 100.0f, -100.0f);
    pid_ctrl_block_handle_f_t h = NULL;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    float out = 0.0f;
    /* Constant error => integral grows linearly, output grows by error each step */
    for (int i = 1; i <= 5; i++) {
        TEST_ESP_OK(pid_compute(h, 0.1f, &out));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.1f * i, out);
    }

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("float positional PID: integral anti-windup (clamp)", "[pid_ctrl][float][positional][integral]")
{
    pid_ctrl_config_f_t cfg;
    make_pos_f_cfg(&cfg, 0.0f, 1.0f, 0.0f, 100.0f, -100.0f, 0.5f, -0.5f);
    pid_ctrl_block_handle_f_t h = NULL;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    float out = 0.0f;
    for (int i = 1; i <= 5; i++) {
        TEST_ESP_OK(pid_compute(h, 0.1f, &out));
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.1f * i, out);
    }
    /* Integral has reached +0.5 (clamped), further error does not wind it up */
    TEST_ESP_OK(pid_compute(h, 0.1f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.5f, out);

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("float positional PID: output clamping", "[pid_ctrl][float][positional][clamp]")
{
    pid_ctrl_config_f_t cfg;
    make_pos_f_cfg(&cfg, 1.0f, 0.0f, 0.0f, 0.05f, -0.05f, 100.0f, -100.0f);
    pid_ctrl_block_handle_f_t h = NULL;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    float out = 0.0f;
    /* Positive error drives output to max_output */
    TEST_ESP_OK(pid_compute(h, 1.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.05f, out);
    /* Negative error drives output to min_output */
    TEST_ESP_OK(pid_compute(h, -1.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -0.05f, out);

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("float incremental PID: first step", "[pid_ctrl][float][incremental]")
{
    /*
     * du = (e-e1)*Kp + (e-2*e1+e2)*Kd + e*Ki + u_{k-1}; first: e1=e2=0, u_prev=0
     * For e=0.1, Kp=2, Ki=1, Kd=0.5:
     *   = 0.1*2 + 0.1*0.5 + 0.1*1 = 0.2 + 0.05 + 0.1 = 0.35
     */
    pid_ctrl_config_f_t cfg;
    make_inc_f_cfg(&cfg, 2.0f, 1.0f, 0.5f, 100.0f, -100.0f, 100.0f, -100.0f);
    pid_ctrl_block_handle_f_t h = NULL;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    float out = 0.0f;
    TEST_ESP_OK(pid_compute(h, 0.1f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.35f, out);

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("float incremental PID: accumulates previous output over steps", "[pid_ctrl][float][incremental]")
{
    /*
     * With Ki=Kd=0 and Kp=1, the delta follows the change in error plus the
     * previous output, so for a ramp e = 0.1, 0.2, 0.3 the output tracks e.
     */
    pid_ctrl_config_f_t cfg;
    make_inc_f_cfg(&cfg, 1.0f, 0.0f, 0.0f, 100.0f, -100.0f, 100.0f, -100.0f);
    pid_ctrl_block_handle_f_t h = NULL;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    float out = 0.0f;
    TEST_ESP_OK(pid_compute(h, 0.1f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.1f, out);
    TEST_ESP_OK(pid_compute(h, 0.2f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.2f, out);
    TEST_ESP_OK(pid_compute(h, 0.3f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.3f, out);

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("float incremental PID: output clamping", "[pid_ctrl][float][incremental][clamp]")
{
    pid_ctrl_config_f_t cfg;
    make_inc_f_cfg(&cfg, 1.0f, 1.0f, 0.0f, 0.05f, -0.05f, 100.0f, -100.0f);
    pid_ctrl_block_handle_f_t h = NULL;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    float out = 0.0f;
    /* Large positive error gives a delta far above max_output */
    TEST_ESP_OK(pid_compute(h, 10.0f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.05f, out);

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("float PID update_parameters: switch type and gains", "[pid_ctrl][float][update]")
{
    pid_ctrl_config_f_t cfg;
    make_pos_f_cfg(&cfg, 1.0f, 0.0f, 0.0f, 100.0f, -100.0f, 100.0f, -100.0f);
    pid_ctrl_block_handle_f_t h = NULL;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    /* Change Kp and switch to incremental */
    pid_ctrl_parameter_f_t params = {
        .kp = 3.0f,
        .ki = 0.0f,
        .kd = 0.0f,
        .max_output = 100.0f,
        .min_output = -100.0f,
        .max_integral = 100.0f,
        .min_integral = -100.0f,
        .cal_type = PID_CAL_TYPE_INCREMENTAL,
    };
    TEST_ESP_OK(pid_update_parameters(h, &params));

    float out = 0.0f;
    TEST_ESP_OK(pid_compute(h, 0.1f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.3f, out);

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("float PID reset clears accumulated state", "[pid_ctrl][float][reset]")
{
    pid_ctrl_config_f_t cfg;
    make_pos_f_cfg(&cfg, 0.0f, 1.0f, 0.0f, 100.0f, -100.0f, 100.0f, -100.0f);
    pid_ctrl_block_handle_f_t h = NULL;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    float out = 0.0f;
    /* Accumulate some integral */
    TEST_ESP_OK(pid_compute(h, 0.1f, &out));
    TEST_ESP_OK(pid_compute(h, 0.1f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.2f, out);

    /* Reset: integral and error history must be cleared */
    TEST_ESP_OK(pid_reset_ctrl_block(h));
    TEST_ESP_OK(pid_compute(h, 0.1f, &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.1f, out);

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("float PID invalid inputs", "[pid_ctrl][float][error]")
{
    pid_ctrl_config_f_t cfg;
    make_pos_f_cfg(&cfg, 1.0f, 0.0f, 0.0f, 100.0f, -100.0f, 100.0f, -100.0f);
    pid_ctrl_block_handle_f_t h = NULL;
    pid_ctrl_config_f_t *null_cfg = NULL;
    pid_ctrl_block_handle_f_t null_h = NULL;
    float *null_out = NULL;

    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_new_control_block(null_cfg, &h));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_new_control_block(&cfg, NULL));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_del_control_block(null_h));

    pid_ctrl_block_handle_f_t ok = NULL;
    TEST_ESP_OK(pid_new_control_block(&cfg, &ok));
    float out = 0.0f;
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_compute(null_h, 0.1f, &out));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_compute(ok, 0.1f, null_out));

    pid_ctrl_parameter_f_t params = cfg.init_param;
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_update_parameters(null_h, &params));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_update_parameters(ok, NULL));

    params.cal_type = (pid_calculate_type_t)0x7f;
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_update_parameters(ok, &params));

    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_reset_ctrl_block(null_h));

    TEST_ESP_OK(pid_del_control_block(ok));
}

/* Keep one IQ case here to verify the C _Generic dispatch independently from
 * the IQ algorithm coverage in the C++ test file. */
TEST_CASE("C _Generic API: IQ smoke test", "[pid_ctrl][generic][iq]")
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
    pid_ctrl_block_handle_iq_t h = NULL;

    TEST_ESP_OK(pid_new_control_block(&cfg, &h));
    TEST_ASSERT_NOT_NULL(h);

    _iq out = 0;
    TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.1f, _IQtoF(out));

    TEST_ESP_OK(pid_del_control_block(h));
}
