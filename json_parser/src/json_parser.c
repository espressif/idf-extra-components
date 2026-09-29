/*
 *    Copyright 2020 Piyush Shah <shahpiyushv@gmail.com>
 *    SPDX-FileContributor: 2026 Espressif Systems (Shanghai) CO LTD
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#define JSMN_PARENT_LINKS
#define JSMN_STRICT
#define JSMN_STATIC
#include <jsmn.h>
#include <json_parser.h>

/*
 * Decode the next unit of a JSON string token at *p (a raw byte or an
 * escape sequence) into out as UTF-8. Returns the number of bytes produced,
 * or -1 when the escape cannot be decoded (a lone surrogate). The token's
 * syntax was already validated by jsmn in strict mode.
 */
static unsigned hex4(const char *s)
{
    return (jsmn_hexval(s[0]) << 12) | (jsmn_hexval(s[1]) << 8) | (jsmn_hexval(s[2]) << 4) | jsmn_hexval(s[3]);
}

static int utf8_encode(unsigned cp, unsigned char out[4])
{
    if (cp < 0x80) {
        out[0] = (unsigned char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = 0xC0 | (cp >> 6);
        out[1] = 0x80 | (cp & 0x3F);
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = 0xE0 | (cp >> 12);
        out[1] = 0x80 | ((cp >> 6) & 0x3F);
        out[2] = 0x80 | (cp & 0x3F);
        return 3;
    }
    out[0] = 0xF0 | (cp >> 18);
    out[1] = 0x80 | ((cp >> 12) & 0x3F);
    out[2] = 0x80 | ((cp >> 6) & 0x3F);
    out[3] = 0x80 | (cp & 0x3F);
    return 4;
}

static int json_string_next(const char **p, const char *end, unsigned char out[4])
{
    const char *s = *p;
    if (*s != '\\') {
        out[0] = (unsigned char) * s;
        *p = s + 1;
        return 1;
    }
    s++;
    char e = *s++;
    switch (e) {
    case '"':
    case '\\':
    case '/':
        out[0] = (unsigned char)e;
        break;
    case 'b':
        out[0] = '\b';
        break;
    case 'f':
        out[0] = '\f';
        break;
    case 'n':
        out[0] = '\n';
        break;
    case 'r':
        out[0] = '\r';
        break;
    case 't':
        out[0] = '\t';
        break;
    case 'u': {
        unsigned cp = hex4(s);
        s += 4;
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            /* high surrogate: the low half must follow as another \u escape */
            if (end - s < 6 || s[0] != '\\' || s[1] != 'u') {
                return -1;
            }
            unsigned lo = hex4(s + 2);
            if (lo < 0xDC00 || lo > 0xDFFF) {
                return -1;
            }
            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            s += 6;
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            return -1;
        }
        *p = s;
        return utf8_encode(cp, out);
    }
    default:
        return -1;
    }
    *p = s;
    return 1;
}

/*
 * Decode a string token into out (NULL to only measure it). Returns the
 * decoded length in bytes, or -1 when it does not fit or cannot be decoded,
 * which includes a decoded U+0000 since a C string cannot carry it. A token
 * without a backslash is its own value, which is the common case.
 */
static int json_tok_decode(jparse_ctx_t *jctx, json_tok_t *tok, char *out, int size)
{
    const char *p = jctx->js + tok->start;
    const char *end = jctx->js + tok->end;
    int total = end - p;
    if (!memchr(p, '\\', total)) {
        if (out) {
            if (total >= size) {
                return -1;
            }
            memcpy(out, p, total);
            out[total] = '\0';
        }
        return total;
    }
    total = 0;
    while (p < end) {
        unsigned char u[4];
        int n = json_string_next(&p, end, u);
        if (n < 0 || u[0] == 0) {
            return -1;
        }
        if (out) {
            if (total + n >= size) {
                return -1;
            }
            for (int i = 0; i < n; i++) {
                out[total + i] = (char)u[i];
            }
        }
        total += n;
    }
    if (out) {
        out[total] = '\0';
    }
    return total;
}

