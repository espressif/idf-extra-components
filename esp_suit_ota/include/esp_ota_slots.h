/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_partition.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Declared partition pairs with a power-safe slots record.
 *
 * Every pair declares its kind. The partition labels are optional.
 *
 *   ESP_OTA_SLOTS_AB      primary and secondary; the record says which one is live
 *   ESP_OTA_SLOTS_COPY    primary is always live, secondary is written first and copied into it
 *   ESP_OTA_SLOTS_SINGLE  primary only; written in place, which is not power safe
 *   ESP_OTA_SLOTS_APP     the OTA app partitions; otadata selects them, not this record
 *
 * Name both partitions, or name neither. A pair that names neither takes the labels from its
 * own name: "<name>_0" and "<name>_1" for AB, "<name>" and "<name>_staging" for COPY, and
 * "<name>" for SINGLE. A pair that names one partition without the other is refused.
 * The app pair names no partition, because otadata selects it.
 */

#define ESP_ERR_OTA_SLOTS_BASE           0x1F100
#define ESP_ERR_OTA_SLOTS_NOT_POWERSAFE  (ESP_ERR_OTA_SLOTS_BASE + 0x01)  /*!< A single partition cannot join a group */
#define ESP_ERR_OTA_SLOTS_NOT_FOUND      (ESP_ERR_OTA_SLOTS_BASE + 0x02)  /*!< No partition with that label, or no pair with that name */
#define ESP_ERR_OTA_SLOTS_DEGRADED       (ESP_ERR_OTA_SLOTS_BASE + 0x03)  /*!< Both record copies were unreadable */
#define ESP_ERR_OTA_SLOTS_PARTIAL_GROUP  (ESP_ERR_OTA_SLOTS_BASE + 0x04)  /*!< A flip names part of a group */
#define ESP_ERR_OTA_SLOTS_STATE          (ESP_ERR_OTA_SLOTS_BASE + 0x05)  /*!< Record not PENDING, or not initialized */
#define ESP_ERR_OTA_SLOTS_RECORD         (ESP_ERR_OTA_SLOTS_BASE + 0x06)  /*!< Record partition missing or write failed */

#define ESP_OTA_SLOTS_MAX_PAIRS          8
#define ESP_OTA_SLOTS_NAME_MAX           15   /*!< Longest logical pair name; a name that derives its labels needs room for the suffix */
#define ESP_OTA_SLOTS_APP_SLOT_NONE      0xFF /*!< Campaign without an app component */

typedef enum {
    ESP_OTA_SLOTS_NONE = 0,  /*!< Not a kind: refused in a declaration, returned by get_kind() for an unknown name */
    ESP_OTA_SLOTS_SINGLE,
    ESP_OTA_SLOTS_AB,
    ESP_OTA_SLOTS_COPY,
    ESP_OTA_SLOTS_APP,
} esp_ota_slots_kind_t;

typedef enum {
    ESP_OTA_SLOTS_CONFIRMED = 0,
    ESP_OTA_SLOTS_PENDING = 1,
} esp_ota_slots_state_t;

typedef struct {
    const char *name;            /*!< Logical name, at most ESP_OTA_SLOTS_NAME_MAX characters */
    esp_ota_slots_kind_t kind;   /*!< Required; ESP_OTA_SLOTS_NONE is refused */
    const char *primary;         /*!< Optional label of AB slot 0, of the live COPY partition, or of the SINGLE partition */
    const char *secondary;       /*!< Optional label of AB slot 1 or of the COPY staging partition; always NULL for SINGLE */
    const char *group;           /*!< Pairs with the same group flip together or not at all, may be NULL */
} esp_ota_slots_decl_t;

/**
 * @brief Register the declared pairs and read the slots record.
 *
 * Call once at boot, after nvs_flash_init() when the NVS backend is selected.
 * Returns ESP_ERR_INVALID_ARG for a declaration without a kind, for one that names a single
 * partition of a pair, for a SINGLE pair that names a second partition, for an app pair that
 * names any partition, and when a derived label does not fit 16 characters.
 * Returns ESP_ERR_OTA_SLOTS_NOT_FOUND when a label, given or derived, is not in the partition
 * table, and ESP_ERR_OTA_SLOTS_NOT_POWERSAFE when a SINGLE pair names a group.
 * Returns ESP_OK and marks the record degraded when both record copies are unreadable;
 * esp_ota_slots_ack_degraded() must then be called before any flip.
 */
esp_err_t esp_ota_slots_init(const esp_ota_slots_decl_t *decls, size_t n);

/** @brief Kind of a declared pair, or ESP_OTA_SLOTS_NONE when the name is unknown. */
esp_ota_slots_kind_t esp_ota_slots_get_kind(const char *name);

/** @brief Partition that holds the live data of a pair. NULL when the name is unknown. */
const esp_partition_t *esp_ota_slots_get_active(const char *name);

/** @brief Partition the next release of a pair is written to. SINGLE: the partition itself. */
const esp_partition_t *esp_ota_slots_get_staging(const char *name);

/** @brief Index (0 or 1) of the active slot of an AB pair; 0 for COPY, SINGLE and APP. */
uint8_t esp_ota_slots_get_active_index(const char *name);

/**
 * @brief Record a flip of the named pairs in one write, state PENDING.
 *
 * new_index[i] is the slot that becomes active for names[i], and matters for AB pairs only.
 * Pairs not named keep their index. expected_app_slot is the OTA app slot the campaign
 * activates, or ESP_OTA_SLOTS_APP_SLOT_NONE for a data-only campaign. A flip that names part
 * of a declared group is refused.
 */
esp_err_t esp_ota_slots_commit_flip(const char *const *names, const uint8_t *new_index, size_t n,
                                    uint8_t expected_app_slot, uint32_t campaign_id);

esp_err_t esp_ota_slots_get_state(esp_ota_slots_state_t *state, uint32_t *campaign_id,
                                  uint8_t *expected_app_slot);

/** @brief PENDING to CONFIRMED. */
esp_err_t esp_ota_slots_confirm(void);

/** @brief PENDING back to the previous indices, then CONFIRMED. Refused when not PENDING. */
esp_err_t esp_ota_slots_revert(void);

bool esp_ota_slots_is_degraded(void);
esp_err_t esp_ota_slots_ack_degraded(void);

/**
 * @brief ESP_OK when every member of every group touched by names[] is listed in names[].
 */
esp_err_t esp_ota_slots_check_group(const char *const *names, size_t n);

#ifdef __cplusplus
}
#endif
