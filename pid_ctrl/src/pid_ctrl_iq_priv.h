/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "pid_ctrl.h"

/*
 * Backend helpers. Q is a decimal token pasted onto _IQNmpy.
 */

#define PID_IQ_MPY(_q, _a, _b) PID_IQ_MPY_I(_q, _a, _b)
#define PID_IQ_MPY_I(_q, _a, _b) _IQ##_q##mpy(_a, _b)

#define PID_IQ_CLAMP(_v, _min, _max) \
    ((_v) < (_min) ? (_min) : ((_v) > (_max) ? (_max) : (_v)))
