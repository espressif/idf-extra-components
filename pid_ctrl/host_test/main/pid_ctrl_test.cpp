/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * pid_ctrl C++ host tests: IQmath algorithm via C++ overloads.
 * Float is a single dispatch smoke; float algorithm coverage lives in
 * pid_ctrl_test.c.
 */

#include "unity.h"
#include "pid_ctrl.h"
#include "IQmathLib.h"

#define TEST_EPS (1e-5f)
#define TEST_IQ_EPS (2e-3f)
#define TEST_IQ8_EPS (1e-2f)

namespace {

static pid_ctrl_config_iq_t make_pos_iq_cfg(_iq kp, _iq ki, _iq kd,
                                            _iq max_out = _IQ(100.0f),
                                            _iq min_out = _IQ(-100.0f),
                                            _iq max_int = _IQ(100.0f),
                                            _iq min_int = _IQ(-100.0f))
{
    pid_ctrl_config_iq_t cfg = {};
    cfg.init_param.kp = kp;
    cfg.init_param.ki = ki;
    cfg.init_param.kd = kd;
    cfg.init_param.max_output = max_out;
    cfg.init_param.min_output = min_out;
    cfg.init_param.max_integral = max_int;
    cfg.init_param.min_integral = min_int;
    cfg.init_param.cal_type = PID_CAL_TYPE_POSITIONAL;
    return cfg;
}

static pid_ctrl_config_iq_t make_inc_iq_cfg(_iq kp, _iq ki, _iq kd,
                                            _iq max_out = _IQ(100.0f),
                                            _iq min_out = _IQ(-100.0f),
                                            _iq max_int = _IQ(100.0f),
                                            _iq min_int = _IQ(-100.0f))
{
    pid_ctrl_config_iq_t cfg = make_pos_iq_cfg(kp, ki, kd, max_out, min_out, max_int, min_int);
    cfg.init_param.cal_type = PID_CAL_TYPE_INCREMENTAL;
    return cfg;
}

}  // namespace

