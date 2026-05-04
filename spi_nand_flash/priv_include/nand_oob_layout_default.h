/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2026 Espressif Systems (Shanghai) CO LTD
 */

#pragma once

#include "nand_oob_layout_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default layout matching proposal §1.2 (private; not stable public API). */
const spi_nand_oob_layout_t *nand_oob_layout_get_default(void);

#ifdef __cplusplus
}
#endif
