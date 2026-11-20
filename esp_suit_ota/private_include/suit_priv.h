/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "sdkconfig.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "psa/crypto.h"
#include "esp_suit_ota.h"
#include "esp_ota_slots.h"
#include "suit_cbor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SUIT_TAG                "esp_suit_ota"
#define SUIT_MAX_COMP           CONFIG_ESP_SUIT_OTA_MAX_COMPONENTS
#define SUIT_DIGEST_LEN         ESP_SUIT_OTA_DIGEST_LEN
#define SUIT_UUID_LEN           ESP_SUIT_OTA_UUID_LEN
#define SUIT_ID_MAX             ESP_SUIT_OTA_ID_MAX
#define SUIT_P256_POINT_LEN     ESP_SUIT_OTA_KEY_POINT_LEN
#define SUIT_P256_POINT_UNCOMPRESSED 0x04  /* first byte of an uncompressed point */
#define SUIT_P256_KEY_BITS      256
#define SUIT_P256_SIG_LEN       64     /* COSE ECDSA signature: r || s, 32 bytes each */
#define SUIT_SHA256_BLOCK_LEN   64
#define SUIT_DIGEST_ITEMS       2      /* SUIT_Digest = [algorithm, bytes] */
#define SUIT_SEQ_ITEMS_PER_CMD  2      /* a command sequence is command, argument pairs */
#define SUIT_TRY_EACH_MAX       16     /* branches one try-each may hold */
#define SUIT_INSTALLED_ID_MAX   32     /* component identifier kept in the installed record (NVS) */
#define SUIT_INTEGRATED_KEY_MAX 64     /* text key of an integrated payload in the envelope */
#define SUIT_COMPONENT_NONE     0xFF   /* no component is current */
#define SUIT_APP_SLOT_UNKNOWN   0xFE   /* the running app is not in an OTA slot */
#define SUIT_JOURNAL_STEP_BYTES ((uint32_t)CONFIG_ESP_SUIT_OTA_JOURNAL_KB * 1024)
#define SUIT_URI_MAX            CONFIG_ESP_SUIT_OTA_URI_MAX
#define SUIT_MANIFEST_VERSION   1
#define SUIT_ENVELOPE_TAG       107
#define SUIT_ENVELOPE_MIN_ENTRIES 2    /* authentication wrapper and manifest */
#define SUIT_FRAME_SKIP_ITEMS_MAX 0x100000 /* nested items an unknown envelope entry may hold */
#define SUIT_COSE_SIGN1_TAG     18
#define SUIT_COSE_SIGN1_ITEMS   4      /* [protected, unprotected, payload, signature] */
#define SUIT_COSE_SIG_STRUCTURE_ITEMS 4 /* [context, protected, external_aad, payload] */
#define SUIT_COSE_HDR_ALG       1      /* COSE header label: algorithm */
#define SUIT_COSE_HDR_KID       4      /* COSE header label: key identifier */
#define SUIT_AUTH_MIN_ITEMS     2      /* the manifest digest and at least one signature block */
#define SUIT_COSE_ALG_ES256     (-7)
#define SUIT_COSE_ALG_ESP256    (-9)
#define SUIT_DIGEST_ALG_SHA256  (-16)
#define SUIT_CBOR_TAG_PEN       112   /* RFC 9090 relative OID under the IANA PEN arc, 1.3.6.1.4.1 */
#define SUIT_OID_ARC_BITS       7      /* RFC 9090: each byte of an arc carries 7 bits */
#define SUIT_OID_ARC_VALUE_MASK 0x7F
#define SUIT_OID_ARC_MORE       0x80   /* set on every byte of an arc except the last */
#define SUIT_HEADER_PROBE_LEN   1024   /* bytes of an app or bootloader image checked before the rest is written */

/* envelope keys */
#define SUIT_ENV_DELEGATION     1
#define SUIT_ENV_AUTH           2
#define SUIT_ENV_MANIFEST       3
#define SUIT_ENV_PAYLOAD_FETCH  16
#define SUIT_ENV_INSTALL        20
#define SUIT_ENV_TEXT           23

