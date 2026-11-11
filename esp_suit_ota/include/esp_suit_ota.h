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
#include "esp_event.h"
#include "esp_partition.h"
#include "esp_app_desc.h"
#include "sdkconfig.h"
#include "esp_suit_ota_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- identifiers, keys, targets ---------- */

typedef struct {
    const uint8_t *ptr;
    size_t len;
} esp_suit_ota_span_t;

#define ESP_SUIT_OTA_SPAN(lit)  { (const uint8_t *)(lit), sizeof(lit) - 1 }

/** @brief A SUIT component identifier: an array of byte strings, for example ["esp32c5", "app"]. */
typedef struct {
    const esp_suit_ota_span_t *parts;
    size_t num_parts;
} esp_suit_ota_cid_t;

#define ESP_SUIT_OTA_CID_MAX_PARTS  4
#define ESP_SUIT_OTA_UUID_LEN       16    /*!< Vendor, class and device identifiers are UUIDs */
#define ESP_SUIT_OTA_DIGEST_LEN     32    /*!< SHA-256 */
#define ESP_SUIT_OTA_KEY_POINT_LEN  65    /*!< Uncompressed P-256 public key: 0x04 || X || Y */
#define ESP_SUIT_OTA_ID_MAX         64    /*!< Component identifier as text, parts joined with '/' */
#define ESP_SUIT_OTA_EVENT_URI_MAX  128   /*!< URI carried in a component event */
#define ESP_SUIT_OTA_ETAG_MAX       64    /*!< Manifest ETag kept by a getter */

/** @brief A trusted P-256 public key as an uncompressed point (65 bytes, 0x04 || X || Y). */
typedef struct {
    const uint8_t *point;
    size_t point_len;
    const uint8_t *kid;     /*!< Optional COSE key id to pre-filter blocks, may be NULL */
    size_t kid_len;
} esp_suit_ota_key_t;

typedef enum {
    ESP_SUIT_VERIFY_DIGEST = 0,      /*!< Manifest digest only (data partitions) */
    ESP_SUIT_VERIFY_APP_IMAGE,       /*!< Digest plus esp_ota_end() app image verification */
} esp_suit_ota_verify_t;

/**
 * @brief A place bytes go when the target is not a flash partition.
 *
 * The core computes the SUIT digest over the bytes it writes, so a sink cannot misreport
 * what it received. read() may return ESP_ERR_NOT_SUPPORTED; image-match then uses the core
 * digest only. close() may return the digest the target computed, or ESP_ERR_NOT_SUPPORTED.
 */
typedef struct {
    esp_err_t (*open)(void *ctx, size_t total_size);
    esp_err_t (*write)(void *ctx, size_t offset, const void *data, size_t len);
    esp_err_t (*read)(void *ctx, size_t offset, void *data, size_t len);
    esp_err_t (*close)(void *ctx, uint8_t digest_out[ESP_SUIT_OTA_DIGEST_LEN]);
    esp_err_t (*abort)(void *ctx);
    void *ctx;
} esp_suit_ota_sink_ops_t;

typedef struct esp_suit_ota_transport esp_suit_ota_transport_t;

/**
 * @brief Read up to cap bytes of the resource the context points at.
 *
 * state is this getter's own slot: NULL on the first call of a campaign, and whatever the getter
 * stored there afterwards. close() receives the same pointer and owns freeing it.
 * @return >0 bytes read, 0 end of this resource, <0 an esp_err_t negated (for example -ESP_ERR_SUIT_OTA_NOT_MODIFIED)
 */
typedef int (*esp_suit_ota_get_fn)(esp_suit_ota_transport_t *t, void **state, uint8_t *buf, size_t cap);
typedef void (*esp_suit_ota_close_fn)(void *state);

/**
 * @brief Binds one component identifier to a target. Exactly one of partition, slots or sink is set.
 */
typedef struct {
    esp_suit_ota_cid_t id;
    esp_suit_ota_verify_t verify;
    const esp_partition_t *partition;      /*!< Staging partition written through esp_ota_begin/write/end */
    const char *slots;                     /*!< Name of an esp_ota_slots pair */
    const esp_suit_ota_sink_ops_t *sink;   /*!< Custom target */
    bool bulk_erase;                       /*!< Erase the whole target up front instead of sector by sector */
} esp_suit_ota_map_entry_t;

typedef struct {
    const esp_suit_ota_key_t *keys;
    size_t num_keys;
    const uint8_t (*vendor_ids)[ESP_SUIT_OTA_UUID_LEN];       /*!< Accepted vendor UUIDs */
    size_t num_vendor_ids;
    const uint32_t *vendor_pens;           /*!< Accepted vendor private enterprise numbers, 1.3.6.1.4.1.<pen> */
    size_t num_vendor_pens;
    const uint8_t (*class_ids)[ESP_SUIT_OTA_UUID_LEN];
    size_t num_class_ids;
    const uint8_t *device_id;              /*!< Optional 16-byte device identifier */
    const esp_suit_ota_map_entry_t *map;   /*!< NULL: ['<anything>', name] maps to the pair declared as name, 'app' to the app pair */
    size_t map_len;
    const char *text_lang;                 /*!< Language tag of the kept update description, NULL for the Kconfig default */
} esp_suit_ota_config_t;

