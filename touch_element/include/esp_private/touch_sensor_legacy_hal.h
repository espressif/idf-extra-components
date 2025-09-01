/*
 * SPDX-FileCopyrightText: 2015-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "esp_private/touch_sensor_legacy_ll.h"
#include "touch_element/touch_sensor_legacy_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    touch_high_volt_t refh;
    touch_low_volt_t refl;
    touch_volt_atten_t atten;
} touch_hal_volt_t;

typedef struct {
    touch_cnt_slope_t slope;    /*!<Set touch sensor charge/discharge speed(currents) for each pad.*/
    touch_tie_opt_t tie_opt;    /*!<Set initial voltage state of touch channel for each measurement.*/
} touch_hal_meas_mode_t;

/**
 * Set touch sensor high / low voltage threshold of charge.
 * The touch sensor measures the channel capacitance value by charging and discharging the channel.
 * So charge threshold should be less than the supply voltage.
 * The actual charge threshold is high voltage threshold minus attenuation value.
 *
 * @param refh The high voltage threshold of charge.
 */
void touch_hal_set_voltage(const touch_hal_volt_t *volt);

/**
 * Get touch sensor high / low voltage threshold of charge.
 * The touch sensor measures the channel capacitance value by charging and discharging the channel.
 * So charge threshold should be less than the supply voltage.
 * The actual charge threshold is high voltage threshold minus attenuation value.
 *
 * @param refh The voltage threshold of charge / discharge.
 */
void touch_hal_get_voltage(touch_hal_volt_t *volt);

/**
 * Set touch sensor charge/discharge speed(currents) and initial voltage state for each pad measurement.
 *
 * @param touch_num Touch pad index.
 * @param meas Touch pad measurement config.
 */
void touch_hal_set_meas_mode(touch_pad_t touch_num, const touch_hal_meas_mode_t *meas);

/**
 * Get touch sensor charge/discharge speed(currents) and initial voltage state for each pad measurement.
 *
 * @param touch_num Touch pad index.
 * @param meas Touch pad measurement config.
 */
void touch_hal_get_meas_mode(touch_pad_t touch_num, touch_hal_meas_mode_t *meas);

/**
 * Initialize touch module.
 *
 * @note  If default parameter don't match the usage scenario, it can be changed after this function.
 */
void touch_hal_init(void);

/**
 * Un-install touch pad driver.
 *
 * @note  After this function is called, other touch functions are prohibited from being called.
 */
void touch_hal_deinit(void);

/**
 * Configure touch sensor for each channel.
 */
void touch_hal_config(touch_pad_t touch_num);

/************************ Filter register setting ************************/

/**
 * Set parameter of touch sensor filter and detection algorithm.
 * For more details on the detection algorithm, please refer to the application documentation.
 *
 * @param filter_info select filter type and threshold of detection algorithm
 */
void touch_hal_filter_set_config(const touch_filter_config_t *filter_info);

/**
 * Get parameter of touch sensor filter and detection algorithm.
 * For more details on the detection algorithm, please refer to the application documentation.
 *
 * @param filter_info select filter type and threshold of detection algorithm
 */
void touch_hal_filter_get_config(touch_filter_config_t *filter_info);


/************************ Denoise register setting ************************/

/**
 * set parameter of denoise pad (TOUCH_PAD_NUM0).
 *        T0 is an internal channel that does not have a corresponding external GPIO.
 *        T0 will work simultaneously with the measured channel Tn. Finally, the actual
 *        measured value of Tn is the value after subtracting lower bits of T0.
 *        This denoise function filters out interference introduced on all channels,
 *        such as noise introduced by the power supply and external EMI.
 *
 * @param denoise parameter of denoise
 */
void touch_hal_denoise_set_config(const touch_pad_denoise_t *denoise);

/**
 * @brief get parameter of denoise pad (TOUCH_PAD_NUM0).
 *
 * @param denoise Pointer to parameter of denoise
 */
void touch_hal_denoise_get_config(touch_pad_denoise_t *denoise);

/**
 * Enable denoise function.
 * T0 is an internal channel that does not have a corresponding external GPIO.
 * T0 will work simultaneously with the measured channel Tn. Finally, the actual
 * measured value of Tn is the value after subtracting lower bits of T0.
 * This denoise function filters out interference introduced on all channels,
 * such as noise introduced by the power supply and external EMI.
 */
