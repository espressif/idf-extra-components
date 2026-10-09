/*
 * SPDX-FileCopyrightText: 2023-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <assert.h>
#include <string.h>
#include <math.h>
#include <float.h>
#include "json_parser.h"
#include "unity.h"

#define json_test_str   "{\n\"str_val\" :    \"JSON Parser\",\n" \
            "\t\"float_val\" : 2.0,\n" \
            "\"int_val\" : 2017,\n" \
            "\"bool_val\" : false,\n" \
            "\"supported_el\" :\t [\"bool\",\"int\","\
            "\"float\",\"str\"" \
            ",\"object\",\"array\"],\n" \
            "\"features\" : { \"objects\":true, "\
            "\"arrays\":\"yes\"},\n"\
            "\"int_64\":109174583252}"

static jparse_ctx_t jctx;
static char sbuf[128];

/* Parse a literal document; fails the test if it is rejected */
#define PARSE(doc) TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_parse_start(&jctx, doc, strlen(doc)))
#define END()      json_parse_end(&jctx)

TEST_CASE("json_parser basic tests", "[json_parser]")
{
    int ret = json_parse_start(&jctx, json_test_str, strlen(json_test_str));
    TEST_ASSERT_EQUAL(OS_SUCCESS, ret);

    char str_val[64];
    int int_val, num_elem;
    int64_t int64_val;
    bool bool_val;
    float float_val;

    TEST_ASSERT_EQUAL(OS_SUCCESS, json_obj_get_string(&jctx, "str_val", str_val, sizeof(str_val)));
    TEST_ASSERT_EQUAL_STRING("JSON Parser", str_val);

    TEST_ASSERT_EQUAL(OS_SUCCESS, json_obj_get_float(&jctx, "float_val", &float_val));
    TEST_ASSERT(fabs(float_val - 2.0f) < 0.0001f);

    TEST_ASSERT_EQUAL(OS_SUCCESS, json_obj_get_int(&jctx, "int_val", &int_val));
    TEST_ASSERT_EQUAL_INT(2017, int_val);

    TEST_ASSERT_EQUAL(OS_SUCCESS, json_obj_get_bool(&jctx, "bool_val", &bool_val));
    TEST_ASSERT_EQUAL(false, bool_val);

    TEST_ASSERT_EQUAL(OS_SUCCESS, json_obj_get_array(&jctx, "supported_el", &num_elem));
    const char *expected_values[] = {"bool", "int", "float", "str", "object", "array"};
    TEST_ASSERT_EQUAL(sizeof(expected_values) / sizeof(expected_values[0]), num_elem);
    for (int i = 0; i < num_elem; ++i) {
        TEST_ASSERT_EQUAL(OS_SUCCESS, json_arr_get_string(&jctx, i, str_val, sizeof(str_val)));
        TEST_ASSERT_EQUAL_STRING(expected_values[i], str_val);
    }
    json_obj_leave_array(&jctx);

    TEST_ASSERT_EQUAL(OS_SUCCESS, json_obj_get_object(&jctx, "features"));
    TEST_ASSERT_EQUAL(OS_SUCCESS, json_obj_get_bool(&jctx, "objects", &bool_val));
    TEST_ASSERT_EQUAL(true, bool_val);
    TEST_ASSERT_EQUAL(OS_SUCCESS, json_obj_get_string(&jctx, "arrays", str_val, sizeof(str_val)));
    TEST_ASSERT_EQUAL_STRING("yes", str_val);
    json_obj_leave_object(&jctx);

    TEST_ASSERT_EQUAL(OS_SUCCESS, json_obj_get_int64(&jctx, "int_64", &int64_val));
    TEST_ASSERT_EQUAL_INT64(109174583252, int64_val);

    json_parse_end(&jctx);
}

