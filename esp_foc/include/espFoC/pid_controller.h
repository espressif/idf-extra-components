#pragma once

typedef struct {
    float kp;
    float ki; /** ki = kp ( 1 / n * Ts) */
    float kd; /** kd = kp * n * Ts */

    float integrator_limit;
    float accumulated_error;
    float previous_error;
    float max_output_value;

}esp_foc_pid_controller_t;

static inline float esp_foc_saturate(float value, float limit) 
{
    float result = value;
    if (value > limit) {
        result = limit;
    } else if (value < -limit) {
        result = -limit;
    }

    return result;
}

static inline void esp_foc_pid_reset(esp_foc_pid_controller_t *self)
{
    self->accumulated_error = 0.0f;
    self->previous_error = 0.0f;
}

static inline float  esp_foc_pid_update(esp_foc_pid_controller_t *self,
                                        float reference,
                                        float measure)
{
    float error = reference - measure;
    float error_diff = error - self->previous_error;
    self->accumulated_error += error;

    self->previous_error = error;

    if(self->accumulated_error > self->integrator_limit) {
        self->accumulated_error = self->integrator_limit;
    } else if (self->accumulated_error < -self->integrator_limit) {
        self->accumulated_error = -self->integrator_limit;
    }

    float mv = (self->kp * error) + 
            (self->ki * self->accumulated_error) + 
            (self->kd * error_diff);

    return esp_foc_saturate(mv, self->max_output_value);
}