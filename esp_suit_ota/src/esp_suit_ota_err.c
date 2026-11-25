/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_suit_ota_err.h"

const char *esp_suit_ota_err_to_name(esp_err_t err)
{
    switch (err) {
    case ESP_SUIT_OTA_READY: return "ESP_SUIT_OTA_READY";
    case ESP_ERR_SUIT_OTA_AUTH: return "ESP_ERR_SUIT_OTA_AUTH";
    case ESP_ERR_SUIT_OTA_SEQUENCE: return "ESP_ERR_SUIT_OTA_SEQUENCE";
    case ESP_ERR_SUIT_OTA_CONDITION: return "ESP_ERR_SUIT_OTA_CONDITION";
    case ESP_ERR_SUIT_OTA_UNSUPPORTED: return "ESP_ERR_SUIT_OTA_UNSUPPORTED";
    case ESP_ERR_SUIT_OTA_FORMAT: return "ESP_ERR_SUIT_OTA_FORMAT";
    case ESP_ERR_SUIT_OTA_MAP: return "ESP_ERR_SUIT_OTA_MAP";
    case ESP_ERR_SUIT_OTA_GROUP: return "ESP_ERR_SUIT_OTA_GROUP";
    case ESP_ERR_SUIT_OTA_SIZE: return "ESP_ERR_SUIT_OTA_SIZE";
    case ESP_ERR_SUIT_OTA_NO_TRANSPORT: return "ESP_ERR_SUIT_OTA_NO_TRANSPORT";
    case ESP_ERR_SUIT_OTA_SEVERED: return "ESP_ERR_SUIT_OTA_SEVERED";
    case ESP_ERR_SUIT_OTA_UP_TO_DATE: return "ESP_ERR_SUIT_OTA_UP_TO_DATE";
    case ESP_ERR_SUIT_OTA_DIGEST: return "ESP_ERR_SUIT_OTA_DIGEST";
    case ESP_ERR_SUIT_OTA_IMAGE: return "ESP_ERR_SUIT_OTA_IMAGE";
    case ESP_ERR_SUIT_OTA_INCOMPLETE: return "ESP_ERR_SUIT_OTA_INCOMPLETE";
    case ESP_ERR_SUIT_OTA_STATE: return "ESP_ERR_SUIT_OTA_STATE";
    case ESP_ERR_SUIT_OTA_NOT_MODIFIED: return "ESP_ERR_SUIT_OTA_NOT_MODIFIED";
    case ESP_ERR_SUIT_OTA_SLOTS: return "ESP_ERR_SUIT_OTA_SLOTS";
    case ESP_ERR_SUIT_OTA_DECRYPT: return "ESP_ERR_SUIT_OTA_DECRYPT";
    default: return NULL;
    }
}