TEST_CASE("strings are returned decoded", "[json_parser]")
{
    int len;
    PARSE("{\"esc\":\"a\\\"b\\\\c\\/d\\be\\ff\\ng\\rh\\ti\","
          "\"bmp\":\"r\\u00e9sum\\u00e9 \\u6e2c\",\"astral\":\"\\ud83d\\ude00\",\"raw\":\"\xc3\xa9t\xc3\xa9\"}");
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_string(&jctx, "esc", sbuf, sizeof(sbuf)));
    TEST_ASSERT_EQUAL_STRING("a\"b\\c/d\be\ff\ng\rh\ti", sbuf);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_string(&jctx, "bmp", sbuf, sizeof(sbuf)));
    TEST_ASSERT_EQUAL_STRING("r\xc3\xa9sum\xc3\xa9 \xe6\xb8\xac", sbuf);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_string(&jctx, "astral", sbuf, sizeof(sbuf)));
    TEST_ASSERT_EQUAL_STRING("\xf0\x9f\x98\x80", sbuf);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_string(&jctx, "raw", sbuf, sizeof(sbuf)));
    TEST_ASSERT_EQUAL_STRING("\xc3\xa9t\xc3\xa9", sbuf);
    /* strlen is the decoded length, in bytes */
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_strlen(&jctx, "esc", &len));
    TEST_ASSERT_EQUAL_INT(17, len);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_strlen(&jctx, "astral", &len));
    TEST_ASSERT_EQUAL_INT(4, len);
    END();
}

TEST_CASE("a buffer too small for the decoded string is refused", "[json_parser]")
{
    char tiny[4];
    PARSE("{\"k\":\"\\u00e9\\u00e9\"}");   /* 4 bytes decoded, needs 5 */
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_string(&jctx, "k", tiny, sizeof(tiny)));
    char fit[5];
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_string(&jctx, "k", fit, sizeof(fit)));
    TEST_ASSERT_EQUAL_STRING("\xc3\xa9\xc3\xa9", fit);
    END();
    /* even an empty string needs room for its terminator */
    PARSE("{\"e\":\"\"}");
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_string(&jctx, "e", tiny, 0));
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_string(&jctx, "e", tiny, 1));
    TEST_ASSERT_EQUAL_STRING("", tiny);
    END();
}

TEST_CASE("lone surrogate escapes and U+0000 cannot be decoded", "[json_parser]")
{
    int len;
    /* syntactically valid (RFC 8259 leaves it to the implementation), but not decodable */
    PARSE("{\"hi\":\"\\ud800\",\"lo\":\"\\udc00x\",\"pair\":\"\\ud83d\\u0041\",\"nul\":\"a\\u0000b\"}");   /* codespell:ignore nul */
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_string(&jctx, "hi", sbuf, sizeof(sbuf)));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_strlen(&jctx, "hi", &len));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_string(&jctx, "lo", sbuf, sizeof(sbuf)));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_string(&jctx, "pair", sbuf, sizeof(sbuf)));
    /* a C string cannot carry U+0000: fail rather than return a truncated value */
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_string(&jctx, "nul", sbuf, sizeof(sbuf)));   /* codespell:ignore nul */
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_strlen(&jctx, "nul", &len));                 /* codespell:ignore nul */
    END();
}

TEST_CASE("names are matched decoded", "[json_parser]")
{
    int v;
    PARSE("{\"r\\u00e9sum\\u00e9\":1,\"a\\\"b\":2,\"\\ud83d\\ude00\":3,\"\":4}");
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_int(&jctx, "r\xc3\xa9sum\xc3\xa9", &v));
    TEST_ASSERT_EQUAL_INT(1, v);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_int(&jctx, "a\"b", &v));
    TEST_ASSERT_EQUAL_INT(2, v);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_int(&jctx, "\xf0\x9f\x98\x80", &v));
    TEST_ASSERT_EQUAL_INT(3, v);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_int(&jctx, "", &v));
    TEST_ASSERT_EQUAL_INT(4, v);
    /* the raw spelling of an escaped name is not a match */
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_int(&jctx, "r\\u00e9sum\\u00e9", &v));
    /* a prefix is not a match either */
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_int(&jctx, "r\xc3\xa9sum", &v));
    END();
}

TEST_CASE("a repeated name returns its first occurrence", "[json_parser]")
{
    int v;
    PARSE("{\"a\":1,\"a\":2}");
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_int(&jctx, "a", &v));
    TEST_ASSERT_EQUAL_INT(1, v);
    END();
}

