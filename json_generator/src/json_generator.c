/*
 *    Copyright 2020 Piyush Shah <shahpiyushv@gmail.com>
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

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>

#include <json_generator.h>

/* Whether snprintf() can format a 64-bit integer; ESP-IDF's "nano" printf
 * (CONFIG_LIBC_NEWLIB_NANO_FORMAT) cannot, and CMakeLists.txt sets this to 0 */
#ifndef JSON_GEN_PRINTF_HAS_INT64
#define JSON_GEN_PRINTF_HAS_INT64 1
#endif

/* The longest int64 is 20 characters, plus NUL */
#define MAX_INT64_IN_STR    24
/* "%.Nf" of FLT_MAX: 39 integer digits + '.' + N fraction digits + sign + NUL */
#define MAX_FLOAT_IN_STR    (39 + 1 + JSON_FLOAT_PRECISION + 1 + 1)
/* "%.17g" is at most 24 characters, plus NUL */
#define MAX_DOUBLE_IN_STR   25

static const char json_gen_hex[] = "0123456789abcdef";

static inline int json_gen_get_empty_len(json_gen_str_t *jstr)
{
    return (jstr->buf_size - (jstr->free_ptr - jstr->buf) - 1);
}

/* Record the first error and make every later call a no-op */
static inline int json_gen_fail(json_gen_str_t *jstr, int err)
{
    if (!jstr->err) {
        jstr->err = err;
    }
    return jstr->err;
}

/* Add len bytes to the JSON string buffer, flushing it out if it gets full.
 * In a measuring pass (buf == NULL) only the length is accounted.
 * Note that the data being flushed out will always be equal to the size of
 * the buffer unless this is the last chunk being flushed out on
 * json_gen_str_end()
 */
static int json_gen_add_len(json_gen_str_t *jstr, const char *str, int len)
{
    if (jstr->err) {
        return jstr->err;
    }
    if (!str || len <= 0) {
        return JSON_GEN_OK;
    }
    jstr->total_len += len;
    if (jstr->buf == NULL) {
        return JSON_GEN_OK;
    }
    while (len) {
        int len_remaining = json_gen_get_empty_len(jstr);
        int copy_len = len_remaining > len ? len : len_remaining;
        memcpy(jstr->free_ptr, str, copy_len);
        str += copy_len;
        jstr->free_ptr += copy_len;
        len -= copy_len;
        if (len) {
            *jstr->free_ptr = '\0';
            /* Report error if the buffer is full and no flush callback
             * is registered
             */
            if (!jstr->flush_cb) {
                return json_gen_fail(jstr, JSON_GEN_ERR_BUF_FULL);
            }
            jstr->flush_cb(jstr->buf, jstr->priv);
            jstr->free_ptr = jstr->buf;
        }
    }
    return JSON_GEN_OK;
}

static int json_gen_add_to_str(json_gen_str_t *jstr, const char *str)
{
    return json_gen_add_len(jstr, str, str ? (int)strlen(str) : 0);
}

/* Feed one byte to the UTF-8 validator (RFC 3629 section 4). The state lives
 * in jstr so that a multi-byte sequence may span long-string chunks.
 * Returns false on the first byte that cannot be part of well-formed UTF-8.
 */
