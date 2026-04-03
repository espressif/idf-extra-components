/*
 * SPDX-FileCopyrightText: 2015-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_err.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PID calculation type
 *
 */
typedef enum {
    PID_CAL_TYPE_INCREMENTAL, /*!< Incremental PID control */
    PID_CAL_TYPE_POSITIONAL,  /*!< Positional PID control */
} pid_calculate_type_t;

/**
 * @brief Type of float PID control block handle
 */
typedef struct pid_ctrl_block_f_t *pid_ctrl_block_f_handle_t;

/**
 * @brief PID control parameters (float)
 */
typedef struct {
    float kp;                      /*!< PID Kp parameter */
    float ki;                      /*!< PID Ki parameter */
    float kd;                      /*!< PID Kd parameter */
    float max_output;              /*!< PID maximum output limitation */
    float min_output;              /*!< PID minimum output limitation */
    float max_integral;            /*!< PID maximum integral value limitation */
    float min_integral;            /*!< PID minimum integral value limitation */
    pid_calculate_type_t cal_type; /*!< PID calculation type */
} pid_ctrl_parameter_f_t;

/**
 * @brief PID control configuration (float)
 */
typedef struct {
    pid_ctrl_parameter_f_t init_param; /*!< Initial parameters */
} pid_ctrl_config_f_t;

esp_err_t pid_new_control_block_f(const pid_ctrl_config_f_t *config, pid_ctrl_block_f_handle_t *ret_pid);
esp_err_t pid_del_control_block_f(pid_ctrl_block_f_handle_t pid);
esp_err_t pid_update_parameters_f(pid_ctrl_block_f_handle_t pid, const pid_ctrl_parameter_f_t *params);
esp_err_t pid_compute_f(pid_ctrl_block_f_handle_t pid, float input_error, float *ret_result);
esp_err_t pid_reset_ctrl_block_f(pid_ctrl_block_f_handle_t pid);

#ifndef GLOBAL_IQ
#define GLOBAL_IQ CONFIG_PID_CTRL_IQ_FORMAT
#endif
#include "IQmathLib.h"
/**
 * @brief Type of IQmath PID control block handle
 */
typedef struct pid_ctrl_block_iq_t *pid_ctrl_block_iq_handle_t;

/**
 * @brief PID control parameters (IQ)
 */
typedef struct {
    _iq kp;                        /*!< PID Kp parameter */
    _iq ki;                        /*!< PID Ki parameter */
    _iq kd;                        /*!< PID Kd parameter */
    _iq max_output;                /*!< PID maximum output limitation */
    _iq min_output;                /*!< PID minimum output limitation */
    _iq max_integral;              /*!< PID maximum integral value limitation */
    _iq min_integral;              /*!< PID minimum integral value limitation */
    pid_calculate_type_t cal_type; /*!< PID calculation type */
} pid_ctrl_parameter_iq_t;

/**
 * @brief PID control configuration (IQ)
 */
typedef struct {
    pid_ctrl_parameter_iq_t init_param; /*!< Initial parameters */
} pid_ctrl_config_iq_t;

esp_err_t pid_new_control_block_iq(const pid_ctrl_config_iq_t *config, pid_ctrl_block_iq_handle_t *ret_pid);
esp_err_t pid_del_control_block_iq(pid_ctrl_block_iq_handle_t pid);
esp_err_t pid_update_parameters_iq(pid_ctrl_block_iq_handle_t pid, const pid_ctrl_parameter_iq_t *params);
esp_err_t pid_compute_iq(pid_ctrl_block_iq_handle_t pid, _iq input_error, _iq *ret_result);
esp_err_t pid_reset_ctrl_block_iq(pid_ctrl_block_iq_handle_t pid);

/**
 * @name Legacy (float) API aliases
 *
 * These are equivalent to the @c *_f() functions and allow existing code
 * to stay unchanged. Unsuffixed generic helpers are C-only, see below.
 */
