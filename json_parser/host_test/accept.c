/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdio.h>
#include <stdlib.h>
#include "json_parser.h"
int main(int argc, char **argv)
{
    (void)argc;
    FILE *f = fopen(argv[1], "rb"); if (!f) return 3;
    static char buf[1 << 22]; size_t n = fread(buf, 1, sizeof(buf) - 1, f); fclose(f); buf[n] = 0;
    jparse_ctx_t ctx;
    int rc = json_parse_start(&ctx, buf, (int)n);
    if (rc == 0) {
        json_parse_end(&ctx);
    }
    return rc == 0 ? 0 : 2;   /* 0 = accepted, 2 = rejected; 1 is what a sanitizer abort exits with */
}
