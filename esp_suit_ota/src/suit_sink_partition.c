/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdlib.h>
#include "suit_priv.h"

static const char *TAG = SUIT_TAG;

#define COPY_CHUNK          4096
#define FLASH_ENC_BLOCK     16      /* encrypted partitions are written in 16-byte blocks */
#define FLASH_ERASED_BYTE   0xFF

esp_err_t suit_sink_part_read(const esp_partition_t *p, size_t offset, void *data, size_t len)
{
    return esp_partition_read(p, offset, data, len);
}

esp_err_t suit_sink_copy_partition(const esp_partition_t *dst, const esp_partition_t *src, size_t len)
{
    if (len > dst->size || len > src->size) {
        return ESP_ERR_SUIT_OTA_SIZE;
    }
    size_t erase_len = (len + dst->erase_size - 1) / dst->erase_size * dst->erase_size;
    esp_err_t err = esp_partition_erase_range(dst, 0, erase_len);
    if (err != ESP_OK) {
        return err;
    }
    uint8_t *buf = malloc(COPY_CHUNK);
    if (buf == NULL) {
        return ESP_ERR_NO_MEM;
    }
    for (size_t off = 0; off < len && err == ESP_OK; off += COPY_CHUNK) {
        size_t n = len - off < COPY_CHUNK ? len - off : COPY_CHUNK;
        err = esp_partition_read(src, off, buf, n);
        if (err == ESP_OK) {
            size_t w = (n + FLASH_ENC_BLOCK - 1) / FLASH_ENC_BLOCK * FLASH_ENC_BLOCK;
            if (w > n) {
                memset(buf + n, FLASH_ERASED_BYTE, w - n);
            }
            err = esp_partition_write(dst, off, buf, w);
        }
    }
    free(buf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "copy %s -> %s failed: 0x%x", src->label, dst->label, err);
    }
    return err;
}