static bool json_gen_utf8_step(json_gen_str_t *jstr, unsigned char c)
{
    if (jstr->utf8_need) {
        if (c < jstr->utf8_lo || c > jstr->utf8_hi) {
            return false;
        }
        jstr->utf8_need--;
        jstr->utf8_lo = 0x80;
        jstr->utf8_hi = 0xBF;
        return true;
    }
    if (c < 0x80) {
        return true;
    }
    jstr->utf8_lo = 0x80;
    jstr->utf8_hi = 0xBF;
    if (c >= 0xC2 && c <= 0xDF) {
        jstr->utf8_need = 1;
    } else if (c == 0xE0) {
        jstr->utf8_need = 2;
        jstr->utf8_lo = 0xA0;   /* no overlong 3-byte forms */
    } else if ((c >= 0xE1 && c <= 0xEC) || c == 0xEE || c == 0xEF) {
        jstr->utf8_need = 2;
    } else if (c == 0xED) {
        jstr->utf8_need = 2;
        jstr->utf8_hi = 0x9F;   /* no UTF-16 surrogates */
    } else if (c == 0xF0) {
        jstr->utf8_need = 3;
        jstr->utf8_lo = 0x90;   /* no overlong 4-byte forms */
    } else if (c >= 0xF1 && c <= 0xF3) {
        jstr->utf8_need = 3;
    } else if (c == 0xF4) {
        jstr->utf8_need = 3;
        jstr->utf8_hi = 0x8F;   /* nothing above U+10FFFF */
    } else {
        return false;           /* C0, C1, F5..FF, or a stray continuation byte */
    }
    return true;
}

/* Close a string. It must not end in the middle of a UTF-8 sequence. */
static int json_gen_close_string(json_gen_str_t *jstr)
{
    if (jstr->utf8_need) {
        jstr->utf8_need = 0;
        json_gen_fail(jstr, JSON_GEN_ERR_INVALID_UTF8);
    }
    return json_gen_add_to_str(jstr, "\"");
}

/* Add len bytes of a string, escaped as required by RFC 8259 section 7 and
 * checked for well-formed UTF-8 (section 8.1). Runs of characters that need
 * no escaping are copied in one go. A NUL byte inside the range is a
 * character like any other and comes out as \u0000.
 */
static int json_gen_add_escaped_len(json_gen_str_t *jstr, const char *str, size_t len)
{
    if (jstr->err) {
        return jstr->err;
    }
    if (len == 0) {
        return JSON_GEN_OK;
    }
    if (!str || len > (size_t)INT_MAX) {
        return json_gen_fail(jstr, JSON_GEN_ERR_INVALID_ARG);
    }
    const char *run = str;
    const char *p = str;
    const char *end = str + len;
    for (; p < end; p++) {
        unsigned char c = (unsigned char)p[0];
        if (!json_gen_utf8_step(jstr, c)) {
            return json_gen_fail(jstr, JSON_GEN_ERR_INVALID_UTF8);
        }
        const char *esc = NULL;
        int esc_len = 2;
        char ubuf[6];
        switch (c) {
        case '"':  esc = "\\\""; break;
        case '\\': esc = "\\\\"; break;
        case '\b': esc = "\\b";  break;
        case '\f': esc = "\\f";  break;
        case '\n': esc = "\\n";  break;
        case '\r': esc = "\\r";  break;
        case '\t': esc = "\\t";  break;
        default:
            if (c < 0x20) {
                ubuf[0] = '\\';
                ubuf[1] = 'u';
                ubuf[2] = '0';
                ubuf[3] = '0';
                ubuf[4] = json_gen_hex[c >> 4];
                ubuf[5] = json_gen_hex[c & 0xF];
                esc = ubuf;
                esc_len = 6;
            }
            break;
        }
        if (esc) {
            json_gen_add_len(jstr, run, (int)(p - run));
            json_gen_add_len(jstr, esc, esc_len);
            run = p + 1;
        }
    }
    return json_gen_add_len(jstr, run, (int)(p - run));
}

static int json_gen_add_escaped(json_gen_str_t *jstr, const char *str)
{
    return json_gen_add_escaped_len(jstr, str, str ? strlen(str) : 0);
}

void json_gen_str_start(json_gen_str_t *jstr, char *buf, int buf_size,
                        json_gen_flush_cb_t flush_cb, void *priv)
{
    memset(jstr, 0, sizeof(json_gen_str_t));
    jstr->buf = buf;
    jstr->buf_size = buf_size;
    jstr->flush_cb = flush_cb;
    jstr->free_ptr = buf;
    jstr->priv = priv;
}

