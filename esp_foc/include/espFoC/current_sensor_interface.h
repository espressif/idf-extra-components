#pragma once

typedef struct {
    float iu_axis_0;
    float iv_axis_0;
    float iw_axis_0;

    float iu_axis_1;
    float iv_axis_1;
    float iw_axis_1;
} isensor_values_t;

typedef struct esp_foc_isensor_s esp_foc_isensor_t;

struct esp_foc_isensor_s {
    void (*fetch_isensors)(esp_foc_isensor_t *self, isensor_values_t *values);
    void (*sample_isensors)(esp_foc_isensor_t *self);
};