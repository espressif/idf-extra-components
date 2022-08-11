#pragma once 

#include "espFoC/esp_foc.h"
#include "driver/adc.h"
#include "esp_err.h"

typedef struct {
    adc_bits_width_t width;
    adc_channel_t axis_channels[4];
    int noof_axis;
}esp_foc_isensor_adc_config_t;


esp_foc_isensor_t *isensor_adc_new(esp_foc_isensor_adc_config_t *config,
                                float adc_to_current_scale);