/* Compare a token's decoded text with a C string (a name lookup, or a literal) */
static bool token_matches_str(jparse_ctx_t *ctx, json_tok_t *tok, const char *str)
{
    const char *p = ctx->js + tok->start;
    const char *end = ctx->js + tok->end;
    if (!memchr(p, '\\', end - p)) {
        /* strncmp stops at the end of str when it is the shorter one (the
         * text has no NUL), so str[n] is only read after n bytes matched */
        size_t n = end - p;
        return strncmp(p, str, n) == 0 && str[n] == '\0';
    }
    while (p < end) {
        unsigned char u[4];
        int n = json_string_next(&p, end, u);
        if (n < 0) {
            return false;
        }
        for (int i = 0; i < n; i++) {
            /* a decoded NUL can never match a C string */
            if (u[i] == 0 || (unsigned char)str[i] != u[i]) {
                return false;
            }
        }
        str += n;
    }
    return *str == '\0';
}

static json_tok_t *json_skip_elem(json_tok_t *token)
{
    json_tok_t *cur = token;
    int cnt = cur->size;
    while (cnt--) {
        cur++;
        cur = json_skip_elem(cur);
    }
    return cur;
}

static int json_tok_to_bool(jparse_ctx_t *jctx, json_tok_t *tok, bool *val)
{
    if (token_matches_str(jctx, tok, "true")) {
        *val = true;
    } else if (token_matches_str(jctx, tok, "false")) {
        *val = false;
    } else {
        return -OS_FAIL;
    }
    return OS_SUCCESS;
}

static int json_tok_is_null(jparse_ctx_t *jctx, json_tok_t *tok)
{
    return token_matches_str(jctx, tok, "null") ? OS_SUCCESS : -OS_FAIL;
}

/*
 * A number token as NUL-terminated text for strtol and friends. A token that
 * is followed by more of the document ends at a delimiter, so it is used in
 * place; one that ends the document (jsmn leaves parser.pos == len) is
 * copied into buf. A number longer than buf is copied to the heap, and
 * *heap is what the caller frees; NULL is returned when that fails, or when
 * heap is NULL (an integer that long is out of range anyway).
 */
static const char *json_tok_number(jparse_ctx_t *jctx, json_tok_t *tok, char *buf, int size, char **heap)
{
    const char *s = jctx->js + tok->start;
    int n = tok->end - tok->start;
    char *copy = buf;
    if (heap) {
        *heap = NULL;
    }
    if (tok->end < (int)jctx->parser.pos) {
        return s;
    }
    if (n >= size) {
        if (!heap || !(*heap = malloc(n + 1))) {
            return NULL;
        }
        copy = *heap;
    }
    memcpy(copy, s, n);
    copy[n] = '\0';
    return copy;
}

/* The longest int64 is 20 characters; most reals fit 32 */
#define JSON_INT_TEXT_MAX   24
#define JSON_REAL_TEXT_MAX  32

static int json_tok_to_int64(jparse_ctx_t *jctx, json_tok_t *tok, int64_t *val)
{
    char buf[JSON_INT_TEXT_MAX], *endptr;
    const char *s = json_tok_number(jctx, tok, buf, sizeof(buf), NULL);
    if (!s) {
        return -OS_FAIL;
    }
    errno = 0;
    long long i64 = strtoll(s, &endptr, 10);
    if (endptr != s + (tok->end - tok->start) || errno == ERANGE) {
        return -OS_FAIL;
    }
    *val = i64;
    return OS_SUCCESS;
}

static int json_tok_to_int(jparse_ctx_t *jctx, json_tok_t *tok, int *val)
{
    char buf[JSON_INT_TEXT_MAX], *endptr;
    const char *s = json_tok_number(jctx, tok, buf, sizeof(buf), NULL);
    if (!s) {
        return -OS_FAIL;
    }
    errno = 0;
    long l = strtol(s, &endptr, 10);
    if (endptr != s + (tok->end - tok->start) || errno == ERANGE || l < INT_MIN || l > INT_MAX) {
        return -OS_FAIL;
    }
    *val = (int)l;
    return OS_SUCCESS;
}

