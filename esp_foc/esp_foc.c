#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "espFoC/esp_foc.h"
#include "esp_attr.h"
#include "esp_log.h"

static const char * tag = "ESP_FOC";

static inline float esp_foc_ticks_to_radians_normalized(esp_foc_axis_t *axis)
{
    axis->rotor_shaft_ticks = 
        axis->rotor_sensor_driver->read_counts(axis->rotor_sensor_driver);
    
    esp_foc_critical_enter();

    axis->rotor_position = 
        axis->rotor_shaft_ticks * axis->shaft_ticks_to_radians_ratio * axis->natural_direction;

    esp_foc_critical_leave();

    return esp_foc_normalize_angle(
        esp_foc_mechanical_to_elec_angle(
            axis->rotor_position, axis->motor_pole_pairs
        )
    );
}

static inline void esp_foc_motor_speed_estimator(esp_foc_axis_t * axis)
{
    axis->accumulated_rotor_position = axis->rotor_sensor_driver->read_accumulated_counts(axis->rotor_sensor_driver) *
        axis->shaft_ticks_to_radians_ratio;

    axis->current_speed =  (axis->accumulated_rotor_position - axis->rotor_position_prev) * 
                        axis->estimators_sample_rate;
                        
    axis->rotor_position_prev = axis->accumulated_rotor_position;        
}

static inline void esp_foc_position_control_loop(esp_foc_axis_t *axis)
{
    /* position control is disabled */
    if(!axis->downsampling_position_reload_value) return;

    axis->downsampling_position--;

    if(axis->downsampling_position == 0) {
        axis->downsampling_position = axis->downsampling_position_reload_value;

        axis->target_speed = esp_foc_pid_update( &axis->position_controller,
                                            axis->target_position,
                                            axis->accumulated_rotor_position);
    }
}

static inline void esp_foc_velocity_control_loop(esp_foc_axis_t *axis)
{
    /* speed control is disabled */
    if(!axis->downsampling_speed_reload_value) return;

    axis->downsampling_speed--;

    if(axis->downsampling_speed == 0) {
        esp_foc_motor_speed_estimator(axis);

        axis->downsampling_speed = axis->downsampling_speed_reload_value;

        axis->target_i_q.raw = esp_foc_pid_update( &axis->velocity_controller,
                                            axis->target_speed,
                                            esp_foc_low_pass_filter_update(
                                            &axis->velocity_filter,
                                            axis->current_speed)) ;
        axis->target_i_d.raw = 0.0f;
    }
}


static inline void esp_foc_torque_control_loop(esp_foc_axis_t *axis)
{

    axis->u_q.raw = esp_foc_pid_update( &axis->torque_controller[0],
                                        axis->target_i_q.raw,
                                        esp_foc_low_pass_filter_update(
                                            &axis->current_filters[0], axis->i_q.raw)) +
                                            axis->target_u_q.raw;
   
    axis->u_d.raw = esp_foc_pid_update( &axis->torque_controller[1],
                                        axis->target_i_d.raw,
                                        esp_foc_low_pass_filter_update(
                                            &axis->current_filters[1], axis->i_d.raw)) +
                                            axis->target_u_d.raw;

}

IRAM_ATTR static void esp_foc_control_loop(void *arg)
{
    esp_foc_axis_t *axis = (esp_foc_axis_t *)arg;
    
    esp_foc_position_control_loop(axis);
    esp_foc_velocity_control_loop(axis);
    esp_foc_torque_control_loop(axis);

    esp_foc_modulate_dq_voltage(axis->biased_dc_link_voltage, 
                    axis->rotor_elec_angle, 
                    axis->u_d.raw, 
                    axis->u_q.raw, 
                    &axis->u_u.raw, 
                    &axis->u_v.raw, 
                    &axis->u_w.raw);

    axis->inverter_driver->set_voltages(axis->inverter_driver,
                                        axis->u_u.raw, 
                                        axis->u_v.raw, 
                                        axis->u_w.raw);

    if(axis->downsampling_estimators > 0) {
        axis->downsampling_estimators--;
        if(axis->downsampling_estimators == 0) {
            axis->downsampling_estimators = axis->downsampling_estimators_reload_val;
            esp_foc_send_notification(axis->ev_handle);
        }
    }
}

