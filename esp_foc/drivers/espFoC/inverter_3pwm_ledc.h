#pragma once 

#include "espFoC/esp_foc.h"
#include "driver/ledc.h"
#include "esp_err.h"

esp_foc_inverter_t *inverter_3pwm_ledc_new(ledc_channel_t ch_u,
                                        ledc_channel_t ch_v,
                                        ledc_channel_t ch_w,
                                        int gpio_u,
                                        int gpio_v,
                                        int gpio_w,
                                        float dc_link_voltage,
                                        int port);
