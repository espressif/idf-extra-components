/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Calculate Dallas CRC8 value of a given buffer
 *
 * @param[in] init_crc Initial CRC value
 * @param[in] input Input buffer to calculate CRC value
 * @param[in] input_size Size of input buffer, in bytes
 * @return CRC8 result of the input buffer
 */
uint8_t onewire_crc8(uint8_t init_crc, uint8_t *input, size_t input_size);

#ifdef __cplusplus
}
#endif
