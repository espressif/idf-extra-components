/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "pid_ctrl.h"

/*
 * Backend helpers. Q is a decimal token (8, 16, 24, ...), pasted onto IQmath's
 * _IQNmpy. One expansion per format in pid_ctrl_iq.c; unused formats are
 * dropped by --gc-sections.
 */

#define PID_IQ_AS(_v) ((pid_iq_raw_t)(_v))

#define PID_IQ_MPY(_q, _a, _b) PID_IQ_MPY_I(_q, _a, _b)
#define PID_IQ_MPY_I(_q, _a, _b) \
    PID_IQ_AS(_IQ##_q##mpy(PID_IQ_AS(_a), PID_IQ_AS(_b)))

#define PID_IQ_CLAMP(_v, _min, _max) PID_IQ_CLAMP_I(PID_IQ_AS(_v), PID_IQ_AS(_min), PID_IQ_AS(_max))
#define PID_IQ_CLAMP_I(_v, _min, _max) ((_v) < (_min) ? (_min) : ((_v) > (_max) ? (_max) : (_v)))

#define PID_IQ_FROM_FLOAT(_q, _v) ((pid_iq_raw_t)((_v) * (float)((int32_t)1 << (_q))))