void touch_hal_denoise_enable(void);


/************************ Waterproof register setting ************************/

/**
 * Set parameter of waterproof function.
 *
 * The waterproof function includes a shielded channel (TOUCH_PAD_NUM14) and a guard channel.
 * Guard pad is used to detect the large area of water covering the touch panel.
 * Shield pad is used to shield the influence of water droplets covering the touch panel.
 * It is generally designed as a grid and is placed around the touch buttons.
 *
 * @param waterproof parameter of waterproof
 */
void touch_hal_waterproof_set_config(const touch_pad_waterproof_t *waterproof);

/**
 * Get parameter of waterproof function.
 *
 * @param waterproof parameter of waterproof.
 */
void touch_hal_waterproof_get_config(touch_pad_waterproof_t *waterproof);

/**
 * Enable parameter of waterproof function.
 * Should be called after function ``touch_hal_waterproof_set_config``.
 */
void touch_hal_waterproof_enable(void);

/************************ Proximity register setting ************************/

/**
 * Enable/disable proximity function of touch channels.
 * The proximity sensor measurement is the accumulation of touch channel measurements.
 *
 * @note Supports up to three touch channels configured as proximity sensors.
 * @param touch_num touch pad index
 * @param enabled true: enable the proximity function; false:  disable the proximity function
 * @return
 *     - true: Configured correctly.
 *     - false: Configured error.
 */
bool touch_hal_enable_proximity(touch_pad_t touch_num, bool enabled);


/************** sleep pad setting ***********************/

/**
 * Get parameter of touch sensor sleep channel.
 * The touch sensor can works in sleep mode to wake up sleep.
 * After the sleep channel is configured, users should query the channel reading using a specific function.
 *
 * @param slp_config Point to touch sleep pad config.
 */
void touch_hal_sleep_channel_get_config(touch_pad_sleep_channel_t *slp_config);

/**
 * Set parameter of touch sensor sleep channel.
 * The touch sensor can works in sleep mode to wake up sleep.
 * After the sleep channel is configured, users should query the channel reading using a specific function.
 *
 * @note ESP32S2 only support one channel to be set sleep channel.
 *
 * @param pad_num touch sleep pad number.
 * @param enable Enable/disable sleep pad function.
 */
void touch_hal_sleep_channel_enable(touch_pad_t pad_num, bool enable);

/**
 * Change the operating frequency of touch pad in deep sleep state. Reducing the operating frequency can effectively reduce power consumption.
 * If this function is not called, the working frequency of touch in the deep sleep state is the same as that in the wake-up state.
 *
 * @param sleep_cycle The touch sensor will sleep after each measurement.
 *                    sleep_cycle decide the interval between each measurement.
 *                    t_sleep = sleep_cycle / (RTC_SLOW_CLK frequency).
 *                    The approximate frequency value of RTC_SLOW_CLK can be obtained using rtc_clk_slow_freq_get_hz function.
 * @param meas_times The times of charge and discharge in each measure process of touch channels.
 *                  The timer frequency is 8Mhz. Range: 0 ~ 0xffff.
 *                  Recommended typical value: Modify this value to make the measurement time around 1ms.
 */
void touch_hal_sleep_channel_set_work_time(uint16_t sleep_cycle, uint16_t meas_times);

/**
 * Get the operating frequency of touch pad in deep sleep state. Reducing the operating frequency can effectively reduce power consumption.
 *
 * @param sleep_cycle The touch sensor will sleep after each measurement.
 *                    sleep_cycle decide the interval between each measurement.
 *                    t_sleep = sleep_cycle / (RTC_SLOW_CLK frequency).
 *                    The approximate frequency value of RTC_SLOW_CLK can be obtained using rtc_clk_slow_freq_get_hz function.
 * @param meas_times The times of charge and discharge in each measure process of touch channels.
 *                  The timer frequency is 8Mhz. Range: 0 ~ 0xffff.
 *                  Recommended typical value: Modify this value to make the measurement time around 1ms.
 */
void touch_hal_sleep_channel_get_work_time(uint16_t *sleep_cycle, uint16_t *meas_times);

#ifdef __cplusplus
}
#endif
