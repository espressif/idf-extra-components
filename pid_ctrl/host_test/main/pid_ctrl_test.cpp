/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * pid_ctrl C++ host tests.
 *
 * These cases mirror pid_ctrl_test.c one-to-one. The C tests exercise the
 * float backend through the C _Generic API; these tests exercise the IQ
 * backend through the C++ overloads.
 *
 * The cases below use the unsuffixed _iq API, which keeps following GLOBAL_IQ.
 * Concrete Q-formats (_iq1 .. _iq30) are covered by the multi-format cases
 * at the end of this file.
 */

#include "unity.h"
#include "pid_ctrl.h"
#include "IQmathLib.h"

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
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.1f, _IQtoF(out));

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
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.1f * i, _IQtoF(out));
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
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.1f * i, _IQtoF(out));
    }
    TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.5f, _IQtoF(out));

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
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.05f, _IQtoF(out));
    TEST_ESP_OK(pid_compute(h, _IQ(-1.0f), &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -0.05f, _IQtoF(out));

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("IQ incremental PID: first step", "[pid_ctrl][iq][incremental]")
{
    const pid_ctrl_config_iq_t cfg = make_inc_iq_cfg(_IQ(2.0f), _IQ(1.0f), _IQ(0.5f));
    pid_ctrl_block_handle_iq_t h = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    _iq out = 0;
    TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
    TEST_ASSERT_FLOAT_WITHIN(2e-3f, 0.35f, _IQtoF(out));

    TEST_ESP_OK(pid_del_control_block(h));
}

TEST_CASE("IQ incremental PID: accumulates previous output over steps", "[pid_ctrl][iq][incremental]")
{
    const pid_ctrl_config_iq_t cfg = make_inc_iq_cfg(_IQ(1.0f), _IQ(0.0f), _IQ(0.0f));
    pid_ctrl_block_handle_iq_t h = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg, &h));

    _iq out = 0;
    TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.1f, _IQtoF(out));
    TEST_ESP_OK(pid_compute(h, _IQ(0.2f), &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.2f, _IQtoF(out));
    TEST_ESP_OK(pid_compute(h, _IQ(0.3f), &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.3f, _IQtoF(out));

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
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.05f, _IQtoF(out));

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
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.3f, _IQtoF(out));

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
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.2f, _IQtoF(out));

    TEST_ESP_OK(pid_reset_ctrl_block(h));
    TEST_ESP_OK(pid_compute(h, _IQ(0.1f), &out));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.1f, _IQtoF(out));

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
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.1f, out);
    TEST_ESP_OK(pid_del_control_block(h));
}

/*
 * The point of the typed configs: several Q-formats coexist in one firmware,
 * each dispatching to its own backend, without any GLOBAL_IQ agreement.
 *
 * The unsuffixed _iq backend is a separate instance as well, so it can be used
 * next to the concrete formats in the same translation unit.
 */
namespace {

static pid_ctrl_config_iq24_t make_pos_iq24_cfg(_iq24 kp, _iq24 ki, _iq24 kd)
{
    pid_ctrl_config_iq24_t cfg = {};
    cfg.init_param.kp = kp;
    cfg.init_param.ki = ki;
    cfg.init_param.kd = kd;
    cfg.init_param.max_output = _IQ24(100.0f);
    cfg.init_param.min_output = _IQ24(-100.0f);
    cfg.init_param.max_integral = _IQ24(100.0f);
    cfg.init_param.min_integral = _IQ24(-100.0f);
    cfg.init_param.cal_type = PID_CAL_TYPE_POSITIONAL;
    return cfg;
}

static pid_ctrl_config_iq8_t make_pos_iq8_cfg(_iq8 kp, _iq8 ki, _iq8 kd)
{
    pid_ctrl_config_iq8_t cfg = {};
    cfg.init_param.kp = kp;
    cfg.init_param.ki = ki;
    cfg.init_param.kd = kd;
    cfg.init_param.max_output = _IQ8(100.0f);
    cfg.init_param.min_output = _IQ8(-100.0f);
    cfg.init_param.max_integral = _IQ8(100.0f);
    cfg.init_param.min_integral = _IQ8(-100.0f);
    cfg.init_param.cal_type = PID_CAL_TYPE_POSITIONAL;
    return cfg;
}

}  // namespace