/* ---------- transport context ---------- */

/** @brief Which registered getter a resource travels through. Chosen from the scheme of its URI. */
typedef enum {
    ESP_SUIT_MODE_HTTPS = 0,      /*!< "https" and "http" */
    ESP_SUIT_MODE_BLE,            /*!< "ble" */
    ESP_SUIT_MODE_MQTT,           /*!< "mqtt" and "mqtts" */
    ESP_SUIT_MODE_CUSTOM,         /*!< every other scheme */
} esp_suit_ota_mode_t;

typedef struct {
    esp_suit_ota_get_fn get;      /*!< NULL: this device has no such link */
    esp_suit_ota_close_fn close;
    void *cfg;                    /*!< Yours: certificates, link parameters */
    void *state;                  /*!< The getter's connection state, owned by the getter */
} esp_suit_ota_getter_t;

/**
 * @brief The links this device has, and what the running getter is told.
 *
 * You register the getters you have and set url to the manifest. From there feed() maintains it:
 * at every component it sets url to that component's URI and mode to the getter its scheme names.
 */
struct esp_suit_ota_transport {
    esp_suit_ota_getter_t https, ble, mqtt, custom;   /*!< Set by you: register what this device has */
    /* ---- destination: url is yours for the manifest, then feed() owns all of it ---- */
    esp_suit_ota_mode_t mode;     /*!< Which of the four is fetching now */
    const char *url;              /*!< Current URI; a relative one is resolved against the URL fetched before it */
    size_t offset;                /*!< Byte offset inside url: resume, or a one-file envelope */
    size_t len;                   /*!< Bytes remaining for this resource, ESP_SUIT_OTA_LEN_UNKNOWN for the manifest */
    const uint8_t *fetch_args;    /*!< The manifest's transport hints for this component */
    size_t fetch_args_len;
    size_t component;             /*!< Index of the component being fetched */
    uint32_t epoch;               /*!< Incremented on every redirect; a getter reopens when it changes */
    bool done;                    /*!< Nothing left to fetch: call esp_suit_ota_commit() */
    bool allow_not_modified;      /*!< Getter may answer -ESP_ERR_SUIT_OTA_NOT_MODIFIED (manifest only) */
    char manifest_etag[ESP_SUIT_OTA_ETAG_MAX];       /*!< A getter may leave the manifest's ETag here; kept at activation */
};

#define ESP_SUIT_OTA_LEN_UNKNOWN  SIZE_MAX

/** @brief Dispatch to the getter of the component being fetched. */
int esp_suit_ota_transport_get(esp_suit_ota_transport_t *t, uint8_t *buf, size_t cap);

/* ---------- what the manifest says ---------- */

typedef enum {
    ESP_SUIT_COMP_NEEDED = 0,
    ESP_SUIT_COMP_ALREADY_INSTALLED,
    ESP_SUIT_COMP_STAGED_VERIFIED,
} esp_suit_ota_comp_state_t;

typedef struct {
    char id[ESP_SUIT_OTA_ID_MAX]; /*!< Identifier parts joined with '/' */
    char uri[CONFIG_ESP_SUIT_OTA_URI_MAX];
    bool integrated;              /*!< Payload travels inside the envelope */
    uint32_t size;
    uint8_t digest[ESP_SUIT_OTA_DIGEST_LEN];
    esp_suit_ota_verify_t verify;
    esp_suit_ota_comp_state_t state;
    uint32_t bytes_written;
} esp_suit_ota_component_t;

typedef struct {
    uint64_t sequence_number;
    size_t num_components;
    size_t num_pending;           /*!< Components that still need bytes */
    char description[CONFIG_ESP_SUIT_OTA_TEXT_CAP];
} esp_suit_ota_info_t;

typedef struct esp_suit_ota_ctx *esp_suit_ota_handle_t;

/* ---------- the campaign ---------- */

/**
 * @brief Start a campaign. t may be NULL for a stream the transport's own engine delivers.
 *
 * Resumes from the journal when one exists for the manifest that is fed next.
 * Refuses with ESP_ERR_SUIT_OTA_STATE while the running app is PENDING_VERIFY, or while an
 * activated release waits for esp_suit_ota_confirm() or esp_suit_ota_revert().
 */
esp_err_t esp_suit_ota_begin(const esp_suit_ota_config_t *cfg, esp_suit_ota_transport_t *t,
                             esp_suit_ota_handle_t *out);

