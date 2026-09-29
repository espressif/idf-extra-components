/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <float.h>
#include "unity.h"
#include "unity_test_runner.h"
#include "json_generator.h"

static json_gen_str_t j;
static char buf[512];

#define BEGIN()  json_gen_str_start(&j, buf, sizeof(buf), NULL, NULL)

/* End the string and check both the text and the reported length */
#define END_EXPECT(expected) do {                                        \
        int ret_ = json_gen_str_end(&j);                                 \
        TEST_ASSERT_EQUAL_STRING(expected, buf);                         \
        TEST_ASSERT_EQUAL_INT((int)strlen(expected) + 1, ret_);          \
    } while (0)

TEST_CASE("object with nested array and every scalar type", "[json_generator]")
{
    BEGIN();
    json_gen_start_object(&j);
    json_gen_obj_set_string(&j, "ver", "v1.1");
    json_gen_obj_set_bool(&j, "b", true);
    json_gen_obj_set_int(&j, "i", -42);
    json_gen_obj_set_int64(&j, "i64", -9223372036854775807LL);
    json_gen_obj_set_null(&j, "n");
    json_gen_push_array(&j, "cap");
    json_gen_arr_set_string(&j, "wifi_prov");
    json_gen_arr_set_int(&j, 7);
    json_gen_arr_set_bool(&j, false);
    json_gen_arr_set_null(&j);
    json_gen_pop_array(&j);
    json_gen_push_object(&j, "o");
    json_gen_pop_object(&j);
    json_gen_end_object(&j);
    END_EXPECT("{\"ver\":\"v1.1\",\"b\":true,\"i\":-42,\"i64\":-9223372036854775807,"
               "\"n\":null,\"cap\":[\"wifi_prov\",7,false,null],\"o\":{}}");
}

TEST_CASE("int64 extremes, with or without a 64-bit printf", "[json_generator]")
{
    BEGIN();
    json_gen_start_array(&j);
    json_gen_arr_set_int64(&j, INT64_MIN);
    json_gen_arr_set_int64(&j, INT64_MAX);
    json_gen_arr_set_int64(&j, 0);
    json_gen_arr_set_int64(&j, -1);
    json_gen_arr_set_int64(&j, 1000000000000LL);
    json_gen_end_array(&j);
    END_EXPECT("[-9223372036854775808,9223372036854775807,0,-1,1000000000000]");
}

TEST_CASE("top-level array", "[json_generator]")
{
    BEGIN();
    json_gen_start_array(&j);
    json_gen_arr_set_int(&j, 1);
    json_gen_arr_set_string(&j, "two");
    json_gen_end_array(&j);
    END_EXPECT("[1,\"two\"]");
}

TEST_CASE("quote, backslash and control characters are escaped", "[json_generator]")
{
    BEGIN();
    json_gen_start_object(&j);
    json_gen_obj_set_string(&j, "k", "a\"b\\c\nd\te\rf\bg\fh\x01i\x1f");
    json_gen_end_object(&j);
    END_EXPECT("{\"k\":\"a\\\"b\\\\c\\nd\\te\\rf\\bg\\fh\\u0001i\\u001f\"}");
}

TEST_CASE("solidus and DEL are not escaped", "[json_generator]")
{
    BEGIN();
    json_gen_start_array(&j);
    json_gen_arr_set_string(&j, "a/b\x7f");
    json_gen_end_array(&j);
    END_EXPECT("[\"a/b\x7f\"]");
}

TEST_CASE("element names are escaped", "[json_generator]")
{
    BEGIN();
    json_gen_start_object(&j);
    json_gen_obj_set_int(&j, "ev\"il\\\n", 1);
    json_gen_push_object(&j, "o\"");
    json_gen_pop_object(&j);
    json_gen_push_array(&j, "a\t");
    json_gen_pop_array(&j);
    json_gen_end_object(&j);
    END_EXPECT("{\"ev\\\"il\\\\\\n\":1,\"o\\\"\":{},\"a\\t\":[]}");
}

