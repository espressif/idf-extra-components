/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
/* Execute a navigation script against json_parser's public API.
 *
 * stdin: one command per line; a document begins with "file <path>" and ends
 * with "end". Keys arrive hex-encoded (decoded UTF-8), "-" means empty.
 *   oo K / xo      enter object under key K / leave        aO I / xO  by index
 *   oa K / xa      enter array under key K  (prints n=<count>)   aA I / xA
 *   os K  as I     string    -> s=<hex>          sl K  al I   strlen -> n=<len>
 *   oi K  ai I     int       -> i=<n>            ol K  aL I   int64  -> i=<n>
 *   od K  ad I     double    -> d=<%.17g>        ob K  ab I   bool   -> b=0|1
 *   on K  an I     null      -> ok
 *   rs rn ri rl rd rb rN   root string / strlen / int / int64 / double / bool / null
 * Every command prints exactly one line: the value, "ok", or "err".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "json_parser.h"

static char *unhex(const char *h)
{
    size_t n = strcmp(h, "-") ? strlen(h) / 2 : 0;
    char *s = malloc(n + 1);
    for (size_t i = 0; i < n; i++) {
        unsigned v;
        sscanf(h + 2 * i, "%2x", &v);
        s[i] = (char)v;
    }
    s[n] = '\0';
    return s;
}

static char *doc;
static jparse_ctx_t ctx;
static int open_doc;
static char sbuf[1 << 16];
static char out[1 << 17];

static void set_hex(const char *s)
{
    char *o = out + sprintf(out, "s=");
    for (; *s; s++) {
        o += sprintf(o, "%02x", (unsigned char) * s);
    }
}

/* Runs one op; returns its status and leaves the value, if any, in out */
static int run(const char *op, const char *k, uint32_t idx)
{
    int iv = 0, n = 0;
    int64_t lv = 0;
    double dv = 0;
    bool bv = false;
    int rc;
    out[0] = '\0';
    if (!strcmp(op, "oo")) {
        return json_obj_get_object(&ctx, k);
    } else if (!strcmp(op, "xo")) {
        return json_obj_leave_object(&ctx);
    } else if (!strcmp(op, "xa")) {
        return json_obj_leave_array(&ctx);
    } else if (!strcmp(op, "aO")) {
        return json_arr_get_object(&ctx, idx);
    } else if (!strcmp(op, "xO")) {
        return json_arr_leave_object(&ctx);
    } else if (!strcmp(op, "aA")) {
        return json_arr_get_array(&ctx, idx);
    } else if (!strcmp(op, "xA")) {
        return json_arr_leave_array(&ctx);
    } else if (!strcmp(op, "oa")) {
        rc = json_obj_get_array(&ctx, k, &n);
        sprintf(out, "n=%d", n);
    } else if (!strcmp(op, "os") || !strcmp(op, "as") || !strcmp(op, "rs")) {
        rc = op[0] == 'o' ? json_obj_get_string(&ctx, k, sbuf, sizeof(sbuf))
             : op[0] == 'a' ? json_arr_get_string(&ctx, idx, sbuf, sizeof(sbuf))
             : json_root_get_string(&ctx, sbuf, sizeof(sbuf));
        set_hex(sbuf);
    } else if (!strcmp(op, "sl") || !strcmp(op, "al") || !strcmp(op, "rn")) {
        rc = op[0] == 's' ? json_obj_get_strlen(&ctx, k, &n)
             : op[0] == 'a' ? json_arr_get_strlen(&ctx, idx, &n)
             : json_root_get_strlen(&ctx, &n);
        sprintf(out, "n=%d", n);
    } else if (!strcmp(op, "oi") || !strcmp(op, "ai") || !strcmp(op, "ri")) {
        rc = op[0] == 'o' ? json_obj_get_int(&ctx, k, &iv)
             : op[0] == 'a' ? json_arr_get_int(&ctx, idx, &iv)
             : json_root_get_int(&ctx, &iv);
        sprintf(out, "i=%d", iv);
    } else if (!strcmp(op, "ol") || !strcmp(op, "aL") || !strcmp(op, "rl")) {
        rc = op[0] == 'o' ? json_obj_get_int64(&ctx, k, &lv)
             : op[0] == 'a' ? json_arr_get_int64(&ctx, idx, &lv)
             : json_root_get_int64(&ctx, &lv);
        sprintf(out, "i=%lld", (long long)lv);
    } else if (!strcmp(op, "od") || !strcmp(op, "ad") || !strcmp(op, "rd")) {
        rc = op[0] == 'o' ? json_obj_get_double(&ctx, k, &dv)
             : op[0] == 'a' ? json_arr_get_double(&ctx, idx, &dv)
             : json_root_get_double(&ctx, &dv);
        sprintf(out, "d=%.17g", dv);
    } else if (!strcmp(op, "ob") || !strcmp(op, "ab") || !strcmp(op, "rb")) {
        rc = op[0] == 'o' ? json_obj_get_bool(&ctx, k, &bv)
             : op[0] == 'a' ? json_arr_get_bool(&ctx, idx, &bv)
             : json_root_get_bool(&ctx, &bv);
        sprintf(out, "b=%d", bv);
    } else if (!strcmp(op, "on")) {
        return json_obj_get_null(&ctx, k);
    } else if (!strcmp(op, "an")) {
        return json_arr_get_null(&ctx, idx);
    } else if (!strcmp(op, "rN")) {
        return json_root_get_null(&ctx);
    } else {
        fprintf(stderr, "bad op %s\n", op);
        exit(2);
    }
    if (rc != 0) {
        out[0] = '\0';
    }
    return rc;
}

int main(void)
{
    static char line[1 << 16];
    while (fgets(line, sizeof(line), stdin)) {
        line[strcspn(line, "\n")] = '\0';
        char op[8] = "", a[1 << 15] = "";
        sscanf(line, "%7s %32767s", op, a);
        if (!strcmp(op, "file")) {
            FILE *f = fopen(a, "rb");
            if (!f) {
                printf("err\n");
                continue;
            }
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            free(doc);
            doc = malloc(sz + 1);
            if (fread(doc, 1, sz, f) != (size_t)sz) {
                sz = 0;
            }
            doc[sz] = 0;
            fclose(f);
            open_doc = json_parse_start(&ctx, doc, (int)sz) == 0;
            printf(open_doc ? "ok\n" : "err\n");
            continue;
        }
        if (!strcmp(op, "end")) {
            if (open_doc) {
                json_parse_end(&ctx);
            }
            open_doc = 0;
            printf("--\n");
            fflush(stdout);
            continue;
        }
        if (!open_doc) {
            printf("err\n");
            continue;
        }
        /* ops on an object take a key; the rest an index */
        char *k = NULL;
        uint32_t idx = 0;
        if (op[0] == 'o' || !strcmp(op, "sl")) {
            k = unhex(a);
        } else {
            idx = (uint32_t)strtoul(a, NULL, 10);
        }
        int rc = run(op, k, idx);
        free(k);
        puts(rc != 0 ? "err" : out[0] ? out : "ok");
    }
    return 0;
}
