/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "unity_test_runner.h"

/* This test owns a strict, static copy of the tokenizer regardless of the
 * component's Kconfig, so the grammar checks below always run */
#define JSMN_STATIC
#define JSMN_STRICT
#define JSMN_PARENT_LINKS
#include "jsmn.h"

static jsmntok_t toks[64];

static int parse(const char *js, size_t len)
{
    jsmn_parser p;
    jsmn_init(&p);
    return jsmn_parse(&p, js, len, toks, sizeof(toks) / sizeof(toks[0]));
}

TEST_CASE("strict mode tokenizes valid texts", "[jsmn]")
{
    TEST_ASSERT_EQUAL_INT(1, parse("{}", 2));
    TEST_ASSERT_EQUAL_INT(1, parse("[]", 2));
    TEST_ASSERT_EQUAL_INT(5, parse("{\"a\":[1,2]}", 11));     /* object, name, array, 1, 2 */
    TEST_ASSERT_EQUAL_INT(1, parse("42", 2));                 /* a lone top-level value */
    TEST_ASSERT_EQUAL_INT(1, parse("\"s\"", 3));
    TEST_ASSERT_EQUAL_INT(1, parse(" null ", 6));
    TEST_ASSERT_EQUAL_INT(3, parse("[-0.5e+3,true]", 14));
    const char *utf = "[\"r\xc3\xa9sum\xc3\xa9 \\u00e9\"]";
    TEST_ASSERT_EQUAL_INT(2, parse(utf, strlen(utf)));
}

TEST_CASE("strict mode enforces the RFC 8259 grammar", "[jsmn]")
{
    const char *bad[] = {
        "[1,]", "{\"a\":1,}", "[1 2]", "{\"a\" 1}", "{\"a\":}", "[,1]", "{\"a\"::1}",
        "{1:2}", "{[]:1}", "{\"a\":1}}", "[", "]", "[1]x", "[][]", "1 2", "",
        "01", "1.", ".5", "-", "1e", "+1", "0x1", "tru", "nul", "True",   /* codespell:ignore tru,nul */
        "\"a\nb\"", "\"\x01\"", "\"\\x\"", "\"\\u00g0\"", "\"\xc0\x80\"", "\"\xed\xa0\x80\"",
        "\"\xf4\x90\x80\x80\"", "\"\xc3\"", "[\xef\xbb\xbf]", "\f1",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        TEST_ASSERT_LESS_THAN_INT_MESSAGE(0, parse(bad[i], strlen(bad[i])), bad[i]);
    }
    /* a NUL byte inside the text is not whitespace */
    TEST_ASSERT_LESS_THAN_INT(0, parse("[1]\0", 4));
    TEST_ASSERT_LESS_THAN_INT(0, parse("12\0", 3));
}

TEST_CASE("the counting pass and the token pass agree", "[jsmn]")
{
    const char *js = "{\"k\":[1,{\"n\":null}],\"s\":\"\\u0041\"}";
    jsmn_parser p;
    jsmn_init(&p);
    int count = jsmn_parse(&p, js, strlen(js), NULL, 0);
    TEST_ASSERT_EQUAL_INT(9, count);
    TEST_ASSERT_EQUAL_INT(count, parse(js, strlen(js)));
    /* too few tokens is reported, not silently truncated */
    jsmn_init(&p);
    TEST_ASSERT_EQUAL_INT(JSMN_ERROR_NOMEM, jsmn_parse(&p, js, strlen(js), toks, 4));
}

void app_main(void)
{
    unity_run_menu();
}
