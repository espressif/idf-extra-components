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

#ifdef __cplusplus
extern "C" {
#endif

/*
 * A small reader for deterministic CBOR (RFC 8949 section 4.2.1): definite lengths only,
 * shortest integer encodings only. Everything the manifest profile needs, nothing more.
 */

#define SUIT_CBOR_UINT   0
#define SUIT_CBOR_NINT   1
#define SUIT_CBOR_BSTR   2
#define SUIT_CBOR_TSTR   3
#define SUIT_CBOR_ARRAY  4
#define SUIT_CBOR_MAP    5
#define SUIT_CBOR_TAG    6
#define SUIT_CBOR_SIMPLE 7

/* the initial byte: major type in the top 3 bits, additional information in the low 5 */
#define SUIT_CBOR_MAJOR_SHIFT       5
#define SUIT_CBOR_AI_MASK           0x1F
#define SUIT_CBOR_INITIAL(major, ai) ((uint8_t)(((major) << SUIT_CBOR_MAJOR_SHIFT) | (ai)))

/* additional information below 24 is the value itself; 24..27 say the argument follows in 1, 2, 4 or 8 bytes */
#define SUIT_CBOR_AI_DIRECT_MAX     23
#define SUIT_CBOR_AI_1BYTE          24
#define SUIT_CBOR_AI_2BYTE          25
#define SUIT_CBOR_AI_4BYTE          26
#define SUIT_CBOR_AI_8BYTE          27
#define SUIT_CBOR_HEAD_MAX          9      /* initial byte and an 8-byte argument */

/* the smallest value each argument size may carry in the shortest form (RFC 8949 section 4.2.1) */
#define SUIT_CBOR_1BYTE_MIN         24
#define SUIT_CBOR_2BYTE_MIN         0x100
#define SUIT_CBOR_4BYTE_MIN         0x10000
#define SUIT_CBOR_8BYTE_MIN         0x100000000ULL

/* simple values (major type 7) */
#define SUIT_CBOR_SIMPLE_FALSE      20
#define SUIT_CBOR_SIMPLE_TRUE       21
#define SUIT_CBOR_SIMPLE_NULL       22
#define SUIT_CBOR_SIMPLE_1BYTE_MIN  32     /* simple values below 32 must use the 1-byte form */

#define SUIT_CBOR_EMPTY_BSTR        SUIT_CBOR_INITIAL(SUIT_CBOR_BSTR, 0)
#define SUIT_CBOR_ITEMS_PER_MAP_ENTRY 2    /* a key and its value */

typedef struct {
    uint8_t major;
    uint8_t ai;          /* additional information, 0..27 */
    uint64_t val;        /* argument: integer value, length, count or tag */
    uint8_t len;         /* bytes of the head */
} suit_cbor_head_t;

/**
 * @brief Decode one item head from n available bytes.
 * @return head length, 0 when more bytes are needed, -1 when the encoding is not deterministic
 */
int suit_cbor_head(const uint8_t *p, size_t n, suit_cbor_head_t *h);

typedef struct {
    const uint8_t *p;
    const uint8_t *end;
} suit_cbor_t;

static inline void suit_cbor_init(suit_cbor_t *c, const uint8_t *p, size_t len)
{
    c->p = p;
    c->end = p + len;
}

static inline bool suit_cbor_at_end(const suit_cbor_t *c)
{
    return c->p >= c->end;
}

esp_err_t suit_cbor_peek(const suit_cbor_t *c, suit_cbor_head_t *h);
esp_err_t suit_cbor_get_uint(suit_cbor_t *c, uint64_t *v);
esp_err_t suit_cbor_get_int(suit_cbor_t *c, int64_t *v);
esp_err_t suit_cbor_get_bstr(suit_cbor_t *c, const uint8_t **ptr, size_t *len);
esp_err_t suit_cbor_get_tstr(suit_cbor_t *c, const uint8_t **ptr, size_t *len);
esp_err_t suit_cbor_get_bool(suit_cbor_t *c, bool *v);
esp_err_t suit_cbor_get_nil(suit_cbor_t *c);
esp_err_t suit_cbor_get_array(suit_cbor_t *c, size_t *count);
esp_err_t suit_cbor_get_map(suit_cbor_t *c, size_t *count);
esp_err_t suit_cbor_get_tag(suit_cbor_t *c, uint64_t *tag);
/** @brief Skip one item, nested content included. */
esp_err_t suit_cbor_skip(suit_cbor_t *c);
/** @brief Span of the next item including its head, then advance past it. */
esp_err_t suit_cbor_get_raw(suit_cbor_t *c, const uint8_t **ptr, size_t *len);

#ifdef __cplusplus
}
#endif