TEST_CASE("long string is escaped across chunks", "[json_generator]")
{
    BEGIN();
    json_gen_start_object(&j);
    json_gen_obj_start_long_string(&j, "k", "ab\"");
    json_gen_add_to_long_string(&j, "cd\n");
    json_gen_add_to_long_string(&j, "\\");
    json_gen_end_long_string(&j);
    json_gen_push_array(&j, "arr");
    json_gen_arr_start_long_string(&j, NULL);
    json_gen_add_to_long_string(&j, "x");
    json_gen_end_long_string(&j);
    json_gen_pop_array(&j);
    json_gen_end_object(&j);
    END_EXPECT("{\"k\":\"ab\\\"cd\\n\\\\\",\"arr\":[\"x\"]}");
}

TEST_CASE("valid UTF-8 passes through unchanged", "[json_generator]")
{
    /* 2-, 3- and 4-byte sequences, plus the edges of each range */
    const char *s = "\xc3\xa9t\xc3\xa9 \xe6\xb8\xac \xf0\x9f\x98\x80 \xe0\xa0\x80 \xed\x9f\xbf \xf4\x8f\xbf\xbf";
    BEGIN();
    json_gen_start_array(&j);
    json_gen_arr_set_string(&j, s);
    json_gen_end_array(&j);
    char expected[128];
    snprintf(expected, sizeof(expected), "[\"%s\"]", s);
    END_EXPECT(expected);
}

TEST_CASE("UTF-8 sequence split across long string chunks is accepted", "[json_generator]")
{
    BEGIN();
    json_gen_start_array(&j);
    json_gen_arr_start_long_string(&j, "\xc3\xa9t\xc3");
    json_gen_add_to_long_string(&j, "\xa9 \xf0\x9f");
    json_gen_add_to_long_string(&j, "\x98");
    json_gen_add_to_long_string(&j, "\x80");
    json_gen_end_long_string(&j);
    json_gen_end_array(&j);
    END_EXPECT("[\"\xc3\xa9t\xc3\xa9 \xf0\x9f\x98\x80\"]");
}

TEST_CASE("invalid UTF-8 is rejected", "[json_generator]")
{
    const char *bad[] = {
        "\xc3 x",           /* truncated 2-byte sequence */
        "\xc0\x80",         /* overlong encoding of NUL */
        "\xe0\x80\x80",     /* overlong 3-byte form */
        "\xed\xa0\x80",     /* UTF-16 surrogate U+D800 */
        "\xf4\x90\x80\x80", /* above U+10FFFF */
        "\xf5\x80\x80\x80", /* invalid lead byte */
        "\x80",             /* stray continuation byte */
        "abc\xe6\xb8",      /* truncated at end of string */
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        BEGIN();
        json_gen_start_array(&j);
        TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_INVALID_UTF8, json_gen_arr_set_string(&j, bad[i]));
        TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_INVALID_UTF8, json_gen_str_end(&j));
    }
    /* the same applies to element names */
    BEGIN();
    json_gen_start_object(&j);
    TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_INVALID_UTF8, json_gen_obj_set_int(&j, "k\xff", 1));
    TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_INVALID_UTF8, json_gen_str_end(&j));
}

TEST_CASE("long string ending mid-sequence is rejected", "[json_generator]")
{
    BEGIN();
    json_gen_start_array(&j);
    TEST_ASSERT_EQUAL_INT(JSON_GEN_OK, json_gen_arr_start_long_string(&j, "ab\xe6"));
    TEST_ASSERT_EQUAL_INT(JSON_GEN_OK, json_gen_add_to_long_string(&j, "\xb8"));
    TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_INVALID_UTF8, json_gen_end_long_string(&j));
    TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_INVALID_UTF8, json_gen_str_end(&j));
}