TEST_CASE("numbers are range checked per type", "[json_parser]")
{
    int i;
    int64_t l;
    double d;
    float f;
    PARSE("{\"max\":2147483647,\"min\":-2147483648,\"over\":2147483648,\"under\":-2147483649,"
          "\"l\":9223372036854775807,\"lover\":9223372036854775808,\"neg\":-1,"
          "\"f\":1.5,\"e\":1e3,\"huge\":1e400,\"fhuge\":1e39,\"tiny\":5e-324,\"z\":-0}");
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_int(&jctx, "max", &i));
    TEST_ASSERT_EQUAL_INT(2147483647, i);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_int(&jctx, "min", &i));
    TEST_ASSERT_EQUAL_INT(-2147483647 - 1, i);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_int(&jctx, "neg", &i));
    TEST_ASSERT_EQUAL_INT(-1, i);
    /* does not fit an int: an error, not a wrapped value */
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_int(&jctx, "over", &i));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_int(&jctx, "under", &i));
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_int64(&jctx, "over", &l));
    TEST_ASSERT_TRUE(l == 2147483648LL);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_int64(&jctx, "l", &l));
    TEST_ASSERT_TRUE(l == 9223372036854775807LL);
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_int64(&jctx, "lover", &l));
    /* a real number is not an int */
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_int(&jctx, "f", &i));
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_double(&jctx, "f", &d));
    TEST_ASSERT_TRUE(d == 1.5);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_double(&jctx, "e", &d));
    TEST_ASSERT_TRUE(d == 1000.0);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_double(&jctx, "neg", &d));
    TEST_ASSERT_TRUE(d == -1.0);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_double(&jctx, "tiny", &d));
    TEST_ASSERT_TRUE(d > 0.0 && d < 1e-300);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_double(&jctx, "z", &d));
    TEST_ASSERT_TRUE(d == 0.0);
    /* beyond the type's range */
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_double(&jctx, "huge", &d));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_float(&jctx, "fhuge", &f));
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_float(&jctx, "f", &f));
    TEST_ASSERT_TRUE(f == 1.5f);
    END();
}

TEST_CASE("booleans are only true and false, null has its own accessor", "[json_parser]")
{
    bool b;
    int i;
    PARSE("{\"t\":true,\"f\":false,\"one\":1,\"n\":null,\"s\":\"true\"}");
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_bool(&jctx, "t", &b));
    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_bool(&jctx, "f", &b));
    TEST_ASSERT_FALSE(b);
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_bool(&jctx, "one", &b));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_bool(&jctx, "s", &b));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_int(&jctx, "t", &i));
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_null(&jctx, "n"));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_null(&jctx, "t"));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_null(&jctx, "missing"));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_obj_get_string(&jctx, "n", sbuf, sizeof(sbuf)));
    END();
}

TEST_CASE("array elements of every type", "[json_parser]")
{
    int n, i;
    int64_t l;
    double d;
    bool b;
    PARSE("[\"x\\u0041\",7,-9223372036854775808,2.5,true,null,[1],{\"k\":\"v\"}]");
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_get_string(&jctx, 0, sbuf, sizeof(sbuf)));
    TEST_ASSERT_EQUAL_STRING("xA", sbuf);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_get_strlen(&jctx, 0, &n));
    TEST_ASSERT_EQUAL_INT(2, n);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_get_int(&jctx, 1, &i));
    TEST_ASSERT_EQUAL_INT(7, i);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_get_int64(&jctx, 2, &l));
    TEST_ASSERT_TRUE(l == (-9223372036854775807LL - 1));
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_get_double(&jctx, 3, &d));
    TEST_ASSERT_TRUE(d == 2.5);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_get_bool(&jctx, 4, &b));
    TEST_ASSERT_TRUE(b);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_get_null(&jctx, 5));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_arr_get_null(&jctx, 4));
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_get_array(&jctx, 6));
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_get_int(&jctx, 0, &i));
    TEST_ASSERT_EQUAL_INT(1, i);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_leave_array(&jctx));
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_get_object(&jctx, 7));
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_string(&jctx, "k", sbuf, sizeof(sbuf)));
    TEST_ASSERT_EQUAL_STRING("v", sbuf);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_leave_object(&jctx));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_arr_get_int(&jctx, 8, &i));
    END();
}