/**
 * @brief Consume bytes. Payload bytes go to their targets, boundaries are verified.
 *
 * In stream mode *used may be less than len when an integrated payload ended and the rest
 * belongs to the next envelope entry: feed the remainder again. In URL mode a resource longer
 * than its declared size is an error. len == 0 means "this resource ended".
 * @return ESP_OK continue; ESP_SUIT_OTA_READY all staged and verified; an error otherwise
 */
esp_err_t esp_suit_ota_feed(esp_suit_ota_handle_t h, const void *data, size_t len, size_t *used);

/** @brief The gate: runs the validate sequence over the staged targets. Activates nothing. */
esp_err_t esp_suit_ota_commit(esp_suit_ota_handle_t h);

/** @brief Pause: close getters, keep the journal, free the handle. begin() resumes later. */
esp_err_t esp_suit_ota_end(esp_suit_ota_handle_t h);

/** @brief Discard the campaign, its journal and the handle. */
esp_err_t esp_suit_ota_abort(esp_suit_ota_handle_t h);

esp_err_t esp_suit_ota_get_info(esp_suit_ota_handle_t h, esp_suit_ota_info_t *info);
esp_err_t esp_suit_ota_get_component(esp_suit_ota_handle_t h, size_t index, esp_suit_ota_component_t *out);

/** @brief App descriptor of an APP_IMAGE component once its first 1024 bytes arrived. */
esp_err_t esp_suit_ota_get_app_desc(esp_suit_ota_handle_t h, size_t index, esp_app_desc_t *desc);

/**
 * @brief Copy the COPY pairs, flip the slots, persist the installed record, write the boot selector.
 *
 * Call after commit() returned ESP_OK. restart_needed is true when the campaign had an app
 * component, and the caller restarts the device; a data-only campaign is live at once. The handle
 * stays open: release it with esp_suit_ota_end().
 */
esp_err_t esp_suit_ota_activate(esp_suit_ota_handle_t h, bool *restart_needed);

/** @brief After the self-test: cancel rollback and settle the slots record. */
esp_err_t esp_suit_ota_confirm(void);

/**
 * @brief Data-only release whose new data failed validation: flip back what it flipped, and record
 *        the release as rejected so it is not installed again. Needs esp_ota_slots_init() first.
 */
esp_err_t esp_suit_ota_revert(void);

/* ---------- boot time ---------- */

esp_err_t esp_suit_ota_get_installed_info(esp_suit_ota_info_t *info);

/** @brief Settle a campaign cut by power loss from its journal. Call once at boot after esp_ota_slots_init(). */
esp_err_t esp_suit_ota_recover(void);

/* ---------- events ---------- */

ESP_EVENT_DECLARE_BASE(ESP_SUIT_OTA_EVENT);

typedef enum {
    ESP_SUIT_OTA_EVENT_MANIFEST_VERIFIED,   /*!< esp_suit_ota_event_manifest_t */
    ESP_SUIT_OTA_EVENT_COMPONENT_NEXT,      /*!< esp_suit_ota_event_component_t */
    ESP_SUIT_OTA_EVENT_COMPONENT_SKIPPED,   /*!< esp_suit_ota_event_component_t */
    ESP_SUIT_OTA_EVENT_COMPONENT_HEADER,    /*!< esp_suit_ota_event_header_t */
    ESP_SUIT_OTA_EVENT_PROGRESS,            /*!< esp_suit_ota_event_progress_t */
    ESP_SUIT_OTA_EVENT_COMPONENT_STAGED,    /*!< esp_suit_ota_event_component_t */
    ESP_SUIT_OTA_EVENT_READY,               /*!< no data */
    ESP_SUIT_OTA_EVENT_ACTIVATED,           /*!< esp_suit_ota_event_manifest_t */
    ESP_SUIT_OTA_EVENT_ABORTED,             /*!< esp_suit_ota_event_abort_t */
    ESP_SUIT_OTA_EVENT_CONNECTED,           /*!< posted by the HTTPS getter, no data */
} esp_suit_ota_event_id_t;

typedef struct {
    uint64_t sequence_number;
    uint8_t num_components;
    uint8_t num_pending;
    char description[CONFIG_ESP_SUIT_OTA_TEXT_CAP];
} esp_suit_ota_event_manifest_t;

typedef struct {
    uint8_t index;
    char id[ESP_SUIT_OTA_ID_MAX];
    char uri[ESP_SUIT_OTA_EVENT_URI_MAX];
    uint32_t size;
} esp_suit_ota_event_component_t;

typedef struct {
    uint8_t index;
    uint16_t chip_id;
    uint16_t min_chip_rev_full;
    esp_app_desc_t app_desc;
} esp_suit_ota_event_header_t;

typedef struct {
    uint8_t index;
    uint32_t bytes_written;
    uint32_t size;
} esp_suit_ota_event_progress_t;

typedef struct {
    esp_err_t err;
    uint8_t phase;
    uint8_t component;
} esp_suit_ota_event_abort_t;

#ifdef __cplusplus
}
#endif