TEST_CASE("first error is sticky and stops emission", "[json_generator]")
{
    BEGIN();
    json_gen_start_object(&j);
    json_gen_obj_set_int(&j, "a", 1);
    int len_before = j.total_len;
    TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_INVALID_UTF8, json_gen_obj_set_string(&j, "b", "\xff"));
    TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_INVALID_UTF8, json_gen_obj_set_int(&j, "c", 2));
    TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_INVALID_UTF8, json_gen_end_object(&j));
    /* nothing after the failing value made it into the length */
    TEST_ASSERT_EQUAL_INT(len_before + (int)strlen(",\"b\":\""), j.total_len);
    TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_INVALID_UTF8, json_gen_str_end(&j));
}

TEST_CASE("buffer full without flush callback is reported", "[json_generator]")
{
    char small[8];
    json_gen_str_start(&j, small, sizeof(small), NULL, NULL);
    json_gen_start_array(&j);
    TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_BUF_FULL, json_gen_arr_set_string(&j, "0123456789"));
    TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_BUF_FULL, json_gen_str_end(&j));
}

TEST_CASE("NaN and infinity are written as null", "[json_generator]")
{
    BEGIN();
    json_gen_start_array(&j);
    json_gen_arr_set_float(&j, NAN);
    json_gen_arr_set_float(&j, INFINITY);
    json_gen_arr_set_float(&j, -INFINITY);
    json_gen_arr_set_double(&j, NAN);
    json_gen_arr_set_double(&j, -INFINITY);
    json_gen_end_array(&j);
    END_EXPECT("[null,null,null,null,null]");
}

TEST_CASE("float keeps its fixed format and is never truncated", "[json_generator]")
{
    BEGIN();
    json_gen_start_array(&j);
    json_gen_arr_set_float(&j, 123.456f);
    json_gen_arr_set_float(&j, FLT_MAX);
    json_gen_arr_set_float(&j, -FLT_MAX);
    json_gen_end_array(&j);
    TEST_ASSERT_GREATER_THAN(0, json_gen_str_end(&j));
    TEST_ASSERT_EQUAL_STRING_LEN("[123.45600,", buf, 11);
    /* FLT_MAX with %.5f is 39 digits, a point and 5 zeros: parse it back */
    char *end = NULL;
    float back = strtof(buf + 11, &end);
    TEST_ASSERT_EQUAL_FLOAT(FLT_MAX, back);
    TEST_ASSERT_EQUAL_CHAR(',', *end);
    TEST_ASSERT_EQUAL_FLOAT(-FLT_MAX, strtof(end + 1, &end));
    TEST_ASSERT_EQUAL_STRING("]", end);
}

TEST_CASE("double round-trips exactly in the fewest digits", "[json_generator]")
{
    const double vals[] = { 0.1, 1.0 / 3.0, 1e300, -0.0, 123456789012345678.0, 2.2250738585072014e-308,
                            -1.779652973678931e+173, 9007199254740993.0
                          };
    BEGIN();
    json_gen_start_array(&j);
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        json_gen_arr_set_double(&j, vals[i]);
    }
    json_gen_end_array(&j);
    TEST_ASSERT_GREATER_THAN(0, json_gen_str_end(&j));
    /* 15 digits where they suffice, 16 where 15 do not, 17 only when needed */
    TEST_ASSERT_EQUAL_STRING("[0.1,0.3333333333333333,1e+300,-0,1.2345678901234568e+17,"
                             "2.2250738585072014e-308,-1.779652973678931e+173,9007199254740992]", buf);
    char *p = buf + 1;
    for (size_t i = 0; i < sizeof(vals) / sizeof(vals[0]); i++) {
        char *end = NULL;
        double back = strtod(p, &end);
        TEST_ASSERT_TRUE_MESSAGE(memcmp(&back, &vals[i], sizeof(double)) == 0, "value did not round-trip");
        p = end + 1;
    }
    /* the text of a subnormal differs between C libraries; it only has to round-trip */
    BEGIN();
    json_gen_start_array(&j);
    json_gen_arr_set_double(&j, 5e-324);
    json_gen_end_array(&j);
    TEST_ASSERT_GREATER_THAN(0, json_gen_str_end(&j));
    TEST_ASSERT_TRUE(strtod(buf + 1, NULL) == 5e-324);
}