IRAM_ATTR static void esp_foc_sensors_loop(void *arg)
{
    esp_foc_axis_t *axis = (esp_foc_axis_t *)arg;
    float now;
    axis->ev_handle = esp_foc_get_event_handle();
    axis->inverter_driver->set_inverter_callback(axis->inverter_driver,
                                    esp_foc_control_loop,
                                    axis);

    ESP_LOGI(tag, "Starting foc loop task for axis: %p", axis);
    ESP_LOGI(tag, "Control loop rate [Samples/S]: %f", axis->inverter_driver->get_inverter_pwm_rate(axis->inverter_driver));
    ESP_LOGI(tag, "Speed control loop rate [Samples/S]: %f", axis->estimators_sample_rate);

    for(;;) {
        esp_foc_wait_notifier();
        now = esp_foc_now_seconds();
        axis->dt = now - axis->last_timestamp;
        axis->last_timestamp = now;
        axis->rotor_elec_angle = esp_foc_ticks_to_radians_normalized(axis); 
    }
}

esp_foc_err_t esp_foc_initialize_axis(esp_foc_axis_t *axis,
                                    esp_foc_inverter_t *inverter,
                                    esp_foc_rotor_sensor_t *rotor,
                                    esp_foc_isensor_t *isensor,
                                    esp_foc_motor_control_settings_t settings)
{
    if(axis == NULL) {
        ESP_LOGE(tag, "invalid axis object!");
        return ESP_FOC_ERR_INVALID_ARG;
    }

    if(inverter == NULL) {
        ESP_LOGE(tag, "invalid inverter driver!");
        return ESP_FOC_ERR_INVALID_ARG;
    }

    if(rotor == NULL) {
        ESP_LOGE(tag, "invalid rotor sensor driver!");
        return ESP_FOC_ERR_INVALID_ARG;
    }

    axis->inverter_driver = inverter;
    axis->rotor_sensor_driver = rotor;
    axis->isensor_driver = isensor;

    axis->dc_link_voltage = 
        axis->inverter_driver->get_dc_link_voltage(axis->inverter_driver);
    axis->biased_dc_link_voltage = axis->dc_link_voltage * 0.5;
    axis->inverter_driver->set_voltages(axis->inverter_driver, 0.0, 0.0, 0.0);
    ESP_LOGI(tag,"inverter dc-link voltage: %f[V]", axis->dc_link_voltage);

    axis->dt = 0.0;
    axis->last_timestamp = 0.0;
    axis->target_speed = 0.0;
    axis->target_position = 0.0f;
    axis->accumulated_rotor_position = 0.0f;

    axis->downsampling_speed_reload_value = 0;
    axis->downsampling_position_reload_value = 0;

    axis->i_d.raw = 0.0f;
    axis->i_q.raw = 0.0f;
    axis->u_d.raw = 0.0f;
    axis->u_q.raw = 0.0f;
    axis->target_u_d.raw = 0.0f;
    axis->target_u_q.raw = 0.0f;
    axis->target_i_d.raw = 0.0f;
    axis->target_i_q.raw = 0.0f;

    axis->position_controller.kp = settings.position_control_settings.kp;
    axis->position_controller.ki = settings.position_control_settings.ki;
    axis->position_controller.kd = settings.position_control_settings.kd;
    axis->position_controller.integrator_limit = settings.position_control_settings.integrator_limit;
    axis->position_controller.max_output_value = settings.position_control_settings.max_output_value;
    esp_foc_pid_reset(&axis->position_controller);
    axis->downsampling_position = settings.downsampling_position_rate;
    axis->downsampling_position_reload_value = settings.downsampling_position_rate;;

    axis->velocity_controller.kp = settings.velocity_control_settings.kp;
    axis->velocity_controller.ki = settings.velocity_control_settings.ki;
    axis->velocity_controller.kd = settings.velocity_control_settings.kd;
    axis->velocity_controller.integrator_limit = settings.velocity_control_settings.integrator_limit;
    axis->velocity_controller.max_output_value = settings.velocity_control_settings.max_output_value;
    esp_foc_pid_reset(&axis->velocity_controller);
    esp_foc_low_pass_filter_init(&axis->velocity_filter, 0.99f);
    axis->downsampling_speed = settings.downsampling_speed_rate;
    axis->downsampling_speed_reload_value = settings.downsampling_speed_rate;

    axis->estimators_sample_rate = axis->inverter_driver->get_inverter_pwm_rate(axis->inverter_driver) / 
        axis->downsampling_speed_reload_value;

    axis->downsampling_estimators_reload_val = 4;
    axis->downsampling_estimators = 4;

    axis->torque_controller[0].kp = 1.0f;
    axis->torque_controller[0].ki = 0.0f;
    axis->torque_controller[0].kd = 0.0f;
    axis->torque_controller[0].integrator_limit = 0.0f;
    axis->torque_controller[0].max_output_value = settings.torque_control_settings[0].max_output_value;
    esp_foc_pid_reset(&axis->torque_controller[0]);
    esp_foc_low_pass_filter_init(&axis->current_filters[0], 0.9);

    axis->torque_controller[1].kp = 1.0f;
    axis->torque_controller[1].ki = 0.0f;
    axis->torque_controller[1].kd = 0.0f;
    axis->torque_controller[1].integrator_limit = 0.0f;
    axis->torque_controller[1].max_output_value = settings.torque_control_settings[1].max_output_value;
    esp_foc_pid_reset(&axis->torque_controller[1]);
    esp_foc_low_pass_filter_init(&axis->current_filters[1], 0.9);

    axis->motor_pole_pairs = (float)settings.motor_pole_pairs;
    ESP_LOGI(tag, "Motor poler pairs: %f",  axis->motor_pole_pairs);

    axis->shaft_ticks_to_radians_ratio = ((2.0 * M_PI)) /
        axis->rotor_sensor_driver->get_counts_per_revolution(axis->rotor_sensor_driver);
    ESP_LOGI(tag, "Shaft to ticks ratio: %f", axis->shaft_ticks_to_radians_ratio);

    esp_foc_sleep_ms(250);
    axis->rotor_aligned = ESP_FOC_ERR_NOT_ALIGNED;
    axis->natural_direction = (settings.natural_direction == ESP_FOC_MOTOR_NATURAL_DIRECTION_CW) ? 
                                1.0f : -1.0f;

    return ESP_FOC_OK;
}