TEST_CASE("a JSON text that is a single value", "[json_parser]")
{
    int i, n;
    int64_t l;
    double d;
    bool b;
    PARSE("\"as\\u0064\"");
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_root_get_string(&jctx, sbuf, sizeof(sbuf)));
    TEST_ASSERT_EQUAL_STRING("asd", sbuf);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_root_get_strlen(&jctx, &n));
    TEST_ASSERT_EQUAL_INT(3, n);
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_root_get_int(&jctx, &i));
    END();
    /* a number that ends the text is read from a buffer that is not NUL
     * terminated: the byte after it must not be looked at */
    char num[] = "42x";
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_parse_start(&jctx, num, 2));
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_root_get_int(&jctx, &i));
    TEST_ASSERT_EQUAL_INT(42, i);
    float f;
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_root_get_float(&jctx, &f));
    TEST_ASSERT_EQUAL_FLOAT(42.0f, f);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_root_get_int64(&jctx, &l));
    TEST_ASSERT_TRUE(l == 42);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_root_get_double(&jctx, &d));
    TEST_ASSERT_TRUE(d == 42.0);
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_root_get_string(&jctx, sbuf, sizeof(sbuf)));
    END();
    /* a long number that ends the text is still converted whole */
    char small[90] = "0.";
    memset(small + 2, '0', 80);
    strcpy(small + 82, "1");
    PARSE(small);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_root_get_double(&jctx, &d));
    TEST_ASSERT_TRUE(d > 0.9e-81 && d < 1.1e-81);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_root_get_float(&jctx, &f));
    TEST_ASSERT_EQUAL_FLOAT(0.0f, f);   /* underflows to zero, which is not an error */
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_root_get_int(&jctx, &i));
    END();
    PARSE(" -0.5 ");
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_root_get_double(&jctx, &d));
    TEST_ASSERT_TRUE(d == -0.5);
    END();
    PARSE("true");
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_root_get_bool(&jctx, &b));
    TEST_ASSERT_TRUE(b);
    END();
    PARSE("null");
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_root_get_null(&jctx));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_root_get_bool(&jctx, &b));
    END();
    /* root accessors do not apply to an object or array document */
    PARSE("{\"a\":1}");
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_root_get_int(&jctx, &i));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_root_get_null(&jctx));
    END();
}

TEST_CASE("invalid documents are rejected", "[json_parser]")
{
    const char *bad[] = {
        "", " ", "[1,]", "{\"a\":1,}", "[1 2]", "{\"a\" 1}", "{\"a\":}", "[,1]", "[1,,2]",
        "{\"a\"::1}", "{1:2}", "{\"a\":1}}", "[", "]", "[1]]", "[1]x", "[][]", "{}[]", "1 2",
        "01", "-01", "1.", ".5", "-", "-.5", "1e", "1e+", "1.0e", "0x1", "1_000", "+1", "1 000", "-NaN", "Infinity",
        "tru", "fals", "nul", "True", "nulL", "[t]",   /* codespell:ignore tru,fals,nul */
        "\"a\nb\"", "\"a\tb\"", "\"\x01\"", "\"\\x41\"", "\"\\u00g0\"", "\"\\u12\"", "\"abc",
        "\"\xc3\"", "\"\xc0\x80\"", "\"\xed\xa0\x80\"", "\"\xf4\x90\x80\x80\"", "\"\xff\"", "\"\x80\"",
        "[\xc3\xa9]", "{\"a\":1}\xef\xbb\xbf", "[1]\f", "\f[1]",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(-OS_FAIL, json_parse_start(&jctx, bad[i], strlen(bad[i])), bad[i]);
    }
    /* a NUL byte inside the given length is not an end-of-text marker */
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_parse_start(&jctx, "[1]\0", 4));
}