TEST_CASE("IQ positional PID: P-only", "[pid_ctrl][iq][positional]")
{
    const pid_ctrl_config_iq_t cfg = make_pos_iq_cfg(_IQ(1.0f), _IQ(0.0f), _IQ(0.0f));
    pid_ctrl_block_handle_iq_t h = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));
    TEST_ASSERT_NOT_NULL(h);

    _iq out = 0;
    TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.1f, _IQtoF(out));

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("IQ positional PID: integral accumulation", "[pid_ctrl][iq][positional][integral]")
{
    const pid_ctrl_config_iq_t cfg = make_pos_iq_cfg(_IQ(0.0f), _IQ(1.0f), _IQ(0.0f));
    pid_ctrl_block_handle_iq_t h = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    _iq out = 0;
    for (int i = 1; i <= 5; ++i) {
        TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
        TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.1f * i, _IQtoF(out));
    }

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("IQ positional PID: integral anti-windup (clamp)", "[pid_ctrl][iq][positional][integral]")
{
    const pid_ctrl_config_iq_t cfg = make_pos_iq_cfg(
                                         _IQ(0.0f), _IQ(1.0f), _IQ(0.0f), _IQ(100.0f), _IQ(-100.0f), _IQ(0.5f), _IQ(-0.5f));
    pid_ctrl_block_handle_iq_t h = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    _iq out = 0;
    for (int i = 1; i <= 5; ++i) {
        TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
        TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.1f * i, _IQtoF(out));
    }
    TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.5f, _IQtoF(out));

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("IQ positional PID: derivative on error change", "[pid_ctrl][iq][positional][derivative]")
{
    const pid_ctrl_config_iq_t cfg = make_pos_iq_cfg(_IQ(0.0f), _IQ(0.0f), _IQ(1.0f));
    pid_ctrl_block_handle_iq_t h = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    _iq out = 0;
    TEST_ESP_OK(pid_compute(h, _IQ(0.2f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.2f, _IQtoF(out));
    TEST_ESP_OK(pid_compute(h, _IQ(0.5f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.3f, _IQtoF(out));
    TEST_ESP_OK(pid_compute(h, _IQ(0.5f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.0f, _IQtoF(out));

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("IQ positional PID: output clamping", "[pid_ctrl][iq][positional][clamp]")
{
    const pid_ctrl_config_iq_t cfg = make_pos_iq_cfg(
                                         _IQ(1.0f), _IQ(0.0f), _IQ(0.0f), _IQ(0.05f), _IQ(-0.05f));
    pid_ctrl_block_handle_iq_t h = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    _iq out = 0;
    TEST_ESP_OK(pid_compute(h, _IQ(1.0f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.05f, _IQtoF(out));
    TEST_ESP_OK(pid_compute(h, _IQ(-1.0f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, -0.05f, _IQtoF(out));

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("IQ incremental PID: first step", "[pid_ctrl][iq][incremental]")
{
    const pid_ctrl_config_iq_t cfg = make_inc_iq_cfg(_IQ(2.0f), _IQ(1.0f), _IQ(0.5f));
    pid_ctrl_block_handle_iq_t h = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    _iq out = 0;
    TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.35f, _IQtoF(out));

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("IQ incremental PID: accumulates previous output over steps", "[pid_ctrl][iq][incremental]")
{
    const pid_ctrl_config_iq_t cfg = make_inc_iq_cfg(_IQ(1.0f), _IQ(0.0f), _IQ(0.0f));
    pid_ctrl_block_handle_iq_t h = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    _iq out = 0;
    TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.1f, _IQtoF(out));
    TEST_ESP_OK(pid_compute(h, _IQ(0.2f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.2f, _IQtoF(out));
    TEST_ESP_OK(pid_compute(h, _IQ(0.3f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.3f, _IQtoF(out));

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("IQ incremental PID: output clamping", "[pid_ctrl][iq][incremental][clamp]")
{
    const pid_ctrl_config_iq_t cfg = make_inc_iq_cfg(
                                         _IQ(1.0f), _IQ(1.0f), _IQ(0.0f), _IQ(0.05f), _IQ(-0.05f));
    pid_ctrl_block_handle_iq_t h = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    _iq out = 0;
    TEST_ESP_OK(pid_compute(h, _IQ(10.0f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.05f, _IQtoF(out));

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("IQ PID update_parameters: switch type and gains", "[pid_ctrl][iq][update]")
{
    const pid_ctrl_config_iq_t cfg = make_pos_iq_cfg(_IQ(1.0f), _IQ(0.0f), _IQ(0.0f));
    pid_ctrl_block_handle_iq_t h = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    pid_ctrl_parameter_iq_t params = {
        .kp = _IQ(3.0f),
        .ki = _IQ(0.0f),
        .kd = _IQ(0.0f),
        .max_output = _IQ(100.0f),
        .min_output = _IQ(-100.0f),
        .max_integral = _IQ(100.0f),
        .min_integral = _IQ(-100.0f),
        .cal_type = PID_CAL_TYPE_INCREMENTAL,
    };
    TEST_ESP_OK(pid_update_parameters(h, &params));

    _iq out = 0;
    TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.3f, _IQtoF(out));

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("IQ PID reset clears accumulated state", "[pid_ctrl][iq][reset]")
{
    const pid_ctrl_config_iq_t cfg = make_pos_iq_cfg(_IQ(0.0f), _IQ(1.0f), _IQ(0.0f));
    pid_ctrl_block_handle_iq_t h = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    _iq out = 0;
    TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
    TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.2f, _IQtoF(out));

    TEST_ESP_OK(pid_reset_ctrl_block(h));
    TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.1f, _IQtoF(out));

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("IQ PID invalid inputs", "[pid_ctrl][iq][error]")
{
    const pid_ctrl_config_iq_t cfg = make_pos_iq_cfg(_IQ(1.0f), _IQ(0.0f), _IQ(0.0f));
    pid_ctrl_block_handle_iq_t h = nullptr;
    pid_ctrl_block_handle_iq_t null_h = nullptr;
    pid_ctrl_config_iq_t *null_cfg = nullptr;
    _iq *null_out = nullptr;

    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_new_control_block(null_cfg, &h));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_new_control_block(&cfg, nullptr));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_del_control_block(null_h));

    TEST_ESP_OK(pid_new_control_block(&cfg, &h));
    _iq out = 0;
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_compute(null_h, _IQ(0.1f), &out));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_compute(h, _IQ(0.1f), null_out));

    pid_ctrl_parameter_iq_t params = cfg.init_param;
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_update_parameters(null_h, &params));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_update_parameters(h, nullptr));

    params.cal_type = (pid_calculate_type_t)0x7f;
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_update_parameters(h, &params));
    TEST_ESP_ERR(ESP_ERR_INVALID_ARG, pid_reset_ctrl_block(null_h));

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("C++ generic API: float smoke test", "[pid_ctrl][cpp][generic][float]")
{
    pid_ctrl_config_f_t cfg = {};
    cfg.init_param.kp = 1.0f;
    cfg.init_param.max_output = 100.0f;
    cfg.init_param.min_output = -100.0f;
    cfg.init_param.max_integral = 100.0f;
    cfg.init_param.min_integral = -100.0f;
    cfg.init_param.cal_type = PID_CAL_TYPE_POSITIONAL;

    pid_ctrl_block_handle_f_t h = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));
    TEST_ASSERT_NOT_NULL(h);

    float out = 0.0f;
    TEST_ESP_OK(pid_compute(h, 0.1f, &out));
    TEST_ASSERT_FLOAT_WITHIN(TEST_EPS, 0.1f, out);
    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("IQ: every Q-format backend", "[pid_ctrl][iq][allformats]")
{
    /* Limits of ±1 and an error of 0.5 fit every Q1..Q30 range and are
     * exactly representable in all of them. */
#define PID_IQ_SMOKE(_q)                                                         \
    do {                                                                         \
        pid_ctrl_config_iq##_q##_t cfg = {};                                     \
        cfg.init_param.kp = _IQ##_q(1.0f);                                       \
        cfg.init_param.ki = _IQ##_q(0.0f);                                       \
        cfg.init_param.kd = _IQ##_q(0.0f);                                       \
        cfg.init_param.max_output = _IQ##_q(1.0f);                               \
        cfg.init_param.min_output = _IQ##_q(-1.0f);                              \
        cfg.init_param.max_integral = _IQ##_q(1.0f);                             \
        cfg.init_param.min_integral = _IQ##_q(-1.0f);                            \
        cfg.init_param.cal_type = PID_CAL_TYPE_POSITIONAL;                       \
        pid_ctrl_block_handle_iq##_q##_t h = nullptr;                            \
        TEST_ESP_OK(pid_new_control_block(&cfg, &h));                            \
        _iq##_q out = 0;                                                         \
        TEST_ESP_OK(pid_compute(h, _IQ##_q(0.5f), &out));                        \
        TEST_ASSERT_FLOAT_WITHIN(TEST_IQ_EPS, 0.5f, _IQ##_q##toF(out));          \
        TEST_ESP_OK(pid_del_control_block(h));                                   \
    } while (0);
    PID_IQ_FORMATS(PID_IQ_SMOKE)
#undef PID_IQ_SMOKE
}