esp_foc_err_t esp_foc_align_axis(esp_foc_axis_t *axis)
{
    if(axis == NULL) {
        ESP_LOGE(tag, "Invalid axis object!");
        return ESP_FOC_ERR_INVALID_ARG;
    }
    if(axis->rotor_aligned != ESP_FOC_ERR_NOT_ALIGNED) {
        ESP_LOGE(tag, "This rotor was aligned already!");
        return ESP_FOC_ERR_AXIS_INVALID_STATE;
    }
    float current_ticks;
    axis->rotor_aligned = ESP_FOC_ERR_ALIGNMENT_IN_PROGRESS;
    
    ESP_LOGI(tag, "Starting to align the rotor");

    axis->inverter_driver->set_voltages(axis->inverter_driver,
                                        0.0f,
                                        0.0f,
                                        0.0f);
    esp_foc_sleep_ms(500);

    axis->inverter_driver->set_voltages(axis->inverter_driver,
                                        0.4 * axis->biased_dc_link_voltage,
                                        0.0f,
                                        0.0f);
    esp_foc_sleep_ms(500);
    current_ticks = axis->rotor_sensor_driver->read_counts(axis->rotor_sensor_driver);
    ESP_LOGI(tag, "rotor ticks offset: %f [ticks] for Coil U", current_ticks);

    axis->rotor_sensor_driver->set_to_zero(axis->rotor_sensor_driver);
    axis->rotor_aligned = ESP_FOC_OK;
    ESP_LOGI(tag, "Done, rotor aligned!");
    return ESP_FOC_OK;
}