/* manifest keys */
#define SUIT_MAN_VERSION        1
#define SUIT_MAN_SEQUENCE       2
#define SUIT_MAN_COMMON         3
#define SUIT_MAN_REFERENCE_URI  4
#define SUIT_MAN_COMPONENT_ID   5
#define SUIT_MAN_VALIDATE       7
#define SUIT_MAN_LOAD           8
#define SUIT_MAN_INVOKE         9
#define SUIT_MAN_COSWID         14
#define SUIT_MAN_DEP_RESOLUTION 15
#define SUIT_MAN_PAYLOAD_FETCH  16
#define SUIT_MAN_CANDIDATE_VERIFICATION 18
#define SUIT_MAN_INSTALL        20
#define SUIT_MAN_TEXT           23
#define SUIT_MAN_UNINSTALL      24

/* common keys */
#define SUIT_COMMON_DEPENDENCIES 1
#define SUIT_COMMON_COMPONENTS  2
#define SUIT_COMMON_SHARED_SEQ  4

/* commands */
#define SUIT_CMD_COND_VENDOR    1
#define SUIT_CMD_COND_CLASS     2
#define SUIT_CMD_COND_IMAGE_MATCH 3
#define SUIT_CMD_COND_SLOT      5
#define SUIT_CMD_COND_CHECK_CONTENT 6
#define SUIT_CMD_SET_INDEX      12
#define SUIT_CMD_COND_ABORT     14
#define SUIT_CMD_TRY_EACH       15
#define SUIT_CMD_WRITE          18
#define SUIT_CMD_OVERRIDE       20
#define SUIT_CMD_FETCH          21
#define SUIT_CMD_COPY           22
#define SUIT_CMD_INVOKE         23
#define SUIT_CMD_COND_DEVICE    24
#define SUIT_CMD_SWAP           31
#define SUIT_CMD_RUN_SEQUENCE   32

/* parameters */
#define SUIT_PARAM_VENDOR       1
#define SUIT_PARAM_CLASS        2
#define SUIT_PARAM_DIGEST       3
#define SUIT_PARAM_SLOT         5
#define SUIT_PARAM_STRICT_ORDER 12
#define SUIT_PARAM_SOFT_FAILURE 13
#define SUIT_PARAM_SIZE         14
#define SUIT_PARAM_CONTENT      18
#define SUIT_PARAM_URI          21
#define SUIT_PARAM_SOURCE_COMP  22
#define SUIT_PARAM_INVOKE_ARGS  23
#define SUIT_PARAM_DEVICE       24
#define SUIT_PARAM_FETCH_ARGS   25

/* text keys */
#define SUIT_TEXT_UPDATE_DESCRIPTION 2

/* ---------- crypto ---------- */

typedef struct {
    psa_hash_operation_t op;
    bool active;
} suit_hash_t;

esp_err_t suit_hash_start(suit_hash_t *h);
esp_err_t suit_hash_update(suit_hash_t *h, const void *data, size_t len);
esp_err_t suit_hash_finish(suit_hash_t *h, uint8_t out[SUIT_DIGEST_LEN]);
void suit_hash_abort(suit_hash_t *h);
esp_err_t suit_sha256(const void *data, size_t len, uint8_t out[SUIT_DIGEST_LEN]);
esp_err_t suit_ecdsa_p256_verify(const uint8_t *point, size_t point_len, const uint8_t hash[SUIT_DIGEST_LEN],
                                 const uint8_t sig[SUIT_P256_SIG_LEN]);

/* ---------- cose ---------- */

esp_err_t suit_cose_verify(const uint8_t *auth, size_t auth_len,
                           const uint8_t *manifest_bstr, size_t manifest_bstr_len,
                           const esp_suit_ota_key_t *keys, size_t num_keys,
                           uint8_t manifest_digest[SUIT_DIGEST_LEN]);

/* ---------- parsed manifest ---------- */

typedef struct {
    const uint8_t *ptr;
    size_t len;
} suit_span_t;

typedef struct {
    bool present;
    bool severed;                  /* value is a SUIT_Digest; bytes come from the envelope */
    bool have_bytes;
    uint8_t digest[SUIT_DIGEST_LEN];
    suit_span_t bytes;             /* the bstr content (the command sequence or text map) */
} suit_section_t;

