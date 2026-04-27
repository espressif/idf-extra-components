/*
 * SPDX-FileCopyrightText: 2015-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>
#include <stdlib.h>
#include <sys/param.h>
#include "esp_check.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "pid_ctrl.h"

static const char *TAG = "pid_ctrl";

#define PFX(name) name##_f
#define PID_NUM_T float
#define PID_MUL(a, b) ((a) * (b))
#define PID_ZERO() 0.0f
#define PID_CTRL_TAG pid_ctrl_block_f_t
#define PID_CONFIG_T pid_ctrl_config_f_t
#define PID_PARAM_T pid_ctrl_parameter_f_t
#define PID_HANDLE_T pid_ctrl_block_f_handle_t
#include "pid_ctrl_impl.inc"
#undef PFX
#undef PID_NUM_T
#undef PID_MUL
#undef PID_ZERO
#undef PID_CTRL_TAG
#undef PID_CONFIG_T
#undef PID_PARAM_T
#undef PID_HANDLE_T

#define PFX(name) name##_iq
#define PID_NUM_T _iq
#define PID_MUL(a, b) _IQmpy((a), (b))
#define PID_ZERO() _IQ(0.0f)
#define PID_CTRL_TAG pid_ctrl_block_iq_t
#define PID_CONFIG_T pid_ctrl_config_iq_t
#define PID_PARAM_T pid_ctrl_parameter_iq_t
#define PID_HANDLE_T pid_ctrl_block_iq_handle_t
#include "pid_ctrl_impl.inc"
#undef PFX
#undef PID_NUM_T
#undef PID_MUL
#undef PID_ZERO
#undef PID_CTRL_TAG
#undef PID_CONFIG_T
#undef PID_PARAM_T
#undef PID_HANDLE_T
