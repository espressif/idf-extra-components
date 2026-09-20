/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "clarke_park.h"
#include "clarke_park_private.h"

/*
 * Backend helpers. Q is a decimal token pasted onto _IQN() / _IQNmpy /
 * _IQNsin / _IQNcos. _IQdiv2 is the same shift for every Q.
 */

#define CLARKE_PARK_IQ_DIV2(_v) _IQdiv2(_v)

#define CLARKE_PARK_IQ_MPY(_q, _a, _b) CLARKE_PARK_IQ_MPY_I(_q, _a, _b)
#define CLARKE_PARK_IQ_MPY_I(_q, _a, _b) _IQ##_q##mpy(_a, _b)

#define CLARKE_PARK_IQ_SIN(_q, _v) CLARKE_PARK_IQ_SIN_I(_q, _v)
#define CLARKE_PARK_IQ_SIN_I(_q, _v) _IQ##_q##sin(_v)

#define CLARKE_PARK_IQ_COS(_q, _v) CLARKE_PARK_IQ_COS_I(_q, _v)
#define CLARKE_PARK_IQ_COS_I(_q, _v) _IQ##_q##cos(_v)

#define CLARKE_PARK_IQ_FROM_FLOAT(_q, _v) CLARKE_PARK_IQ_FROM_FLOAT_I(_q, _v)
#define CLARKE_PARK_IQ_FROM_FLOAT_I(_q, _v) _IQ##_q(_v)
