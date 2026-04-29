/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Template: include from pid_ctrl.c with macros set:
 *   PFX(name)     -> e.g. name##_f, name##_iq
 *   PID_NUM_T, PID_MUL, PID_ZERO
 *   PID_CTRL_TAG, PID_CONFIG_T, PID_PARAM_T, PID_HANDLE_T
 */

struct PID_CTRL_TAG {
    PID_NUM_T kp;
    PID_NUM_T ki;
    PID_NUM_T kd;
    PID_NUM_T previous_err1;
    PID_NUM_T previous_err2;
    PID_NUM_T integral_err;
    PID_NUM_T last_output;
    PID_NUM_T max_output;
    PID_NUM_T min_output;
    PID_NUM_T max_integral;
    PID_NUM_T min_integral;
    PID_NUM_T (*calculate_func)(struct PID_CTRL_TAG *pid, PID_NUM_T error);
};

static inline PID_NUM_T PFX(_pid_clamp)(PID_NUM_T value, PID_NUM_T min_value, PID_NUM_T max_value)
{
    return MAX(min_value, MIN(value, max_value));
}

static PID_NUM_T PFX(pid_calc_positional)(struct PID_CTRL_TAG *pid, PID_NUM_T error)
{
    PID_NUM_T output;

    pid->integral_err += error;
    pid->integral_err = PFX(_pid_clamp)(pid->integral_err, pid->min_integral, pid->max_integral);

    output = PID_MUL(error, pid->kp) +
            PID_MUL(error - pid->previous_err1, pid->kd) +
            PID_MUL(pid->integral_err, pid->ki);
    output = PFX(_pid_clamp)(output, pid->min_output, pid->max_output);

    pid->previous_err1 = error;
    return output;
}

static PID_NUM_T PFX(pid_calc_incremental)(struct PID_CTRL_TAG *pid, PID_NUM_T error)
{
    PID_NUM_T output;

    output = PID_MUL(error - pid->previous_err1, pid->kp) +
            PID_MUL(error - pid->previous_err1 - pid->previous_err1 + pid->previous_err2, pid->kd) +
            PID_MUL(error, pid->ki) +
            pid->last_output;
    output = PFX(_pid_clamp)(output, pid->min_output, pid->max_output);

    pid->previous_err2 = pid->previous_err1;
    pid->previous_err1 = error;
    pid->last_output = output;
    return output;
}

esp_err_t PFX(pid_update_parameters)(PID_HANDLE_T pid, const PID_PARAM_T *params)
{
    struct PID_CTRL_TAG *b = (struct PID_CTRL_TAG *)pid;

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
        b->calculate_func = PFX(pid_calc_incremental);
        break;
    case PID_CAL_TYPE_POSITIONAL:
        b->calculate_func = PFX(pid_calc_positional);
        break;
    default:
        ESP_RETURN_ON_FALSE(false, ESP_ERR_INVALID_ARG, TAG, "invalid PID calculation type:%d", params->cal_type);
    }

    return ESP_OK;
}

esp_err_t PFX(pid_new_control_block)(const PID_CONFIG_T *config, PID_HANDLE_T *ret_pid)
{
    esp_err_t ret = ESP_OK;
    struct PID_CTRL_TAG *pid = NULL;

    ESP_GOTO_ON_FALSE(config && ret_pid, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");

    pid = calloc(1, sizeof(struct PID_CTRL_TAG));
    ESP_GOTO_ON_FALSE(pid, ESP_ERR_NO_MEM, err, TAG, "no mem for PID control block");

    ESP_GOTO_ON_ERROR(PFX(pid_update_parameters)((PID_HANDLE_T)pid, &config->init_param), err, TAG, "init PID parameters failed");
    *ret_pid = (PID_HANDLE_T)pid;
    return ret;

err:
    free(pid);
    return ret;
}

esp_err_t PFX(pid_del_control_block)(PID_HANDLE_T pid)
{
    ESP_RETURN_ON_FALSE(pid, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    free(pid);
    return ESP_OK;
}

esp_err_t PFX(pid_compute)(PID_HANDLE_T pid, PID_NUM_T input_error, PID_NUM_T *ret_result)
{
    struct PID_CTRL_TAG *b = (struct PID_CTRL_TAG *)pid;

    ESP_RETURN_ON_FALSE(b && ret_result, ESP_ERR_INVALID_ARG, TAG, "invalid argument");
    *ret_result = b->calculate_func(b, input_error);
    return ESP_OK;
}

esp_err_t PFX(pid_reset_ctrl_block)(PID_HANDLE_T pid)
{
    struct PID_CTRL_TAG *b = (struct PID_CTRL_TAG *)pid;

    ESP_RETURN_ON_FALSE(b, ESP_ERR_INVALID_ARG, TAG, "invalid argument");

    b->integral_err = PID_ZERO();
    b->previous_err1 = PID_ZERO();
    b->previous_err2 = PID_ZERO();
    b->last_output = PID_ZERO();
    return ESP_OK;
}