IRAM_ATTR esp_foc_err_t esp_foc_set_target_voltage(esp_foc_axis_t *axis,
                                        esp_foc_q_voltage uq,
                                        esp_foc_d_voltage ud)
{
    if(axis == NULL) {
        ESP_LOGE(tag, "invalid axis object!");
        return ESP_FOC_ERR_INVALID_ARG;
    }
    if(axis->rotor_aligned != ESP_FOC_OK) {
        ESP_LOGE(tag, "align rotor first!");
        return ESP_FOC_ERR_AXIS_INVALID_STATE;
    }
    esp_foc_critical_enter();

    axis->target_u_q = uq;
    axis->target_u_d = ud;

    esp_foc_critical_leave();
    
    return ESP_FOC_OK;
}

IRAM_ATTR esp_foc_err_t esp_foc_set_target_speed(esp_foc_axis_t *axis, 
                                                esp_foc_radians_per_second speed)
{
    if(axis == NULL) {
        ESP_LOGE(tag, "invalid axis object!");
        return ESP_FOC_ERR_INVALID_ARG;
    }
    if(axis->rotor_aligned != ESP_FOC_OK) {
        ESP_LOGE(tag, "align rotor first!");
        return ESP_FOC_ERR_AXIS_INVALID_STATE;
    }

    esp_foc_critical_enter();
    axis->target_speed = speed.raw;
    esp_foc_critical_leave();
    
    return ESP_FOC_OK;
}

IRAM_ATTR esp_foc_err_t esp_foc_set_target_position(esp_foc_axis_t *axis, 
                                                    esp_foc_radians position)
{
    if(axis == NULL) {
        ESP_LOGE(tag, "invalid axis object!");
        return ESP_FOC_ERR_INVALID_ARG;
    }
    if(axis->rotor_aligned != ESP_FOC_OK) {
        ESP_LOGE(tag, "align rotor first!");
        return ESP_FOC_ERR_AXIS_INVALID_STATE;
    }

    esp_foc_critical_enter();
    axis->target_position = position.raw;
    esp_foc_critical_leave();

    return ESP_FOC_OK;
}

esp_foc_err_t esp_foc_run(esp_foc_axis_t *axis)
{
    if(axis == NULL) {
        ESP_LOGE(tag, "invalid axis object!");
        return ESP_FOC_ERR_INVALID_ARG;
    }
    if(axis->rotor_aligned != ESP_FOC_OK) {
        ESP_LOGE(tag, "align rotor first!");
        return ESP_FOC_ERR_AXIS_INVALID_STATE;
    }

    int ret = esp_foc_create_runner(esp_foc_sensors_loop, axis, CONFIG_FOC_TASK_PRIORITY);

    if (ret < 0) {
        ESP_LOGE(tag, "Check os interface, the runner creation has failed!");
        return ESP_FOC_ERR_UNKNOWN;
    }

    return ESP_FOC_OK;
}

