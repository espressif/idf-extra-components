/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdlib.h>
#include <sys/param.h>
#include "esp_check.h"
#include "esp_log.h"
#include "pid_ctrl.h"

static const char *TAG = "pid_ctrl";

struct pid_ctrl_block_iq_t {
    _iq kp;
    _iq ki;
    _iq kd;
    _iq previous_err1;
    _iq previous_err2;
    _iq integral_err;
    _iq last_output;
    _iq max_output;
    _iq min_output;
    _iq max_integral;
    _iq min_integral;
    _iq (*calculate_func)(struct pid_ctrl_block_iq_t *pid, _iq error);
};

static inline _iq pid_clamp_iq(_iq value, _iq min_value, _iq max_value)
{
    return MAX(min_value, MIN(value, max_value));
}

static _iq pid_calc_positional_iq(struct pid_ctrl_block_iq_t *pid, _iq error)
{
    _iq output;

    pid->integral_err += error;
    pid->integral_err = pid_clamp_iq(pid->integral_err, pid->min_integral, pid->max_integral);

    output = _IQmpy(error, pid->kp) +
             _IQmpy(error - pid->previous_err1, pid->kd) +
             _IQmpy(pid->integral_err, pid->ki);
    output = pid_clamp_iq(output, pid->min_output, pid->max_output);

    pid->previous_err1 = error;
    return output;
}

static _iq pid_calc_incremental_iq(struct pid_ctrl_block_iq_t *pid, _iq error)
{
    _iq output;

    output = _IQmpy(error - pid->previous_err1, pid->kp) +
             _IQmpy(error - pid->previous_err1 - pid->previous_err1 + pid->previous_err2, pid->kd) +
             _IQmpy(error, pid->ki) +
             pid->last_output;
    output = pid_clamp_iq(output, pid->min_output, pid->max_output);

    pid->previous_err2 = pid->previous_err1;
    pid->previous_err1 = error;
    pid->last_output = output;
    return output;
}

esp_err_t pid_update_parameters_iq(pid_ctrl_block_handle_iq_t pid, const pid_ctrl_parameter_iq_t *params)
{
    struct pid_ctrl_block_iq_t *b = (struct pid_ctrl_block_iq_t *)pid;

    ESP_RETURN_ON_FALSE(b && params, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    b->kp = params->kp;
    b->ki = params->ki;
    b->kd = params->kd;
    b->max_output = params->max_output;
    b->min_output = params->min_output;
    b->max_integral = params->max_integral;
    b->min_integral = params->min_integral;

    switch (params->cal_type) {
    case PID_CAL_TYPE_INCREMENTAL:
        b->calculate_func = pid_calc_incremental_iq;
        break;
    case PID_CAL_TYPE_POSITIONAL:
        b->calculate_func = pid_calc_positional_iq;
        break;
    default:
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_ARG, TAG, "invalid PID calculation type:%d", params->cal_type);
    }

    return ESP_OK;
}

esp_err_t pid_new_control_block_iq(const pid_ctrl_config_iq_t *config, pid_ctrl_block_handle_iq_t *ret_pid)
{
    esp_err_t ret = ESP_OK;
    struct pid_ctrl_block_iq_t *pid = NULL;

    ESP_GOTO_ON_FALSE(config && ret_pid, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");

    pid = calloc(1, sizeof(struct pid_ctrl_block_iq_t));
    ESP_GOTO_ON_FALSE(pid, ESP_ERR_NO_MEM, err, TAG, "no mem for PID control block");

    ESP_GOTO_ON_ERROR(pid_update_parameters_iq(pid, &config->init_param), err, TAG, "init PID parameters failed");
    *ret_pid = pid;
    return ret;

err:
    free(pid);
    return ret;
}

esp_err_t pid_del_control_block_iq(pid_ctrl_block_handle_iq_t pid)
{
    ESP_RETURN_ON_FALSE(pid, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    free(pid);
    return ESP_OK;
}

esp_err_t pid_compute_iq(pid_ctrl_block_handle_iq_t pid, _iq input_error, _iq *ret_result)
{
    struct pid_ctrl_block_iq_t *b = (struct pid_ctrl_block_iq_t *)pid;

    ESP_RETURN_ON_FALSE(b && ret_result, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    *ret_result = b->calculate_func(b, input_error);
    return ESP_OK;
}

esp_err_t pid_reset_ctrl_block_iq(pid_ctrl_block_handle_iq_t pid)
{
    struct pid_ctrl_block_iq_t *b = (struct pid_ctrl_block_iq_t *)pid;

    ESP_RETURN_ON_FALSE(b, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    b->integral_err = _IQ(0.0f);
    b->previous_err1 = _IQ(0.0f);
    b->previous_err2 = _IQ(0.0f);
    b->last_output = _IQ(0.0f);
    return ESP_OK;
}