/**@{*/
typedef pid_ctrl_block_f_handle_t pid_ctrl_block_handle_t;
typedef pid_ctrl_parameter_f_t pid_ctrl_parameter_t;
typedef pid_ctrl_config_f_t pid_ctrl_config_t;
/**@}*/

#ifdef __cplusplus
} // extern "C"
#endif

#ifndef __cplusplus
#define pid_new_control_block(config, ret_pid)               \
    _Generic((config), const pid_ctrl_config_f_t  *: pid_new_control_block_f, \
        pid_ctrl_config_f_t  *: pid_new_control_block_f,       \
        const pid_ctrl_config_iq_t *: pid_new_control_block_iq, \
        pid_ctrl_config_iq_t *: pid_new_control_block_iq)      \
    ((config), (ret_pid))

#define pid_del_control_block(pid)                         \
    _Generic((pid),                                        \
        pid_ctrl_block_f_handle_t:  pid_del_control_block_f, \
        pid_ctrl_block_iq_handle_t: pid_del_control_block_iq)  \
    ((pid))

#define pid_update_parameters(pid, params)                 \
    _Generic((pid),                                        \
        pid_ctrl_block_f_handle_t:  pid_update_parameters_f,  \
        pid_ctrl_block_iq_handle_t: pid_update_parameters_iq) \
    ((pid), (params))

#define pid_compute(pid, input_error, ret_result)         \
    _Generic((pid),                                        \
        pid_ctrl_block_f_handle_t:  pid_compute_f,        \
        pid_ctrl_block_iq_handle_t: pid_compute_iq)         \
    ((pid), (input_error), (ret_result))

#define pid_reset_ctrl_block(pid)                          \
    _Generic((pid),                                        \
        pid_ctrl_block_f_handle_t:  pid_reset_ctrl_block_f,  \
        pid_ctrl_block_iq_handle_t: pid_reset_ctrl_block_iq)  \
    ((pid))
#else
/* C++: overloaded inline wrappers dispatch to _f or _iq based on argument type. */
static inline esp_err_t pid_new_control_block(const pid_ctrl_config_f_t *config, pid_ctrl_block_f_handle_t *ret_pid)
{
    return pid_new_control_block_f(config, ret_pid);
}
static inline esp_err_t pid_new_control_block(const pid_ctrl_config_iq_t *config, pid_ctrl_block_iq_handle_t *ret_pid)
{
    return pid_new_control_block_iq(config, ret_pid);
}

static inline esp_err_t pid_del_control_block(pid_ctrl_block_f_handle_t pid)
{
    return pid_del_control_block_f(pid);
}
static inline esp_err_t pid_del_control_block(pid_ctrl_block_iq_handle_t pid)
{
    return pid_del_control_block_iq(pid);
}

static inline esp_err_t pid_update_parameters(pid_ctrl_block_f_handle_t pid, const pid_ctrl_parameter_f_t *params)
{
    return pid_update_parameters_f(pid, params);
}
static inline esp_err_t pid_update_parameters(pid_ctrl_block_iq_handle_t pid, const pid_ctrl_parameter_iq_t *params)
{
    return pid_update_parameters_iq(pid, params);
}

static inline esp_err_t pid_compute(pid_ctrl_block_f_handle_t pid, float input_error, float *ret_result)
{
    return pid_compute_f(pid, input_error, ret_result);
}
static inline esp_err_t pid_compute(pid_ctrl_block_iq_handle_t pid, _iq input_error, _iq *ret_result)
{
    return pid_compute_iq(pid, input_error, ret_result);
}

static inline esp_err_t pid_reset_ctrl_block(pid_ctrl_block_f_handle_t pid)
{
    return pid_reset_ctrl_block_f(pid);
}
static inline esp_err_t pid_reset_ctrl_block(pid_ctrl_block_iq_handle_t pid)
{
    return pid_reset_ctrl_block_iq(pid);
}
#endif