void json_gen_str_start_measure(json_gen_str_t *jstr)
{
    json_gen_str_start(jstr, NULL, 0, NULL, NULL);
}

int json_gen_str_end(json_gen_str_t *jstr)
{
    int err = jstr->err;
    int total_len = jstr->total_len;
    if (jstr->buf) {
        *jstr->free_ptr = '\0';
        if (!err && jstr->flush_cb) {
            jstr->flush_cb(jstr->buf, jstr->priv);
        }
    }
    memset(jstr, 0, sizeof(json_gen_str_t));
    if (err) {
        return err;
    }
    return total_len + 1; /* +1 for the NULL termination */
}

static inline void json_gen_handle_comma(json_gen_str_t *jstr)
{
    if (jstr->comma_req) {
        json_gen_add_to_str(jstr, ",");
    }
}

static int json_gen_handle_name(json_gen_str_t *jstr, const char *name)
{
    json_gen_add_to_str(jstr, "\"");
    json_gen_add_escaped(jstr, name);
    json_gen_close_string(jstr);
    return json_gen_add_to_str(jstr, ":");
}

int json_gen_start_object(json_gen_str_t *jstr)
{
    json_gen_handle_comma(jstr);
    jstr->comma_req = false;
    return json_gen_add_to_str(jstr, "{");
}

int json_gen_end_object(json_gen_str_t *jstr)
{
    jstr->comma_req = true;
    return json_gen_add_to_str(jstr, "}");
}

int json_gen_start_array(json_gen_str_t *jstr)
{
    json_gen_handle_comma(jstr);
    jstr->comma_req = false;
    return json_gen_add_to_str(jstr, "[");
}

int json_gen_end_array(json_gen_str_t *jstr)
{
    jstr->comma_req = true;
    return json_gen_add_to_str(jstr, "]");
}

int json_gen_push_object(json_gen_str_t *jstr, const char *name)
{
    json_gen_handle_comma(jstr);
    json_gen_handle_name(jstr, name);
    jstr->comma_req = false;
    return json_gen_add_to_str(jstr, "{");
}

int json_gen_pop_object(json_gen_str_t *jstr)
{
    jstr->comma_req = true;
    return json_gen_add_to_str(jstr, "}");
}

int json_gen_push_object_str(json_gen_str_t *jstr, const char *name, const char *object_str)
{
    json_gen_handle_comma(jstr);
    json_gen_handle_name(jstr, name);
    jstr->comma_req = true;
    return json_gen_add_to_str(jstr, object_str);
}

int json_gen_push_array(json_gen_str_t *jstr, const char *name)
{
    json_gen_handle_comma(jstr);
    json_gen_handle_name(jstr, name);
    jstr->comma_req = false;
    return json_gen_add_to_str(jstr, "[");
}

int json_gen_pop_array(json_gen_str_t *jstr)
{
    jstr->comma_req = true;
    return json_gen_add_to_str(jstr, "]");
}

int json_gen_push_array_str(json_gen_str_t *jstr, const char *name, const char *array_str)
{
    json_gen_handle_comma(jstr);
    json_gen_handle_name(jstr, name);
    jstr->comma_req = true;
    return json_gen_add_to_str(jstr, array_str);
}

static int json_gen_set_bool(json_gen_str_t *jstr, bool val)
{
    jstr->comma_req = true;
    if (val) {
        return json_gen_add_to_str(jstr, "true");
    } else {
        return json_gen_add_to_str(jstr, "false");
    }
}

int json_gen_obj_set_bool(json_gen_str_t *jstr, const char *name, bool val)
{
    json_gen_handle_comma(jstr);
    json_gen_handle_name(jstr, name);
    return json_gen_set_bool(jstr, val);
}

int json_gen_arr_set_bool(json_gen_str_t *jstr, bool val)
{
    json_gen_handle_comma(jstr);
    return json_gen_set_bool(jstr, val);
}