/* The token is an RFC number, so an infinite result can only mean overflow */
static int json_tok_to_double(jparse_ctx_t *jctx, json_tok_t *tok, double *val)
{
    char buf[JSON_REAL_TEXT_MAX], *heap, *endptr;
    const char *s = json_tok_number(jctx, tok, buf, sizeof(buf), &heap);
    if (!s) {
        return -OS_FAIL;
    }
    double d = strtod(s, &endptr);
    bool ok = endptr == s + (tok->end - tok->start) && !isinf(d);
    free(heap);
    if (!ok) {
        return -OS_FAIL;
    }
    *val = d;
    return OS_SUCCESS;
}

static int json_tok_to_float(jparse_ctx_t *jctx, json_tok_t *tok, float *val)
{
    char buf[JSON_REAL_TEXT_MAX], *heap, *endptr;
    const char *s = json_tok_number(jctx, tok, buf, sizeof(buf), &heap);
    if (!s) {
        return -OS_FAIL;
    }
    float f = strtof(s, &endptr);
    bool ok = endptr == s + (tok->end - tok->start) && !isinf(f);
    free(heap);
    if (!ok) {
        return -OS_FAIL;
    }
    *val = f;
    return OS_SUCCESS;
}

static int json_tok_to_strlen(jparse_ctx_t *jctx, json_tok_t *tok, int *strlen)
{
    int len = json_tok_decode(jctx, tok, NULL, 0);
    if (len < 0) {
        return -OS_FAIL;
    }
    *strlen = len;
    return OS_SUCCESS;
}

static int json_tok_to_string(jparse_ctx_t *jctx, json_tok_t *tok, char *val, int size)
{
    return json_tok_decode(jctx, tok, val, size) < 0 ? -OS_FAIL : OS_SUCCESS;
}

/* Copy a token's text as it appears in the document (nested object/array text) */
static int json_tok_to_raw(jparse_ctx_t *jctx, json_tok_t *tok, char *val, int size)
{
    if ((tok->end - tok->start) > (size - 1)) {
        return -OS_FAIL;
    }
    memcpy(val, jctx->js + tok->start, tok->end - tok->start);
    val[tok->end - tok->start] = '\0';
    return OS_SUCCESS;
}

static json_tok_t *json_obj_search(jparse_ctx_t *jctx, const char *key)
{
    json_tok_t *tok = jctx->cur;
    int size = tok->size;
    if (size <= 0) {
        return NULL;
    }
    if (tok->type != JSMN_OBJECT) {
        return NULL;
    }

    while (size--) {
        tok++;
        if (token_matches_str(jctx, tok, key)) {
            return tok;
        }
        tok = json_skip_elem(tok);
    }
    return NULL;
}

json_tok_t *json_obj_get_val_tok(jparse_ctx_t *jctx, const char *name, jsmntype_t type)
{
    json_tok_t *tok = json_obj_search(jctx, name);
    if (!tok) {
        return NULL;
    }
    tok++;
    if (type != JSMN_UNDEFINED && tok->type != type) {
        return NULL;
    }
    return tok;
}

int json_obj_get_array(jparse_ctx_t *jctx, const char *name, int *num_elem)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_ARRAY);
    if (!tok) {
        return -OS_FAIL;
    }
    jctx->cur = tok;
    *num_elem = tok->size;
    return OS_SUCCESS;
}

int json_obj_leave_array(jparse_ctx_t *jctx)
{
    /* The array's parent will be the key */
    if (jctx->cur->parent < 0) {
        return -OS_FAIL;
    }
    jctx->cur = &jctx->tokens[jctx->cur->parent];

    /* The key's parent will be the actual parent object */
    if (jctx->cur->parent < 0) {
        return -OS_FAIL;
    }
    jctx->cur = &jctx->tokens[jctx->cur->parent];
    return OS_SUCCESS;
}

