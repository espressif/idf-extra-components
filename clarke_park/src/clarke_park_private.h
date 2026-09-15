/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <math.h>

/* glibc's math.h (linux target) does not define M_SQRT3, newlib does. */
#ifndef M_SQRT3
#define M_SQRT3 1.73205080756887719000
#endif

#define CLARKE_PARK_K1 (2.0f / 3.0f)    /*!< 2/3, the equal-amplitude (amplitude-invariant) coefficient */
#define CLARKE_PARK_K3 (1.0f / M_SQRT3) /*!< 1/sqrt(3) */