/* Add a formatted number, refusing to emit anything the formatter had to truncate */
static int json_gen_add_number(json_gen_str_t *jstr, const char *str, int cap, int written)
{
    jstr->comma_req = true;
    if (written < 0 || written >= cap) {
        return json_gen_fail(jstr, JSON_GEN_ERR_NUM_TRUNC);
    }
    return json_gen_add_len(jstr, str, written);
}

static int json_gen_set_int64(json_gen_str_t *jstr, int64_t val);

static int json_gen_set_int(json_gen_str_t *jstr, int val)
{
    return json_gen_set_int64(jstr, val);
}

int json_gen_obj_set_int(json_gen_str_t *jstr, const char *name, int val)
{
    json_gen_handle_comma(jstr);
    json_gen_handle_name(jstr, name);
    return json_gen_set_int(jstr, val);
}

int json_gen_arr_set_int(json_gen_str_t *jstr, int val)
{
    json_gen_handle_comma(jstr);
    return json_gen_set_int(jstr, val);
}

#if JSON_GEN_PRINTF_HAS_INT64
#define json_gen_format_int64(str, size, val) snprintf(str, size, "%" PRId64, val)
#else
/* Decimal text of val, as "%" PRId64 writes it: digits are produced from the
 * end of str backwards, then moved to the front. size is MAX_INT64_IN_STR. */
static int json_gen_format_int64(char *str, int size, int64_t val)
{
    char *end = str + size - 1, *p = end;
    uint64_t mag = val < 0 ? 0 - (uint64_t)val : (uint64_t)val;
    *p = '\0';
    do {
        uint64_t q = mag / 10;   /* one 64-bit division per digit */
        *--p = (char)('0' + (unsigned)(mag - q * 10));
        mag = q;
    } while (mag);
    if (val < 0) {
        *--p = '-';
    }
    memmove(str, p, end - p + 1);
    return (int)(end - p);
}
#endif

static int json_gen_set_int64(json_gen_str_t *jstr, int64_t val)
{
    char str[MAX_INT64_IN_STR];
    int written = json_gen_format_int64(str, sizeof(str), val);
    return json_gen_add_number(jstr, str, sizeof(str), written);
}

int json_gen_obj_set_int64(json_gen_str_t *jstr, const char *name, int64_t val)
{
    json_gen_handle_comma(jstr);
    json_gen_handle_name(jstr, name);
    return json_gen_set_int64(jstr, val);
}

int json_gen_arr_set_int64(json_gen_str_t *jstr, int64_t val)
{
    json_gen_handle_comma(jstr);
    return json_gen_set_int64(jstr, val);
}

static int json_gen_set_null(json_gen_str_t *jstr);

static int json_gen_set_float(json_gen_str_t *jstr, float val)
{
    /* JSON has no NaN or infinity (RFC 8259 section 6); write null like
     * cJSON and JavaScript's JSON.stringify() do */
    if (!isfinite(val)) {
        return json_gen_set_null(jstr);
    }
    char str[MAX_FLOAT_IN_STR];
    int written = snprintf(str, sizeof(str), "%.*f", JSON_FLOAT_PRECISION, (double)val);
    return json_gen_add_number(jstr, str, sizeof(str), written);
}

int json_gen_obj_set_float(json_gen_str_t *jstr, const char *name, float val)
{
    json_gen_handle_comma(jstr);
    json_gen_handle_name(jstr, name);
    return json_gen_set_float(jstr, val);
}

int json_gen_arr_set_float(json_gen_str_t *jstr, float val)
{
    json_gen_handle_comma(jstr);
    return json_gen_set_float(jstr, val);
}

/* Shortest representation that survives a round trip. %g drops trailing
 * zeros, so 15 significant digits already give the shortest form of every
 * value that has one that short (DBL_DIG); the rest need 16 or 17. */
