/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ESP_ERR_SUIT_OTA_BASE            0x1F000

/* Status, not an error: every component is staged and verified, call esp_suit_ota_commit() */
#define ESP_SUIT_OTA_READY               (ESP_ERR_SUIT_OTA_BASE + 0x01)

/* Class A: the manifest was rejected, nothing was written */
#define ESP_ERR_SUIT_OTA_AUTH            (ESP_ERR_SUIT_OTA_BASE + 0x02)   /*!< Signature, manifest digest or pre-auth framing */
#define ESP_ERR_SUIT_OTA_SEQUENCE        (ESP_ERR_SUIT_OTA_BASE + 0x03)   /*!< Sequence number below the installed one */
#define ESP_ERR_SUIT_OTA_CONDITION       (ESP_ERR_SUIT_OTA_BASE + 0x04)   /*!< Vendor, class or device mismatch, abort, try-each exhausted */
#define ESP_ERR_SUIT_OTA_UNSUPPORTED     (ESP_ERR_SUIT_OTA_BASE + 0x05)   /*!< Version, command, parameter, algorithm or extension not supported */
#define ESP_ERR_SUIT_OTA_FORMAT          (ESP_ERR_SUIT_OTA_BASE + 0x06)   /*!< Malformed CBOR inside the verified manifest */
#define ESP_ERR_SUIT_OTA_MAP             (ESP_ERR_SUIT_OTA_BASE + 0x07)   /*!< Component without a target, or a duplicate component */
#define ESP_ERR_SUIT_OTA_GROUP           (ESP_ERR_SUIT_OTA_BASE + 0x08)   /*!< The manifest updates only part of a declared group */
#define ESP_ERR_SUIT_OTA_SIZE            (ESP_ERR_SUIT_OTA_BASE + 0x09)   /*!< Size missing, larger than the target, or resource longer than declared */
#define ESP_ERR_SUIT_OTA_NO_TRANSPORT    (ESP_ERR_SUIT_OTA_BASE + 0x0A)   /*!< URI scheme without a getter, or a URI on a stream */
#define ESP_ERR_SUIT_OTA_REJECTED        (ESP_ERR_SUIT_OTA_BASE + 0x0B)   /*!< This manifest already failed on this device */
#define ESP_ERR_SUIT_OTA_SEVERED         (ESP_ERR_SUIT_OTA_BASE + 0x0C)   /*!< A referenced severed section is missing or has a bad digest */
#define ESP_ERR_SUIT_OTA_UP_TO_DATE      (ESP_ERR_SUIT_OTA_BASE + 0x0D)   /*!< Same sequence number and every component already installed */

/* Class B: the campaign was aborted, staged slots are inert, nothing was activated */
#define ESP_ERR_SUIT_OTA_DIGEST          (ESP_ERR_SUIT_OTA_BASE + 0x0E)   /*!< Payload digest mismatch */
#define ESP_ERR_SUIT_OTA_IMAGE           (ESP_ERR_SUIT_OTA_BASE + 0x0F)   /*!< App image verification failed */
#define ESP_ERR_SUIT_OTA_INCOMPLETE      (ESP_ERR_SUIT_OTA_BASE + 0x10)   /*!< Resource ended before the declared size */

/* Misuse and getter-only status */
#define ESP_ERR_SUIT_OTA_STATE           (ESP_ERR_SUIT_OTA_BASE + 0x11)   /*!< Call not allowed in this state */
#define ESP_ERR_SUIT_OTA_NOT_MODIFIED    (ESP_ERR_SUIT_OTA_BASE + 0x12)   /*!< The server answered 304 for the manifest */

/**
 * @brief Name of a component error code, or NULL when the code is not one of ours.
 */
const char *esp_suit_ota_err_to_name(esp_err_t err);

#ifdef __cplusplus
}
#endif