typedef struct {
    uint64_t sequence_number;
    size_t num_components;
    esp_suit_ota_span_t cid_parts[SUIT_MAX_COMP][ESP_SUIT_OTA_CID_MAX_PARTS];
    size_t cid_num_parts[SUIT_MAX_COMP];
    suit_span_t shared_seq;
    suit_span_t validate;
    suit_section_t payload_fetch;
    suit_section_t install;
    suit_section_t text;
    suit_span_t text_inline;       /* text map when it is inside the manifest */
} suit_manifest_t;

esp_err_t suit_manifest_parse(const uint8_t *manifest, size_t len, suit_manifest_t *out);
esp_err_t suit_text_description(const uint8_t *text_map, size_t len, const char *lang, char *out, size_t cap);

/* ---------- component table ---------- */

typedef enum {
    SUIT_COMP_NONE = 0,
    SUIT_COMP_INSTALLED,       /* live target already carries the digest */
    SUIT_COMP_PENDING,         /* needs bytes */
    SUIT_COMP_WRITING,         /* bytes arriving */
    SUIT_COMP_STAGED,          /* last byte landed, digest verified */
} suit_comp_state_t;

typedef struct {
    const esp_suit_ota_map_entry_t *entry;   /* NULL for default mapping through slots */
    char slots[ESP_OTA_SLOTS_NAME_MAX + 1]; /* pair name when mapped through esp_ota_slots */
    const esp_partition_t *staging;          /* partition written now */
    const esp_partition_t *active;           /* partition that holds the installed version */
    const esp_suit_ota_sink_ops_t *sink;
    esp_suit_ota_verify_t verify;
    uint8_t slot_index;                      /* index the component occupies after activation */
    bool is_app;
    bool bulk_erase;
} suit_target_t;

typedef struct {
    suit_comp_state_t state;
    char id[SUIT_ID_MAX];
    char uri[SUIT_URI_MAX];
    bool has_uri;
    bool integrated;
    uint8_t digest[SUIT_DIGEST_LEN];
    bool has_digest;
    uint32_t size;
    bool has_size;
    const uint8_t *fetch_args;
    size_t fetch_args_len;
    suit_target_t target;
    uint32_t bytes_written;
    uint32_t ota_handle;                     /* esp_ota_handle_t of the open write session */
    bool sink_open;
    bool header_checked;
    uint8_t header[SUIT_HEADER_PROBE_LEN];   /* first bytes of an app or bootloader image */
    size_t header_len;
    esp_app_desc_t app_desc;
    bool has_app_desc;
} suit_component_t;

/* ---------- interpreter ---------- */

typedef enum { SUIT_EXEC_PLAN = 0, SUIT_EXEC_VALIDATE } suit_exec_mode_t;

struct esp_suit_ota_ctx;

esp_err_t suit_exec_plan(struct esp_suit_ota_ctx *ctx);
esp_err_t suit_exec_validate(struct esp_suit_ota_ctx *ctx);
esp_err_t suit_exec_check_sequence_shape(struct esp_suit_ota_ctx *ctx, const suit_span_t *seq, bool is_shared);

/* ---------- map ---------- */

esp_err_t suit_map_resolve(struct esp_suit_ota_ctx *ctx, size_t index);
esp_err_t suit_map_check_groups(struct esp_suit_ota_ctx *ctx);

/* ---------- stream and sinks ---------- */

esp_err_t suit_stream_open(struct esp_suit_ota_ctx *ctx, size_t index, size_t resume_offset);
esp_err_t suit_stream_write(struct esp_suit_ota_ctx *ctx, size_t index, const uint8_t *data, size_t len);
esp_err_t suit_stream_close(struct esp_suit_ota_ctx *ctx, size_t index);
esp_err_t suit_stream_abort(struct esp_suit_ota_ctx *ctx, size_t index);
esp_err_t suit_stream_digest_of_target(struct esp_suit_ota_ctx *ctx, size_t index, bool staging,
                                       uint8_t out[SUIT_DIGEST_LEN]);

