/*
 * SPDX-FileCopyrightText: 2015-2026 Espressif Systems (Shanghai) CO LTD
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

struct pid_ctrl_block_f_t {
    float kp;
    float ki;
    float kd;
    float previous_err1;
    float previous_err2;
    float integral_err;
    float last_output;
    float max_output;
    float min_output;
    float max_integral;
    float min_integral;
    float (*calculate_func)(struct pid_ctrl_block_f_t *pid, float error);
};

static inline float pid_clamp_f(float value, float min_value, float max_value)
{
    return MAX(min_value, MIN(value, max_value));
}

static float pid_calc_positional_f(struct pid_ctrl_block_f_t *pid, float error)
{
    float output;

    pid->integral_err += error;
    pid->integral_err = pid_clamp_f(pid->integral_err, pid->min_integral, pid->max_integral);

    output = error * pid->kp +
             (error - pid->previous_err1) * pid->kd +
             pid->integral_err * pid->ki;
    output = pid_clamp_f(output, pid->min_output, pid->max_output);

    pid->previous_err1 = error;
    return output;
}

static float pid_calc_incremental_f(struct pid_ctrl_block_f_t *pid, float error)
{
    float output;

    output = (error - pid->previous_err1) * pid->kp +
             (error - pid->previous_err1 - pid->previous_err1 + pid->previous_err2) * pid->kd +
             error * pid->ki +
             pid->last_output;
    output = pid_clamp_f(output, pid->min_output, pid->max_output);

    pid->previous_err2 = pid->previous_err1;
    pid->previous_err1 = error;
    pid->last_output = output;
    return output;
}

esp_err_t pid_update_parameters_f(pid_ctrl_block_f_handle_t pid, const pid_ctrl_parameter_f_t *params)
{
    struct pid_ctrl_block_f_t *b = (struct pid_ctrl_block_f_t *)pid;

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
        b->calculate_func = pid_calc_incremental_f;
        break;
    case PID_CAL_TYPE_POSITIONAL:
        b->calculate_func = pid_calc_positional_f;
        break;
    default:
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_ARG, TAG, "invalid PID calculation type:%d", params->cal_type);
    }

    return ESP_OK;
}

esp_err_t pid_new_control_block_f(const pid_ctrl_config_f_t *config, pid_ctrl_block_f_handle_t *ret_pid)
{
    esp_err_t ret = ESP_OK;
    struct pid_ctrl_block_f_t *pid = NULL;

    ESP_GOTO_ON_FALSE(config && ret_pid, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");

    pid = calloc(1, sizeof(struct pid_ctrl_block_f_t));
    ESP_GOTO_ON_FALSE(pid, ESP_ERR_NO_MEM, err, TAG, "no mem for PID control block");

    ESP_GOTO_ON_ERROR(pid_update_parameters_f(pid, &config->init_param), err, TAG, "init PID parameters failed");
    *ret_pid = pid;
    return ret;

err:
    free(pid);
    return ret;
}

esp_err_t pid_del_control_block_f(pid_ctrl_block_f_handle_t pid)
{
    ESP_RETURN_ON_FALSE(pid, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    free(pid);
    return ESP_OK;
}

esp_err_t pid_compute_f(pid_ctrl_block_f_handle_t pid, float input_error, float *ret_result)
{
    struct pid_ctrl_block_f_t *b = (struct pid_ctrl_block_f_t *)pid;

    ESP_RETURN_ON_FALSE(b && ret_result, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    *ret_result = b->calculate_func(b, input_error);
    return ESP_OK;
}

esp_err_t pid_reset_ctrl_block_f(pid_ctrl_block_f_handle_t pid)
{
    struct pid_ctrl_block_f_t *b = (struct pid_ctrl_block_f_t *)pid;

    ESP_RETURN_ON_FALSE(b, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    b->integral_err = 0.0f;
    b->previous_err1 = 0.0f;
    b->previous_err2 = 0.0f;
    b->last_output = 0.0f;
    return ESP_OK;
}