int json_obj_get_object(jparse_ctx_t *jctx, const char *name)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_OBJECT);
    if (!tok) {
        return -OS_FAIL;
    }
    jctx->cur = tok;
    return OS_SUCCESS;
}

int json_obj_leave_object(jparse_ctx_t *jctx)
{
    /* The objects's parent will be the key */
    if (jctx->cur->parent < 0) {
        return -OS_FAIL;
    }
    jctx->cur = &jctx->tokens[jctx->cur->parent];

    /* The key's parent will be the actual parent object */
    if (jctx->cur->parent < 0) {
        return -OS_FAIL;
    }
    jctx->cur = &jctx->tokens[jctx->cur->parent];
    return OS_SUCCESS;
}

int json_obj_get_bool(jparse_ctx_t *jctx, const char *name, bool *val)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_PRIMITIVE);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_bool(jctx, tok, val);
}

int json_obj_get_int(jparse_ctx_t *jctx, const char *name, int *val)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_PRIMITIVE);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_int(jctx, tok, val);
}

int json_obj_get_int64(jparse_ctx_t *jctx, const char *name, int64_t *val)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_PRIMITIVE);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_int64(jctx, tok, val);
}

int json_obj_get_float(jparse_ctx_t *jctx, const char *name, float *val)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_PRIMITIVE);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_float(jctx, tok, val);
}

int json_obj_get_string(jparse_ctx_t *jctx, const char *name, char *val, int size)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_STRING);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_string(jctx, tok, val, size);
}

int json_obj_get_strlen(jparse_ctx_t *jctx, const char *name, int *strlen)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_STRING);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_strlen(jctx, tok, strlen);
}

int json_obj_get_object_str(jparse_ctx_t *jctx, const char *name, char *val, int size)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_OBJECT);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_raw(jctx, tok, val, size);
}

int json_obj_get_object_strlen(jparse_ctx_t *jctx, const char *name, int *strlen)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_OBJECT);
    if (!tok) {
        return -OS_FAIL;
    }
    *strlen = tok->end - tok->start;
    return OS_SUCCESS;
}
int json_obj_get_array_str(jparse_ctx_t *jctx, const char *name, char *val, int size)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_ARRAY);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_raw(jctx, tok, val, size);
}

int json_obj_get_array_strlen(jparse_ctx_t *jctx, const char *name, int *strlen)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_ARRAY);
    if (!tok) {
        return -OS_FAIL;
    }
    *strlen = tok->end - tok->start;
    return OS_SUCCESS;
}

static json_tok_t *json_arr_search(jparse_ctx_t *ctx, uint32_t index)
{
    json_tok_t *tok = ctx->cur;
    if ((tok->type != JSMN_ARRAY) || (tok->size <= 0)) {
        return NULL;
    }
    if (index > (uint32_t)(tok->size - 1)) {
        return NULL;
    }
    /* Increment by 1, so that token points to index 0 */
    tok++;
    while (index--) {
        tok = json_skip_elem(tok);
        tok++;
    }
    return tok;
}
json_tok_t *json_arr_get_val_tok(jparse_ctx_t *jctx, uint32_t index, jsmntype_t type)
{
    json_tok_t *tok = json_arr_search(jctx, index);
    if (!tok) {
        return NULL;
    }
    if (type != JSMN_UNDEFINED && tok->type != type) {
        return NULL;
    }
    return tok;
}

int json_arr_get_array(jparse_ctx_t *jctx, uint32_t index)
{
    json_tok_t *tok = json_arr_get_val_tok(jctx, index, JSMN_ARRAY);
    if (!tok) {
        return -OS_FAIL;
    }
    jctx->cur = tok;
    return OS_SUCCESS;
}

int json_arr_leave_array(jparse_ctx_t *jctx)
{
    if (jctx->cur->parent < 0) {
        return -OS_FAIL;
    }
    jctx->cur = &jctx->tokens[jctx->cur->parent];
    return OS_SUCCESS;
}

