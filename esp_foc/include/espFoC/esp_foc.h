#pragma once

#include <math.h>
#include "espFoC/ema_low_pass_filter.h"
#include "espFoC/foc_math.h"
#include "espFoC/modulator.h"
#include "espFoC/pid_controller.h"
#include "espFoC/inverter_interface.h"
#include "espFoC/current_sensor_interface.h"
#include "espFoC/rotor_sensor_interface.h"
#include "espFoC/os_interface.h"
#include "espFoC/esp_foc_units.h"

typedef enum {
    ESP_FOC_OK = 0,
    ESP_FOC_ERR_NOT_ALIGNED = -1,
    ESP_FOC_ERR_INVALID_ARG = -2,
    ESP_FOC_ERR_AXIS_INVALID_STATE = -3,
    ESP_FOC_ERR_ALIGNMENT_IN_PROGRESS = -4,
    ESP_FOC_ERR_TIMESTEP_TOO_SMALL = -5,
    ESP_FOC_ERR_UNKNOWN = -128
} esp_foc_err_t;

#include "espFoC/esp_foc_axis.h"

typedef enum {
    ESP_FOC_MOTOR_NATURAL_DIRECTION_CW,
    ESP_FOC_MOTOR_NATURAL_DIRECTION_CCW,
} esp_foc_motor_direction_t;

typedef struct {
    float kp;
    float ki;
    float kd;
    float integrator_limit;
    float max_output_value;
} esp_foc_control_settings_t;

typedef struct {
    esp_foc_control_settings_t torque_control_settings[2];
    esp_foc_control_settings_t velocity_control_settings;
    esp_foc_control_settings_t position_control_settings;
    int downsampling_speed_rate;
    int downsampling_position_rate;
    int motor_pole_pairs;
    int estimators_rate;
    esp_foc_motor_direction_t natural_direction;
} esp_foc_motor_control_settings_t;

typedef struct {
    esp_foc_seconds dt;
    esp_foc_u_voltage u;
    esp_foc_v_voltage v;
    esp_foc_w_voltage w;

    esp_foc_q_voltage out_q;
    esp_foc_d_voltage out_d;

    esp_foc_radians position;
    esp_foc_radians_per_second speed;
} esp_foc_control_data_t;


esp_foc_err_t esp_foc_initialize_axis(esp_foc_axis_t *axis,
                                    esp_foc_inverter_t *inverter,
                                    esp_foc_rotor_sensor_t *rotor,
                                    esp_foc_isensor_t *isensor,
                                    esp_foc_motor_control_settings_t settings);

esp_foc_err_t esp_foc_align_axis(esp_foc_axis_t *axis);

esp_foc_seconds esp_foc_get_runner_dt(esp_foc_axis_t *axis);

esp_foc_err_t esp_foc_set_target_voltage(esp_foc_axis_t *axis,
                                        esp_foc_q_voltage uq,
                                        esp_foc_d_voltage ud);                                        

esp_foc_err_t esp_foc_set_target_speed(esp_foc_axis_t *axis, esp_foc_radians_per_second speed);

esp_foc_err_t esp_foc_set_target_position(esp_foc_axis_t *axis, esp_foc_radians position);

esp_foc_err_t esp_foc_get_control_data(esp_foc_axis_t *axis, esp_foc_control_data_t *control_data);

esp_foc_err_t esp_foc_run(esp_foc_axis_t *axis);

esp_foc_err_t esp_foc_test_motor(esp_foc_inverter_t *inverter,
                                esp_foc_rotor_sensor_t *rotor,
                                esp_foc_motor_control_settings_t settings);