TEST_CASE("IQ: Q8, Q16 and Q24 backends coexist", "[pid_ctrl][iq][multiformat]")
{
    pid_ctrl_config_iq8_t cfg8 = make_pos_iq8_cfg(_IQ8(1.0f), _IQ8(0.0f), _IQ8(0.0f));
    pid_ctrl_block_handle_iq8 h8 = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg8, &h8));

    pid_ctrl_config_iq16_t cfg16 = {};
    cfg16.init_param.kp = _IQ16(1.0f);
    cfg16.init_param.ki = _IQ16(0.0f);
    cfg16.init_param.kd = _IQ16(0.0f);
    cfg16.init_param.max_output = _IQ16(100.0f);
    cfg16.init_param.min_output = _IQ16(-100.0f);
    cfg16.init_param.max_integral = _IQ16(100.0f);
    cfg16.init_param.min_integral = _IQ16(-100.0f);
    cfg16.init_param.cal_type = PID_CAL_TYPE_POSITIONAL;
    pid_ctrl_block_handle_iq16 h16 = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg16, &h16));

    pid_ctrl_config_iq24_t cfg24 = make_pos_iq24_cfg(_IQ24(1.0f), _IQ24(0.0f), _IQ24(0.0f));
    pid_ctrl_block_handle_iq24 h24 = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg24, &h24));

    /* The same physical quantity, expressed in three different Q-formats.
     * Each backend must produce the value in its own format. */
    _iq8 out8 = 0;
    _iq16 out16 = 0;
    _iq24 out24 = 0;
    TEST_ESP_OK(pid_compute(h8, _IQ8(0.5f), &out8));
    TEST_ESP_OK(pid_compute(h16, _IQ16(0.5f), &out16));
    TEST_ESP_OK(pid_compute(h24, _IQ24(0.5f), &out24));

    TEST_ASSERT_FLOAT_WITHIN(1e-2f, 0.5f, _IQ8toF(out8));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.5f, _IQ16toF(out16));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, _IQ24toF(out24));

    TEST_ESP_OK(pid_del_control_block(h8));
    TEST_ESP_OK(pid_del_control_block(h16));
    TEST_ESP_OK(pid_del_control_block(h24));
}

TEST_CASE("IQ: unsuffixed _iq backend coexists with the concrete formats",
          "[pid_ctrl][iq][multiformat]")
{
    /* _iq is an alias of whatever GLOBAL_IQ names, but it has its own types,
     * so it must be usable next to an explicit Q24 control block. */
    const pid_ctrl_config_iq_t cfg_iq = make_pos_iq_cfg(_IQ(1.0f), _IQ(0.0f), _IQ(0.0f));
    pid_ctrl_block_handle_iq_t h_iq = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg_iq, &h_iq));

    const pid_ctrl_config_iq24_t cfg24 = make_pos_iq24_cfg(_IQ24(1.0f), _IQ24(0.0f), _IQ24(0.0f));
    pid_ctrl_block_handle_iq24 h24 = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg24, &h24));

    _iq out_iq = 0;
    _iq24 out24 = 0;
    TEST_ESP_OK(pid_compute(h_iq, _IQ(0.5f), &out_iq));
    TEST_ESP_OK(pid_compute(h24, _IQ24(0.5f), &out24));

    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.5f, _IQtoF(out_iq));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, _IQ24toF(out24));

    TEST_ESP_OK(pid_del_control_block(h_iq));
    TEST_ESP_OK(pid_del_control_block(h24));
}

TEST_CASE("IQ: precision scales with the Q-format", "[pid_ctrl][iq][multiformat]")
{
    /* A small error is representable in Q24 but not in Q8, which pins down
     * that the two control blocks really use different arithmetic. */
    pid_ctrl_config_iq8_t cfg8 = make_pos_iq8_cfg(_IQ8(1.0f), _IQ8(0.0f), _IQ8(0.0f));
    pid_ctrl_block_handle_iq8 h8 = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg8, &h8));

    pid_ctrl_config_iq24_t cfg24 = make_pos_iq24_cfg(_IQ24(1.0f), _IQ24(0.0f), _IQ24(0.0f));
    pid_ctrl_block_handle_iq24 h24 = nullptr;
    TEST_ESP_OK(pid_new_control_block(&cfg24, &h24));

    /* 0.001 is below the Q8 resolution of 1/256, so it collapses to 0. */
    _iq8 out8 = 0;
    _iq24 out24 = 0;
    TEST_ESP_OK(pid_compute(h8, _IQ8(0.001f), &out8));
    TEST_ESP_OK(pid_compute(h24, _IQ24(0.001f), &out24));

    TEST_ASSERT_EQUAL_FLOAT(0.0f, _IQ8toF(out8));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.001f, _IQ24toF(out24));

    TEST_ESP_OK(pid_del_control_block(h8));
    TEST_ESP_OK(pid_del_control_block(h24));
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
        pid_ctrl_block_handle_iq##_q h = nullptr;                                \
        TEST_ESP_OK(pid_new_control_block(&cfg, &h));                            \
        _iq##_q out = 0;                                                         \
        TEST_ESP_OK(pid_compute(h, _IQ##_q(0.5f), &out));                        \
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, 0.5f, _IQ##_q##toF(out));                \
        TEST_ESP_OK(pid_del_control_block(h));                                   \
    } while (0);
    PID_IQ_FORMATS(PID_IQ_SMOKE)
#undef PID_IQ_SMOKE
}