/* partition sink: the staging partition written through esp_ota_begin/write/end */
esp_err_t suit_sink_part_open(suit_component_t *c, size_t resume_offset);
esp_err_t suit_sink_part_write(suit_component_t *c, const uint8_t *data, size_t len);
esp_err_t suit_sink_part_close(suit_component_t *c);
esp_err_t suit_sink_part_abort(suit_component_t *c);
esp_err_t suit_sink_part_read(const esp_partition_t *p, size_t offset, void *data, size_t len);
esp_err_t suit_sink_check_header(suit_component_t *c);
esp_err_t suit_sink_copy_partition(const esp_partition_t *dst, const esp_partition_t *src, size_t len);

/* ---------- persistence ---------- */

typedef enum {
    SUIT_PHASE_NONE = 0,
    SUIT_PHASE_STAGING,
    SUIT_PHASE_READY,
    SUIT_PHASE_ACTIVATING,
    SUIT_PHASE_ACTIVATED,
} suit_phase_t;

typedef struct {
    char slots[ESP_OTA_SLOTS_NAME_MAX + 1];
    uint8_t kind;              /* esp_ota_slots_kind_t, 0 for a custom sink or a plain partition */
    uint8_t new_index;
    uint8_t state;             /* suit_comp_state_t */
    uint8_t is_app;
    uint32_t size;
    uint8_t digest[SUIT_DIGEST_LEN];
} suit_journal_comp_t;

typedef struct {
    uint32_t magic;
    uint8_t phase;
    uint8_t num;
    uint8_t current;           /* component being written */
    uint8_t app_slot;          /* ESP_OTA_SLOTS_APP_SLOT_NONE when no app component */
    uint32_t campaign_id;
    uint32_t bytes_written;
    uint64_t sequence_number;
    uint8_t manifest_digest[SUIT_DIGEST_LEN];
    suit_journal_comp_t comp[SUIT_MAX_COMP];
} suit_journal_t;

typedef struct {
    uint32_t magic;
    uint64_t sequence_number;
    uint8_t num;
    char description[CONFIG_ESP_SUIT_OTA_TEXT_CAP];
    struct {
        char id[SUIT_INSTALLED_ID_MAX];
        uint8_t digest[SUIT_DIGEST_LEN];
    } comp[SUIT_MAX_COMP];
} suit_installed_t;

esp_err_t suit_persist_journal_read(suit_journal_t *j);
esp_err_t suit_persist_journal_write(const suit_journal_t *j);
esp_err_t suit_persist_journal_erase(void);
esp_err_t suit_persist_installed_read(suit_installed_t *r);
esp_err_t suit_persist_installed_write(const suit_installed_t *r);
bool suit_persist_is_rejected(const uint8_t digest[SUIT_DIGEST_LEN]);
esp_err_t suit_persist_add_rejected(const uint8_t digest[SUIT_DIGEST_LEN]);
esp_err_t suit_persist_etag_read(char *out, size_t cap);
esp_err_t suit_persist_etag_write(const char *etag);
uint64_t suit_persist_installed_sequence(void);

/* ---------- framer ---------- */

typedef enum {
    SUIT_FR_START = 0,
    SUIT_FR_KEY,
    SUIT_FR_TSTR_KEY,
    SUIT_FR_VALUE_HEAD,
    SUIT_FR_COLLECT,          /* copying a bstr value into the arena */
    SUIT_FR_HASH_SKIP,        /* hashing a value we do not keep (text) */
    SUIT_FR_SKIP,             /* discarding an unknown value */
    SUIT_FR_PAYLOAD,          /* integrated payload bytes into a sink */
    SUIT_FR_DONE,             /* envelope consumed; URL payloads may follow */
    SUIT_FR_URL,              /* raw payload bytes from the transport */
    SUIT_FR_READY,
    SUIT_FR_FAILED,
} suit_frame_state_t;

typedef struct {
    suit_frame_state_t state;
    uint8_t head[SUIT_CBOR_HEAD_MAX];
    uint8_t head_len;
    uint64_t remaining;        /* bytes left in the value being collected, hashed, skipped or written */
    size_t entries_left;       /* envelope map entries */
    int64_t key;
    int64_t last_int_key;
    char tkey[SUIT_INTEGRATED_KEY_MAX];
    size_t tkey_len;
    uint64_t skip_items;       /* nested items still to skip */
    size_t collect_off;        /* arena offset of the value being collected */
    size_t collect_head_len;
    suit_hash_t hash;          /* digest of a severed section while it streams */
    int payload_comp;          /* component receiving an integrated payload */
    bool tag_seen;
    bool auth_seen;
    bool manifest_seen;
} suit_frame_t;

