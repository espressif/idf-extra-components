#pragma once 

typedef struct esp_foc_rotor_sensor_s esp_foc_rotor_sensor_t;

struct esp_foc_rotor_sensor_s{
    void  (*set_to_zero) (esp_foc_rotor_sensor_t *self);
    float (*get_counts_per_revolution) (esp_foc_rotor_sensor_t *self);
    float (*read_counts) (esp_foc_rotor_sensor_t *self);
    float (*read_accumulated_counts) (esp_foc_rotor_sensor_t *self);
    void  (*set_simulation_count)(esp_foc_rotor_sensor_t *self, float increment);
};