/* The same document for the measuring and the real pass, by construction */
static void write_measured_doc(json_gen_str_t *jstr)
{
    json_gen_start_object(jstr);
    json_gen_obj_set_string(jstr, "k", "\xc3\xa9t\xc3\xa9 \"quoted\"\n");
    json_gen_obj_set_double(jstr, "d", 0.1);
    json_gen_end_object(jstr);
}

TEST_CASE("measuring pass predicts the exact size", "[json_generator]")
{
    json_gen_str_start_measure(&j);
    write_measured_doc(&j);
    int need = json_gen_str_end(&j);

    char *exact = malloc(need);
    TEST_ASSERT_NOT_NULL(exact);
    json_gen_str_start(&j, exact, need, NULL, NULL);
    write_measured_doc(&j);
    TEST_ASSERT_EQUAL_INT(need, json_gen_str_end(&j));
    TEST_ASSERT_EQUAL_STRING("{\"k\":\"\xc3\xa9t\xc3\xa9 \\\"quoted\\\"\\n\",\"d\":0.1}", exact);
    TEST_ASSERT_EQUAL_INT(need, (int)strlen(exact) + 1);
    free(exact);

    /* a measuring pass reports errors too */
    json_gen_str_start_measure(&j);
    json_gen_start_array(&j);
    json_gen_arr_set_string(&j, "\xff");
    TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_INVALID_UTF8, json_gen_str_end(&j));
}

static char flushed[256];
static void flush_cb(char *chunk, void *priv)
{
    (void)priv;
    strcat(flushed, chunk);
}

TEST_CASE("flush callback with a tiny buffer reproduces the output", "[json_generator]")
{
    char tiny[8];
    flushed[0] = '\0';
    json_gen_str_start(&j, tiny, sizeof(tiny), flush_cb, NULL);
    json_gen_start_object(&j);
    json_gen_obj_set_string(&j, "key", "a\"b \xc3\xa9t\xc3\xa9");
    json_gen_obj_set_double(&j, "d", 1e300);
    json_gen_end_object(&j);
    TEST_ASSERT_GREATER_THAN(0, json_gen_str_end(&j));
    TEST_ASSERT_EQUAL_STRING("{\"key\":\"a\\\"b \xc3\xa9t\xc3\xa9\",\"d\":1e+300}", flushed);
}

TEST_CASE("NULL string value is an empty string", "[json_generator]")
{
    BEGIN();
    json_gen_start_object(&j);
    json_gen_obj_set_string(&j, "k", NULL);
    json_gen_end_object(&j);
    END_EXPECT("{\"k\":\"\"}");
}

TEST_CASE("pre-formatted object and array strings are spliced verbatim", "[json_generator]")
{
    BEGIN();
    json_gen_start_object(&j);
    json_gen_push_object_str(&j, "o", "{\"a\":1}");
    json_gen_push_array_str(&j, "a", "[1,2]");
    json_gen_end_object(&j);
    END_EXPECT("{\"o\":{\"a\":1},\"a\":[1,2]}");
}

TEST_CASE("empty element names and empty values", "[json_generator]")
{
    BEGIN();
    json_gen_start_object(&j);
    json_gen_obj_set_null(&j, "");
    json_gen_obj_set_string(&j, "", "");
    json_gen_push_object(&j, "");
    json_gen_pop_object(&j);
    json_gen_push_array(&j, "");
    json_gen_arr_set_string_len(&j, "", 0);
    json_gen_pop_array(&j);
    json_gen_end_object(&j);
    END_EXPECT("{\"\":null,\"\":\"\",\"\":{},\"\":[\"\"]}");
}