int json_arr_get_object(jparse_ctx_t *jctx, uint32_t index)
{
    json_tok_t *tok = json_arr_get_val_tok(jctx, index, JSMN_OBJECT);
    if (!tok) {
        return -OS_FAIL;
    }
    jctx->cur = tok;
    return OS_SUCCESS;
}

int json_arr_leave_object(jparse_ctx_t *jctx)
{
    if (jctx->cur->parent < 0) {
        return -OS_FAIL;
    }
    jctx->cur = &jctx->tokens[jctx->cur->parent];
    return OS_SUCCESS;
}

int json_arr_get_bool(jparse_ctx_t *jctx, uint32_t index, bool *val)
{
    json_tok_t *tok = json_arr_get_val_tok(jctx, index, JSMN_PRIMITIVE);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_bool(jctx, tok, val);
}

int json_arr_get_int(jparse_ctx_t *jctx, uint32_t index, int *val)
{
    json_tok_t *tok = json_arr_get_val_tok(jctx, index, JSMN_PRIMITIVE);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_int(jctx, tok, val);
}

int json_arr_get_int64(jparse_ctx_t *jctx, uint32_t index, int64_t *val)
{
    json_tok_t *tok = json_arr_get_val_tok(jctx, index, JSMN_PRIMITIVE);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_int64(jctx, tok, val);
}

int json_arr_get_float(jparse_ctx_t *jctx, uint32_t index, float *val)
{
    json_tok_t *tok = json_arr_get_val_tok(jctx, index, JSMN_PRIMITIVE);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_float(jctx, tok, val);
}

int json_arr_get_string(jparse_ctx_t *jctx, uint32_t index, char *val, int size)
{
    json_tok_t *tok = json_arr_get_val_tok(jctx, index, JSMN_STRING);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_string(jctx, tok, val, size);
}

int json_arr_get_strlen(jparse_ctx_t *jctx, uint32_t index, int *strlen)
{
    json_tok_t *tok = json_arr_get_val_tok(jctx, index, JSMN_STRING);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_strlen(jctx, tok, strlen);
}

int json_obj_get_double(jparse_ctx_t *jctx, const char *name, double *val)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_PRIMITIVE);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_double(jctx, tok, val);
}

int json_arr_get_double(jparse_ctx_t *jctx, uint32_t index, double *val)
{
    json_tok_t *tok = json_arr_get_val_tok(jctx, index, JSMN_PRIMITIVE);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_to_double(jctx, tok, val);
}

int json_obj_get_null(jparse_ctx_t *jctx, const char *name)
{
    json_tok_t *tok = json_obj_get_val_tok(jctx, name, JSMN_PRIMITIVE);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_is_null(jctx, tok);
}

int json_arr_get_null(jparse_ctx_t *jctx, uint32_t index)
{
    json_tok_t *tok = json_arr_get_val_tok(jctx, index, JSMN_PRIMITIVE);
    if (!tok) {
        return -OS_FAIL;
    }
    return json_tok_is_null(jctx, tok);
}

/* The root token, when the whole JSON text is a single value of that type */
static json_tok_t *json_root_tok(jparse_ctx_t *jctx, jsmntype_t type)
{
    if (!jctx->tokens || jctx->tokens[0].type != type) {
        return NULL;
    }
    return &jctx->tokens[0];
}

int json_root_get_string(jparse_ctx_t *jctx, char *val, int size)
{
    json_tok_t *tok = json_root_tok(jctx, JSMN_STRING);
    return tok ? json_tok_to_string(jctx, tok, val, size) : -OS_FAIL;
}

int json_root_get_strlen(jparse_ctx_t *jctx, int *strlen)
{
    json_tok_t *tok = json_root_tok(jctx, JSMN_STRING);
    return tok ? json_tok_to_strlen(jctx, tok, strlen) : -OS_FAIL;
}