esp_foc_err_t esp_foc_test_motor(esp_foc_inverter_t *inverter,
                                esp_foc_rotor_sensor_t *rotor,
                                esp_foc_motor_control_settings_t settings)
{
    if(inverter == NULL) {
        ESP_LOGE(tag, "invalid inverter driver!");
        return ESP_FOC_ERR_INVALID_ARG;
    }

    if(rotor == NULL) {
        ESP_LOGE(tag, "invalid rotor sensor driver!");
        return ESP_FOC_ERR_INVALID_ARG;
    }

    ESP_LOGI(tag, "Starting motor test, check the spinning direction !");

    inverter->set_voltages(inverter,
                            0.2 * (inverter->get_dc_link_voltage(inverter) / 2) , 
                            0.0f, 
                            0.0f);
    esp_foc_sleep_ms(250);

    inverter->set_voltages(inverter,
                            0.0f,
                            0.2 * (inverter->get_dc_link_voltage(inverter) / 2) ,  
                            0.0f);
    esp_foc_sleep_ms(250);

    /* Turn motor in one direction */
    for(float i = 0.0f; i < 2 * M_PI * settings.motor_pole_pairs; i += 0.05) {
        esp_foc_u_voltage u;
        esp_foc_v_voltage v;
        esp_foc_w_voltage w;
        float electrical_angle = settings.motor_pole_pairs * rotor->read_counts(rotor) * 
                                (((2.0 * M_PI)) / rotor->get_counts_per_revolution(rotor));

        ESP_LOGI(tag, "SVM calculated angle: %f [rad] Caculated electrical angle: %f [rad]", i, electrical_angle);

        esp_foc_modulate_dq_voltage(inverter->get_dc_link_voltage(inverter) / 2, 
                i, 
                0.2 * (inverter->get_dc_link_voltage(inverter) / 2), 
                0.0, 
                &u.raw, 
                &v.raw, 
                &w.raw);

        inverter->set_voltages(inverter,
                                u.raw, 
                                v.raw, 
                                w.raw);

        esp_foc_sleep_ms(10);
    }

    /* Now in another: */
    for(float i =  2 * M_PI * settings.motor_pole_pairs; i > 0.0f; i -= 0.05) {
        esp_foc_u_voltage u;
        esp_foc_v_voltage v;
        esp_foc_w_voltage w;
        float electrical_angle = settings.motor_pole_pairs * rotor->read_counts(rotor) * 
                                (((2.0 * M_PI)) / rotor->get_counts_per_revolution(rotor));

        ESP_LOGI(tag, "SVM calculated angle: %f [rad] Caculated electrical angle: %f [rad]", i, electrical_angle);

        esp_foc_modulate_dq_voltage(inverter->get_dc_link_voltage(inverter) / 2, 
                i, 
                0.2 * (inverter->get_dc_link_voltage(inverter) / 2), 
                0.0, 
                &u.raw, 
                &v.raw, 
                &w.raw);

        inverter->set_voltages(inverter,
                                u.raw, 
                                v.raw, 
                                w.raw);

        esp_foc_sleep_ms(10);
    }

    inverter->set_voltages(inverter,
                            0.0f,
                            0.0f,  
                            0.0f);

    ESP_LOGI(tag, "Test finished keep or switch motor phases depending on resultant motion");
    return ESP_FOC_OK;
}

esp_foc_err_t esp_foc_get_control_data(esp_foc_axis_t *axis, esp_foc_control_data_t *control_data)
{
    if(axis == NULL) {
        ESP_LOGE(tag, "invalid axis object!");
        return ESP_FOC_ERR_INVALID_ARG;
    }

    if(control_data == NULL) {
        ESP_LOGE(tag, "invalid control data object!");
        return ESP_FOC_ERR_INVALID_ARG;
    }

    esp_foc_critical_enter();

    control_data->u = axis->u_u;
    control_data->v = axis->u_v;
    control_data->w = axis->u_w;

    control_data->out_q = axis->u_q;
    control_data->out_d = axis->u_d;
    control_data->dt.raw = axis->dt;

    control_data->position.raw = axis->accumulated_rotor_position;
    control_data->speed.raw = axis->current_speed;

    esp_foc_critical_leave();

    return ESP_FOC_OK;
}
