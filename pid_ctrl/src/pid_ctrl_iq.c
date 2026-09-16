/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdlib.h>
#include "esp_check.h"
#include "esp_log.h"
#include "pid_ctrl.h"
#include "pid_ctrl_iq_priv.h"

static const char *TAG = "pid_ctrl";

/*
 * One algorithm body, instantiated for every Q in PID_IQ_FORMATS. _IQNmpy is
 * resolved at compile time. The unsuffixed _iq API is a header wrapper around
 * this TU's GLOBAL_IQ;
 */

#define PID_IQ_BLOCK_FIELDS(_q) \
    _iq##_q kp;                 \
    _iq##_q ki;                 \
    _iq##_q kd;                 \
    _iq##_q previous_err1;      \
    _iq##_q previous_err2;      \
    _iq##_q integral_err;       \
    _iq##_q last_output;        \
    _iq##_q max_output;         \
    _iq##_q min_output;         \
    _iq##_q max_integral;       \
    _iq##_q min_integral

#define PID_IQ_DEFINE(_q)                                                                        \
                                                                                                 \
    struct pid_ctrl_block_iq##_q##_t {                                                           \
        PID_IQ_BLOCK_FIELDS(_q);                                                                 \
        _iq##_q (*calculate_func)(struct pid_ctrl_block_iq##_q##_t *pid,                         \
                                  _iq##_q error);                                                \
    };                                                                                           \
                                                                                                 \
    static _iq##_q pid_calc_positional_iq##_q(                                                   \
        struct pid_ctrl_block_iq##_q##_t *pid, _iq##_q error)                                    \
    {                                                                                            \
        _iq##_q output;                                                                          \
                                                                                                 \
        pid->integral_err += error;                                                              \
        pid->integral_err = PID_IQ_CLAMP(pid->integral_err,                                      \
                                         pid->min_integral, pid->max_integral);                  \
                                                                                                 \
        output = PID_IQ_MPY(_q, error, pid->kp) +                                                \
                 PID_IQ_MPY(_q, error - pid->previous_err1, pid->kd) +                           \
                 PID_IQ_MPY(_q, pid->integral_err, pid->ki);                                     \
        output = PID_IQ_CLAMP(output, pid->min_output, pid->max_output);                         \
                                                                                                 \
        pid->previous_err1 = error;                                                              \
        return output;                                                                           \
    }                                                                                            \
                                                                                                 \
    static _iq##_q pid_calc_incremental_iq##_q(                                                  \
        struct pid_ctrl_block_iq##_q##_t *pid, _iq##_q error)                                    \
    {                                                                                            \
        _iq##_q output;                                                                          \
                                                                                                 \
        output = PID_IQ_MPY(_q, error - pid->previous_err1, pid->kp) +                           \
                 PID_IQ_MPY(_q, error - pid->previous_err1 -                                     \
                            pid->previous_err1 + pid->previous_err2, pid->kd) +                  \
                 PID_IQ_MPY(_q, error, pid->ki) +                                                \
                 pid->last_output;                                                               \
        output = PID_IQ_CLAMP(output, pid->min_output, pid->max_output);                         \
                                                                                                 \
        pid->previous_err2 = pid->previous_err1;                                                 \
        pid->previous_err1 = error;                                                              \
        pid->last_output = output;                                                               \
        return output;                                                                           \
    }                                                                                            \
                                                                                                 \
    esp_err_t pid_update_parameters_iq##_q(pid_ctrl_block_handle_iq##_q##_t pid,                 \
                                           const pid_ctrl_parameter_iq##_q##_t *params)          \
    {                                                                                            \
        struct pid_ctrl_block_iq##_q##_t *b = (struct pid_ctrl_block_iq##_q##_t *)pid;           \
                                                                                                 \
        ESP_RETURN_ON_FALSE(b && params, ESP_ERR_INVALID_ARG, TAG, "invalid argument");          \
                                                                                                 \
        b->kp = params->kp;                                                                      \
        b->ki = params->ki;                                                                      \
        b->kd = params->kd;                                                                      \
        b->max_output = params->max_output;                                                      \
        b->min_output = params->min_output;                                                      \
        b->max_integral = params->max_integral;                                                  \
        b->min_integral = params->min_integral;                                                  \
                                                                                                 \
        switch (params->cal_type) {                                                              \
        case PID_CAL_TYPE_INCREMENTAL:                                                           \
            b->calculate_func = pid_calc_incremental_iq##_q;                                     \
            break;                                                                               \
        case PID_CAL_TYPE_POSITIONAL:                                                            \
            b->calculate_func = pid_calc_positional_iq##_q;                                      \
            break;                                                                               \
        default:                                                                                 \
            ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_ARG, TAG,                                 \
                                "invalid PID calculation type:%d", params->cal_type);            \
        }                                                                                        \
                                                                                                 \
        return ESP_OK;                                                                           \
    }                                                                                            \
                                                                                                 \
    esp_err_t pid_new_control_block_iq##_q(const pid_ctrl_config_iq##_q##_t *config,             \
                                           pid_ctrl_block_handle_iq##_q##_t *ret_pid)            \
    {                                                                                            \
        esp_err_t ret = ESP_OK;                                                                  \
        struct pid_ctrl_block_iq##_q##_t *pid = NULL;                                            \
                                                                                                 \
        ESP_GOTO_ON_FALSE(config && ret_pid, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument"); \
                                                                                                 \
        pid = calloc(1, sizeof(struct pid_ctrl_block_iq##_q##_t));                               \
        ESP_GOTO_ON_FALSE(pid, ESP_ERR_NO_MEM, err, TAG, "no mem for PID control block");        \
                                                                                                 \
        ESP_GOTO_ON_ERROR(pid_update_parameters_iq##_q(pid, &config->init_param),                \
                          err, TAG, "init PID parameters failed");                               \
        *ret_pid = pid;                                                                          \
        return ret;                                                                              \
                                                                                                 \
    err:                                                                                         \
        free(pid);                                                                               \
        return ret;                                                                              \
    }                                                                                            \
                                                                                                 \
    esp_err_t pid_del_control_block_iq##_q(pid_ctrl_block_handle_iq##_q##_t pid)                 \
    {                                                                                            \
        ESP_RETURN_ON_FALSE(pid, ESP_ERR_INVALID_ARG, TAG, "invalid argument");                  \
        free(pid);                                                                               \
        return ESP_OK;                                                                           \
    }                                                                                            \
                                                                                                 \
    esp_err_t pid_compute_iq##_q(pid_ctrl_block_handle_iq##_q##_t pid,                           \
                                 _iq##_q input_error, _iq##_q *ret_result)                       \
    {                                                                                            \
        struct pid_ctrl_block_iq##_q##_t *b = (struct pid_ctrl_block_iq##_q##_t *)pid;           \
                                                                                                 \
        ESP_RETURN_ON_FALSE(b && ret_result, ESP_ERR_INVALID_ARG, TAG, "invalid argument");      \
        *ret_result = b->calculate_func(b, input_error);                                         \
        return ESP_OK;                                                                           \
    }                                                                                            \
                                                                                                 \
    esp_err_t pid_reset_ctrl_block_iq##_q(pid_ctrl_block_handle_iq##_q##_t pid)                  \
    {                                                                                            \
        struct pid_ctrl_block_iq##_q##_t *b = (struct pid_ctrl_block_iq##_q##_t *)pid;           \
                                                                                                 \
        ESP_RETURN_ON_FALSE(b, ESP_ERR_INVALID_ARG, TAG, "invalid argument");                    \
                                                                                                 \
        b->integral_err = 0;                                                                     \
        b->previous_err1 = 0;                                                                    \
        b->previous_err2 = 0;                                                                    \
        b->last_output = 0;                                                                      \
        return ESP_OK;                                                                           \
    }

PID_IQ_FORMATS(PID_IQ_DEFINE)
#undef PID_IQ_DEFINE
#undef PID_IQ_BLOCK_FIELDS