esp_err_t suit_frame_feed(struct esp_suit_ota_ctx *ctx, const uint8_t *data, size_t len, size_t *used);

/* ---------- the handle ---------- */

typedef enum {
    SUIT_ST_FEEDING = 0,
    SUIT_ST_READY,
    SUIT_ST_COMMITTED,
} suit_handle_state_t;

struct esp_suit_ota_ctx {
    esp_suit_ota_config_t cfg;
    esp_suit_ota_transport_t *t;
    SemaphoreHandle_t lock;
    suit_handle_state_t st;
    esp_err_t sticky_err;

    uint8_t *arena;
    size_t arena_size;
    size_t arena_used;

    suit_frame_t fr;
    uint8_t manifest_digest[SUIT_DIGEST_LEN];
    suit_span_t auth;
    suit_span_t manifest_bstr;     /* head + content */
    suit_manifest_t man;
    bool manifest_verified;
    bool planned;
    uint32_t needed_sections;      /* bit 16 and bit 20 while severed sections are outstanding */

    suit_component_t comp[SUIT_MAX_COMP];
    size_t num_comp;
    uint8_t order[SUIT_MAX_COMP];  /* pending components in fetch order */
    size_t num_order;
    size_t order_pos;              /* next pending component to redirect to */
    size_t current;                /* component receiving bytes, or SIZE_MAX */
    suit_hash_t stream_hash;       /* digest of the payload being written */

    char description[CONFIG_ESP_SUIT_OTA_TEXT_CAP];
    char url_buf[SUIT_URI_MAX];    /* storage for t->url */
    char base_url[SUIT_URI_MAX];   /* URL the manifest was fetched from */

    suit_journal_t journal;
    bool resumed;
    size_t resume_comp;            /* component the loaded journal was writing, SIZE_MAX if none */
    uint32_t resume_bytes;
    uint32_t campaign_id;
    uint32_t journal_mark;         /* bytes written when the journal was last saved */
    bool has_app;
    uint8_t app_slot;
};

void suit_post_event(esp_suit_ota_event_id_t id, const void *data, size_t len);
void suit_post_abort(struct esp_suit_ota_ctx *ctx, esp_err_t err, uint8_t phase);
esp_err_t suit_on_manifest_complete(struct esp_suit_ota_ctx *ctx);
esp_err_t suit_on_severed_section(struct esp_suit_ota_ctx *ctx, int key, const uint8_t *bstr_with_head,
                                  size_t len, const uint8_t digest[SUIT_DIGEST_LEN]);
esp_err_t suit_on_text_bytes(struct esp_suit_ota_ctx *ctx, const uint8_t *content, size_t len,
                             const uint8_t digest[SUIT_DIGEST_LEN]);
esp_err_t suit_run_plan_if_ready(struct esp_suit_ota_ctx *ctx);
int suit_find_integrated_component(struct esp_suit_ota_ctx *ctx, const char *name, size_t name_len);
esp_err_t suit_after_envelope(struct esp_suit_ota_ctx *ctx);
esp_err_t suit_payload_write(struct esp_suit_ota_ctx *ctx, const uint8_t *data, size_t len);
esp_err_t suit_payload_boundary(struct esp_suit_ota_ctx *ctx);
esp_err_t suit_journal_save(struct esp_suit_ota_ctx *ctx, suit_phase_t phase);
void suit_cid_to_string(const esp_suit_ota_span_t *parts, size_t n, char *out, size_t cap);
void suit_ctx_free(struct esp_suit_ota_ctx *ctx);
void suit_close_getters(esp_suit_ota_transport_t *t);
bool suit_cid_equal(const esp_suit_ota_span_t *a, size_t na, const esp_suit_ota_span_t *b, size_t nb);

#ifdef __cplusplus
}
#endif