TEST_CASE("NULL with a length is an argument error", "[json_generator]")
{
    BEGIN();
    json_gen_start_array(&j);
    TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_INVALID_ARG, json_gen_arr_set_string_len(&j, NULL, 5));
    TEST_ASSERT_EQUAL_INT(JSON_GEN_ERR_INVALID_ARG, json_gen_str_end(&j));

    BEGIN();
    json_gen_start_array(&j);
    TEST_ASSERT_EQUAL_INT(JSON_GEN_OK, json_gen_arr_set_string_len(&j, NULL, 0));
    json_gen_end_array(&j);
    END_EXPECT("[\"\"]");
}

TEST_CASE("embedded NUL through the length variants", "[json_generator]")
{
    BEGIN();
    json_gen_start_object(&j);
    json_gen_obj_set_string_len(&j, "k", "a\0b", 3);
    json_gen_push_array(&j, "arr");
    json_gen_arr_set_string_len(&j, "\0", 1);
    json_gen_arr_set_string_len(&j, "not terminated", 3);
    json_gen_arr_start_long_string(&j, "x");
    json_gen_add_to_long_string_len(&j, "\0y", 2);
    json_gen_end_long_string(&j);
    json_gen_pop_array(&j);
    json_gen_end_object(&j);
    END_EXPECT("{\"k\":\"a\\u0000b\",\"arr\":[\"\\u0000\",\"not\",\"x\\u0000y\"]}");
}

/* End the current document and print it, or the error that stopped it */
static void emit_corpus_doc(void)
{
    int ret = json_gen_str_end(&j);
    if (ret < 0) {
        printf("@@ERR:%d\n", ret);
    } else {
        printf("@@JSON:%s\n", buf);
    }
}

/* Print a corpus of documents for the host-side round-trip check in
 * pytest_json_generator.py, which parses each one with a strict JSON parser
 * and compares the parsed value against what was intended. Order and count
 * must match the EXPECTED list there. */
static void print_corpus(void)
{
    struct {
        const char *key;
        const char *val;
    } strings[] = {
        { "k", "he said \"hi\"" },
        { "path", "C:\\temp" },
        { "k", "line1\nline2" },
        { "k", "a\x01" "b" },
        { "ev\"il", "x" },
        { "k", "\xc3\xa9t\xc3\xa9 \xe6\xb8\xac" },
        { "k", "bad\xc3 end" },
    };
    for (size_t i = 0; i < sizeof(strings) / sizeof(strings[0]); i++) {
        BEGIN();
        json_gen_start_object(&j);
        json_gen_obj_set_string(&j, strings[i].key, strings[i].val);
        json_gen_end_object(&j);
        emit_corpus_doc();
    }

    BEGIN();
    json_gen_start_object(&j);
    json_gen_obj_set_float(&j, "nan", NAN);
    json_gen_obj_set_float(&j, "inf", INFINITY);
    json_gen_obj_set_float(&j, "big", 1e30f);
    json_gen_obj_set_double(&j, "tenth", 0.1);
    json_gen_obj_set_double(&j, "huge", 1e300);
    json_gen_obj_set_double(&j, "tiny", 5e-324);
    json_gen_end_object(&j);
    emit_corpus_doc();

    BEGIN();
    json_gen_start_object(&j);
    json_gen_obj_start_long_string(&j, "k", "part1 \" ");
    json_gen_add_to_long_string(&j, "part2\xc3");
    json_gen_add_to_long_string(&j, "\xa9");
    json_gen_end_long_string(&j);
    json_gen_end_object(&j);
    emit_corpus_doc();

    printf("@@CORPUS_END\n");
}

void app_main(void)
{
    print_corpus();
    unity_run_menu();
}