int json_root_get_bool(jparse_ctx_t *jctx, bool *val)
{
    json_tok_t *tok = json_root_tok(jctx, JSMN_PRIMITIVE);
    return tok ? json_tok_to_bool(jctx, tok, val) : -OS_FAIL;
}

int json_root_get_int(jparse_ctx_t *jctx, int *val)
{
    json_tok_t *tok = json_root_tok(jctx, JSMN_PRIMITIVE);
    return tok ? json_tok_to_int(jctx, tok, val) : -OS_FAIL;
}

int json_root_get_int64(jparse_ctx_t *jctx, int64_t *val)
{
    json_tok_t *tok = json_root_tok(jctx, JSMN_PRIMITIVE);
    return tok ? json_tok_to_int64(jctx, tok, val) : -OS_FAIL;
}

int json_root_get_float(jparse_ctx_t *jctx, float *val)
{
    json_tok_t *tok = json_root_tok(jctx, JSMN_PRIMITIVE);
    return tok ? json_tok_to_float(jctx, tok, val) : -OS_FAIL;
}

int json_root_get_double(jparse_ctx_t *jctx, double *val)
{
    json_tok_t *tok = json_root_tok(jctx, JSMN_PRIMITIVE);
    return tok ? json_tok_to_double(jctx, tok, val) : -OS_FAIL;
}

int json_root_get_null(jparse_ctx_t *jctx)
{
    json_tok_t *tok = json_root_tok(jctx, JSMN_PRIMITIVE);
    return tok ? json_tok_is_null(jctx, tok) : -OS_FAIL;
}

int json_parse_start(jparse_ctx_t *jctx, const char *js, int len)
{
    memset(jctx, 0, sizeof(jparse_ctx_t));
    jsmn_init(&jctx->parser);
    int num_tokens = jsmn_parse(&jctx->parser, js, len, NULL, 0);
    if (num_tokens <= 0) {
        return -OS_FAIL;
    }
    jctx->num_tokens = num_tokens;
    jctx->tokens = calloc(num_tokens, sizeof(json_tok_t));
    if (!jctx->tokens) {
        return -OS_FAIL;
    }
    jctx->js = js;
    jsmn_init(&jctx->parser);
    int ret = jsmn_parse(&jctx->parser, js, len, jctx->tokens, jctx->num_tokens);
    if (ret <= 0) {
        free(jctx->tokens);
        memset(jctx, 0, sizeof(jparse_ctx_t));
        return -OS_FAIL;
    }
    jctx->cur = jctx->tokens;
    return OS_SUCCESS;
}

int json_parse_end(jparse_ctx_t *jctx)
{
    if (jctx->tokens) {
        free(jctx->tokens);
    }
    memset(jctx, 0, sizeof(jparse_ctx_t));
    return OS_SUCCESS;
}

int json_parse_start_static(jparse_ctx_t *jctx, const char *js, int len, json_tok_t *buffer_tokens, int buffer_tokens_max_count)
{
    // Init
    memset(buffer_tokens, 0, buffer_tokens_max_count * sizeof(json_tok_t));
    memset(jctx, 0, sizeof(jparse_ctx_t));

    // Check fit
    jsmn_init(&jctx->parser);
    int num_tokens = jsmn_parse(&jctx->parser, js, len, NULL, 0);
    if (num_tokens <= 0 || num_tokens > buffer_tokens_max_count) {
        return -OS_FAIL;
    }

    // Set struct
    jctx->num_tokens = num_tokens;
    jctx->tokens = buffer_tokens;
    jctx->js = js;

    // Parse
    jsmn_init(&jctx->parser);
    int ret = jsmn_parse(&jctx->parser, js, len, jctx->tokens, jctx->num_tokens);
    if (ret <= 0) {
        memset(jctx, 0, sizeof(jparse_ctx_t));
        return -OS_FAIL;
    }
    jctx->cur = jctx->tokens;
    return OS_SUCCESS;
}

int json_parse_end_static(jparse_ctx_t *jctx)
{
    memset(jctx, 0, sizeof(jparse_ctx_t));
    return OS_SUCCESS;
}