TEST_CASE("valid documents are accepted", "[json_parser]")
{
    const char *good[] = {
        "[]", "{}", "[[[]]]", "{\"a\":{}}", " \t\r\n[ 1 , 2 ]\n", "{\"a\":[1,{\"b\":null}],\"c\":\"\"}",
        "0", "-0", "1e-3", "0.5E+2", "-1.25e10", "1E5", "123456789012345678901234567890",
        "\"\"", "\" \"", "\"\\u0000\"", "\"\\ud800\"", "\"\xf4\x8f\xbf\xbf\"", "\"\xe0\xa0\x80\"", "\"\xed\x9f\xbf\"",
        "[\"\\/\", \"\\b\\f\\n\\r\\t\"]", "{\"\":\"\"}", "{\"a\":1,\"a\":2}",
    };
    for (size_t i = 0; i < sizeof(good) / sizeof(good[0]); i++) {
        TEST_ASSERT_EQUAL_INT_MESSAGE(OS_SUCCESS, json_parse_start(&jctx, good[i], strlen(good[i])), good[i]);
        END();
    }
    /* 32 levels of nesting: the tokenizer is iterative, so depth costs tokens, not stack */
    char deep[32 + 1 + 32 + 1];
    memset(deep, '[', 32);
    deep[32] = '1';
    memset(deep + 33, ']', 32);
    deep[65] = '\0';
    PARSE(deep);
    for (int d = 0; d < 31; d++) {
        TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_get_array(&jctx, 0));
    }
    int one;
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_get_int(&jctx, 0, &one));
    TEST_ASSERT_EQUAL_INT(1, one);
    END();
}

TEST_CASE("nested object and array text is returned verbatim", "[json_parser]")
{
    int len;
    PARSE("{\"o\":{ \"a\" : [1, 2] },\"arr\":[ \"x\\u0041\" ]}");
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_object_str(&jctx, "o", sbuf, sizeof(sbuf)));
    TEST_ASSERT_EQUAL_STRING("{ \"a\" : [1, 2] }", sbuf);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_object_strlen(&jctx, "o", &len));
    TEST_ASSERT_EQUAL_INT(16, len);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_array_str(&jctx, "arr", sbuf, sizeof(sbuf)));
    TEST_ASSERT_EQUAL_STRING("[ \"x\\u0041\" ]", sbuf);
    END();
}

TEST_CASE("a value can be read in place through its token", "[json_parser]")
{
    PARSE("{\"s\":\"a\\\"b\",\"n\":42,\"arr\":[\"x\",null]}");
    json_tok_t *tok = json_obj_get_val_tok(&jctx, "s", JSMN_STRING);
    TEST_ASSERT_NOT_NULL(tok);
    TEST_ASSERT_EQUAL_STRING_LEN("a\\\"b", jctx.js + tok->start, tok->end - tok->start);   /* raw: escape intact */
    TEST_ASSERT_NULL(json_obj_get_val_tok(&jctx, "s", JSMN_PRIMITIVE));
    TEST_ASSERT_NULL(json_obj_get_val_tok(&jctx, "missing", JSMN_UNDEFINED));
    tok = json_obj_get_val_tok(&jctx, "n", JSMN_UNDEFINED);                         /* any type */
    TEST_ASSERT_NOT_NULL(tok);
    TEST_ASSERT_EQUAL_INT(JSMN_PRIMITIVE, tok->type);
    TEST_ASSERT_EQUAL_STRING_LEN("42", jctx.js + tok->start, tok->end - tok->start);
    int n;
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_array(&jctx, "arr", &n));
    tok = json_arr_get_val_tok(&jctx, 0, JSMN_STRING);
    TEST_ASSERT_NOT_NULL(tok);
    TEST_ASSERT_EQUAL_STRING_LEN("x", jctx.js + tok->start, tok->end - tok->start);
    TEST_ASSERT_NOT_NULL(json_arr_get_val_tok(&jctx, 1, JSMN_PRIMITIVE));
    TEST_ASSERT_NULL(json_arr_get_val_tok(&jctx, 2, JSMN_UNDEFINED));
    END();
}

TEST_CASE("static token buffer", "[json_parser]")
{
    json_tok_t toks[8];
    int i;
    const char *doc = "{\"a\":[1,2,3]}";   /* 6 tokens: object, name, array, 1, 2, 3 */
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_parse_start_static(&jctx, doc, strlen(doc), toks, 6));
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_obj_get_array(&jctx, "a", &i));
    TEST_ASSERT_EQUAL_INT(3, i);
    TEST_ASSERT_EQUAL_INT(OS_SUCCESS, json_arr_get_int(&jctx, 2, &i));
    TEST_ASSERT_EQUAL_INT(3, i);
    json_parse_end_static(&jctx);
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_parse_start_static(&jctx, doc, strlen(doc), toks, 5));
    TEST_ASSERT_EQUAL_INT(-OS_FAIL, json_parse_start_static(&jctx, "[1,]", 4, toks, 8));
}