static int json_gen_set_double(json_gen_str_t *jstr, double val)
{
    if (!isfinite(val)) {
        return json_gen_set_null(jstr);
    }
    char str[MAX_DOUBLE_IN_STR];
    int written = 0;
    for (int digits = 15; digits <= 17; digits++) {
        written = snprintf(str, sizeof(str), "%.*g", digits, val);
        if (written <= 0 || strtod(str, NULL) == val) {
            break;
        }
    }
    return json_gen_add_number(jstr, str, sizeof(str), written);
}

int json_gen_obj_set_double(json_gen_str_t *jstr, const char *name, double val)
{
    json_gen_handle_comma(jstr);
    json_gen_handle_name(jstr, name);
    return json_gen_set_double(jstr, val);
}

int json_gen_arr_set_double(json_gen_str_t *jstr, double val)
{
    json_gen_handle_comma(jstr);
    return json_gen_set_double(jstr, val);
}

static int json_gen_set_string_len(json_gen_str_t *jstr, const char *val, size_t len)
{
    jstr->comma_req = true;
    json_gen_add_to_str(jstr, "\"");
    json_gen_add_escaped_len(jstr, val, len);
    return json_gen_close_string(jstr);
}

static int json_gen_set_string(json_gen_str_t *jstr, const char *val)
{
    return json_gen_set_string_len(jstr, val, val ? strlen(val) : 0);
}

int json_gen_obj_set_string_len(json_gen_str_t *jstr, const char *name, const char *val, size_t len)
{
    json_gen_handle_comma(jstr);
    json_gen_handle_name(jstr, name);
    return json_gen_set_string_len(jstr, val, len);
}

int json_gen_arr_set_string_len(json_gen_str_t *jstr, const char *val, size_t len)
{
    json_gen_handle_comma(jstr);
    return json_gen_set_string_len(jstr, val, len);
}

int json_gen_obj_set_string(json_gen_str_t *jstr, const char *name, const char *val)
{
    json_gen_handle_comma(jstr);
    json_gen_handle_name(jstr, name);
    return json_gen_set_string(jstr, val);
}

int json_gen_arr_set_string(json_gen_str_t *jstr, const char *val)
{
    json_gen_handle_comma(jstr);
    return json_gen_set_string(jstr, val);
}

static int json_gen_set_long_string(json_gen_str_t *jstr, const char *val)
{
    jstr->comma_req = true;
    json_gen_add_to_str(jstr, "\"");
    return json_gen_add_escaped(jstr, val);
}

int json_gen_obj_start_long_string(json_gen_str_t *jstr, const char *name, const char *val)
{
    json_gen_handle_comma(jstr);
    json_gen_handle_name(jstr, name);
    return json_gen_set_long_string(jstr, val);
}

int json_gen_arr_start_long_string(json_gen_str_t *jstr, const char *val)
{
    json_gen_handle_comma(jstr);
    return json_gen_set_long_string(jstr, val);
}

int json_gen_add_to_long_string(json_gen_str_t *jstr, const char *val)
{
    return json_gen_add_escaped(jstr, val);
}

int json_gen_add_to_long_string_len(json_gen_str_t *jstr, const char *val, size_t len)
{
    return json_gen_add_escaped_len(jstr, val, len);
}

int json_gen_end_long_string(json_gen_str_t *jstr)
{
    return json_gen_close_string(jstr);
}

static int json_gen_set_null(json_gen_str_t *jstr)
{
    jstr->comma_req = true;
    return json_gen_add_to_str(jstr, "null");
}

int json_gen_obj_set_null(json_gen_str_t *jstr, const char *name)
{
    json_gen_handle_comma(jstr);
    json_gen_handle_name(jstr, name);
    return json_gen_set_null(jstr);
}

int json_gen_arr_set_null(json_gen_str_t *jstr)
{
    json_gen_handle_comma(jstr);
    return json_gen_set_null(jstr);
}
