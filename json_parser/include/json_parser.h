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
#ifndef _JSON_PARSER_H_
#define _JSON_PARSER_H_

#define JSMN_PARENT_LINKS
#define JSMN_HEADER
#include <jsmn.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define OS_SUCCESS  0
#define OS_FAIL     -1

/*
 * json_parse_start() accepts exactly the JSON texts of RFC 8259: any value at
 * the top level, and nothing that the grammar does not allow. Strings are
 * returned decoded: escape sequences, including \uXXXX surrogate pairs, are
 * turned into UTF-8, and the *_get_strlen() functions return the decoded
 * length in bytes (without the terminator). Names are matched decoded as
 * well, so a member written as "r\u00e9sum\u00e9" is found by "résumé". A value
 * containing U+0000 or a lone surrogate escape cannot be decoded into a C
 * string and the accessor fails; a name containing U+0000 cannot be looked
 * up. When an
 * object repeats a name, the first occurrence is used. Numbers that do not
 * fit the requested type fail instead of wrapping. Booleans are only true
 * and false; use *_get_null() to test for null.
 */

typedef jsmn_parser json_parser_t;
typedef jsmntok_t json_tok_t;

typedef struct {
    json_parser_t parser;
    const char *js;
    json_tok_t *tokens;
    json_tok_t *cur;
    int num_tokens;
} jparse_ctx_t;

int json_parse_start(jparse_ctx_t *jctx, const char *js, int len);
int json_parse_end(jparse_ctx_t *jctx);
int json_parse_start_static(jparse_ctx_t *jctx, const char *js, int len, json_tok_t *buffer_tokens, int buffer_tokens_max_count);
int json_parse_end_static(jparse_ctx_t *jctx);

int json_obj_get_array(jparse_ctx_t *jctx, const char *name, int *num_elem);
int json_obj_leave_array(jparse_ctx_t *jctx);
int json_obj_get_object(jparse_ctx_t *jctx, const char *name);
int json_obj_leave_object(jparse_ctx_t *jctx);
int json_obj_get_bool(jparse_ctx_t *jctx, const char *name, bool *val);
int json_obj_get_int(jparse_ctx_t *jctx, const char *name, int *val);
int json_obj_get_int64(jparse_ctx_t *jctx, const char *name, int64_t *val);
int json_obj_get_float(jparse_ctx_t *jctx, const char *name, float *val);
int json_obj_get_double(jparse_ctx_t *jctx, const char *name, double *val);
int json_obj_get_null(jparse_ctx_t *jctx, const char *name);
int json_obj_get_string(jparse_ctx_t *jctx, const char *name, char *val, int size);
int json_obj_get_strlen(jparse_ctx_t *jctx, const char *name, int *strlen);
int json_obj_get_object_str(jparse_ctx_t *jctx, const char *name, char *val, int size);
int json_obj_get_object_strlen(jparse_ctx_t *jctx, const char *name, int *strlen);
int json_obj_get_array_str(jparse_ctx_t *jctx, const char *name, char *val, int size);
int json_obj_get_array_strlen(jparse_ctx_t *jctx, const char *name, int *strlen);

int json_arr_get_array(jparse_ctx_t *jctx, uint32_t index);
int json_arr_leave_array(jparse_ctx_t *jctx);
int json_arr_get_object(jparse_ctx_t *jctx, uint32_t index);
int json_arr_leave_object(jparse_ctx_t *jctx);
int json_arr_get_bool(jparse_ctx_t *jctx, uint32_t index, bool *val);
int json_arr_get_int(jparse_ctx_t *jctx, uint32_t index, int *val);
int json_arr_get_int64(jparse_ctx_t *jctx, uint32_t index, int64_t *val);
int json_arr_get_float(jparse_ctx_t *jctx, uint32_t index, float *val);
int json_arr_get_double(jparse_ctx_t *jctx, uint32_t index, double *val);
int json_arr_get_null(jparse_ctx_t *jctx, uint32_t index);
int json_arr_get_string(jparse_ctx_t *jctx, uint32_t index, char *val, int size);
int json_arr_get_strlen(jparse_ctx_t *jctx, uint32_t index, int *strlen);

/*
 * The token of a member or element, to read its text in place without a
 * copy: jctx->js[tok->start] up to jctx->js[tok->end] is the value as it
 * appears in the document, escapes intact. A string token that contains no
 * backslash is therefore already its decoded value. Pass JSMN_UNDEFINED as
 * the type to accept any type; NULL is returned when the name or index does
 * not exist or the type does not match.
 */
json_tok_t *json_obj_get_val_tok(jparse_ctx_t *jctx, const char *name, jsmntype_t type);
json_tok_t *json_arr_get_val_tok(jparse_ctx_t *jctx, uint32_t index, jsmntype_t type);

/* A JSON text that is a single value rather than an object or array */
int json_root_get_string(jparse_ctx_t *jctx, char *val, int size);
int json_root_get_strlen(jparse_ctx_t *jctx, int *strlen);
int json_root_get_bool(jparse_ctx_t *jctx, bool *val);
int json_root_get_int(jparse_ctx_t *jctx, int *val);
int json_root_get_int64(jparse_ctx_t *jctx, int64_t *val);
int json_root_get_float(jparse_ctx_t *jctx, float *val);
int json_root_get_double(jparse_ctx_t *jctx, double *val);
int json_root_get_null(jparse_ctx_t *jctx);

#ifdef __cplusplus
}
#endif

#endif /* _JSON_PARSER_H_ */
