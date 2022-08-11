#pragma once 

#include "espFoC/esp_foc.h"

esp_foc_rotor_sensor_t *rotor_sensor_as5600_new(int pin_sda,
                                                int pin_scl,
                                                int port);