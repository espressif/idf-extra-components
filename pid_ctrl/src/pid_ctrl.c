/*
 * SPDX-FileCopyrightText: 2015-2021 Espressif Systems (Shanghai) CO LTD
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

typedef struct pid_ctrl_block_t pid_ctrl_block_t;
typedef pid_ctrl_num_t (*pid_cal_func_t)(pid_ctrl_block_t *pid, pid_ctrl_num_t error);

struct pid_ctrl_block_t {
    pid_ctrl_num_t kp;
    pid_ctrl_num_t ki;
    pid_ctrl_num_t kd;
    pid_ctrl_num_t previous_err1;
    pid_ctrl_num_t previous_err2;
    pid_ctrl_num_t integral_err;
    pid_ctrl_num_t last_output;
    pid_ctrl_num_t max_output;
    pid_ctrl_num_t min_output;
    pid_ctrl_num_t max_integral;
    pid_ctrl_num_t min_integral;
    pid_cal_func_t calculate_func;
};

static inline pid_ctrl_num_t pid_num_zero(void)
{
#if CONFIG_PID_CTRL_NUM_TYPE_IQ
    return _IQ(0.0f);
#else
    return 0.0f;
#endif
}

static inline pid_ctrl_num_t pid_num_mul(pid_ctrl_num_t a, pid_ctrl_num_t b)
{
#if CONFIG_PID_CTRL_NUM_TYPE_IQ
    return _IQmpy(a, b);
#else
    return a * b;
#endif
}

static inline pid_ctrl_num_t pid_num_clamp(pid_ctrl_num_t value, pid_ctrl_num_t min_value, pid_ctrl_num_t max_value)
{
    return MAX(min_value, MIN(value, max_value));
}

static pid_ctrl_num_t pid_calc_positional(pid_ctrl_block_t *pid, pid_ctrl_num_t error)
{
    pid_ctrl_num_t output;

    pid->integral_err += error;
    pid->integral_err = pid_num_clamp(pid->integral_err, pid->min_integral, pid->max_integral);

    output = pid_num_mul(error, pid->kp) +
             pid_num_mul(error - pid->previous_err1, pid->kd) +
             pid_num_mul(pid->integral_err, pid->ki);
    output = pid_num_clamp(output, pid->min_output, pid->max_output);

    pid->previous_err1 = error;
    return output;
}

static pid_ctrl_num_t pid_calc_incremental(pid_ctrl_block_t *pid, pid_ctrl_num_t error)
{
    pid_ctrl_num_t output;

    output = pid_num_mul(error - pid->previous_err1, pid->kp) +
             pid_num_mul(error - pid->previous_err1 - pid->previous_err1 + pid->previous_err2, pid->kd) +
             pid_num_mul(error, pid->ki) +
             pid->last_output;
    output = pid_num_clamp(output, pid->min_output, pid->max_output);

    pid->previous_err2 = pid->previous_err1;
    pid->previous_err1 = error;
    pid->last_output = output;
    return output;
}

pid_ctrl_num_t pid_ctrl_num_from_float(float value)
{
#if CONFIG_PID_CTRL_NUM_TYPE_IQ
    return (pid_ctrl_num_t)_IQ(value);
#else
    return (pid_ctrl_num_t)value;
#endif
}

float pid_ctrl_num_to_float(pid_ctrl_num_t value)
{
#if CONFIG_PID_CTRL_NUM_TYPE_IQ
    return _IQtoF(value);
#else
    return (float)value;
#endif
}

esp_err_t pid_new_control_block(const pid_ctrl_config_t *config, pid_ctrl_block_handle_t *ret_pid)
{
    esp_err_t ret = ESP_OK;
    pid_ctrl_block_t *pid = NULL;

    ESP_GOTO_ON_FALSE(config && ret_pid, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");

    pid = calloc(1, sizeof(pid_ctrl_block_t));
    ESP_GOTO_ON_FALSE(pid, ESP_ERR_NO_MEM, err, TAG, "no mem for PID control block");

    ESP_GOTO_ON_ERROR(pid_update_parameters(pid, &config->init_param), err, TAG, "init PID parameters failed");
    *ret_pid = pid;
    return ret;

err:
    free(pid);
    return ret;
}

esp_err_t pid_del_control_block(pid_ctrl_block_handle_t pid)
{
    ESP_RETURN_ON_FALSE(pid, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    free(pid);
    return ESP_OK;
}

esp_err_t pid_compute(pid_ctrl_block_handle_t pid, pid_ctrl_num_t input_error, pid_ctrl_num_t *ret_result)
{
    ESP_RETURN_ON_FALSE(pid && ret_result, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    *ret_result = pid->calculate_func(pid, input_error);
    return ESP_OK;
}

esp_err_t pid_update_parameters(pid_ctrl_block_handle_t pid, const pid_ctrl_parameter_t *params)
{
    ESP_RETURN_ON_FALSE(pid && params, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    pid->kp = params->kp;
    pid->ki = params->ki;
    pid->kd = params->kd;
    pid->max_output = params->max_output;
    pid->min_output = params->min_output;
    pid->max_integral = params->max_integral;
    pid->min_integral = params->min_integral;

    switch (params->cal_type) {
    case PID_CAL_TYPE_INCREMENTAL:
        pid->calculate_func = pid_calc_incremental;
        break;
    case PID_CAL_TYPE_POSITIONAL:
        pid->calculate_func = pid_calc_positional;
        break;
    default:
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_ARG, TAG, "invalid PID calculation type:%d", params->cal_type);
    }

    return ESP_OK;
}

esp_err_t pid_reset_ctrl_block(pid_ctrl_block_handle_t pid)
{
    ESP_RETURN_ON_FALSE(pid, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    pid->integral_err = pid_num_zero();
    pid->previous_err1 = pid_num_zero();
    pid->previous_err2 = pid_num_zero();
    pid->last_output = pid_num_zero();
    return ESP_OK;
}
