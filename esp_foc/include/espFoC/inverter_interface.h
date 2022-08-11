#pragma once

typedef void (*esp_foc_inverter_callback_t) (void *argument);

typedef struct esp_foc_inverter_s esp_foc_inverter_t;

struct esp_foc_inverter_s {
    void (*set_inverter_callback)(esp_foc_inverter_t *self,
                        esp_foc_inverter_callback_t callback,
                        void *argument);
    float (*get_dc_link_voltage)(esp_foc_inverter_t *self);
    void (*set_voltages)(esp_foc_inverter_t *self,
                        float v_u,
                        float v_v,
                        float v_w);
    void (*phase_remap)(esp_foc_inverter_t *self);
    float (*get_inverter_pwm_rate)(esp_foc_inverter_t *self);
};