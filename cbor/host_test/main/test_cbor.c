/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "cbor.h"
#include "cborjson.h"

#define CBOR_TEST_BUF_SIZE 256

/* Encode [{"chip": "esp32", "unicore": false, "ip": [192, 168, 1, 100]},
 * 3.14, 99(simple), "2019-07-10 09:00:00+0000", undefined]. */
static size_t encode_reference_document(uint8_t *buf, size_t buf_size)
{
    CborEncoder root, array, map, ip;
    CborError err;

    cbor_encoder_init(&root, buf, buf_size, 0);

    err = cbor_encoder_create_array(&root, &array, 5);
    TEST_ASSERT_EQUAL(CborNoError, err);

    err = cbor_encoder_create_map(&array, &map, 3);
    TEST_ASSERT_EQUAL(CborNoError, err);

    // chip: "esp32"
    err = cbor_encode_text_stringz(&map, "chip");
    TEST_ASSERT_EQUAL(CborNoError, err);
    err = cbor_encode_text_stringz(&map, "esp32");
    TEST_ASSERT_EQUAL(CborNoError, err);

    // unicore: false
    err = cbor_encode_text_stringz(&map, "unicore");
    TEST_ASSERT_EQUAL(CborNoError, err);
    err = cbor_encode_boolean(&map, false);
    TEST_ASSERT_EQUAL(CborNoError, err);

    // ip: [192, 168, 1, 100]
    err = cbor_encode_text_stringz(&map, "ip");
    TEST_ASSERT_EQUAL(CborNoError, err);
    err = cbor_encoder_create_array(&map, &ip, 4);
    TEST_ASSERT_EQUAL(CborNoError, err);
    static const uint64_t octets[] = {192, 168, 1, 100};
    for (unsigned i = 0; i < 4; i++) {
        err = cbor_encode_uint(&ip, octets[i]);
        TEST_ASSERT_EQUAL(CborNoError, err);
    }
    err = cbor_encoder_close_container(&map, &ip);
    TEST_ASSERT_EQUAL(CborNoError, err);
    err = cbor_encoder_close_container(&array, &map);
    TEST_ASSERT_EQUAL(CborNoError, err);

    // 3.14, encoded as a single precision float
    err = cbor_encode_float(&array, 3.14f);
    TEST_ASSERT_EQUAL(CborNoError, err);

    // 99, encoded as a simple value
    err = cbor_encode_simple_value(&array, 99);
    TEST_ASSERT_EQUAL(CborNoError, err);

    // "2019-07-10 09:00:00+0000"
    err = cbor_encode_text_stringz(&array, "2019-07-10 09:00:00+0000");
    TEST_ASSERT_EQUAL(CborNoError, err);

    // undefined
    err = cbor_encode_undefined(&array);
    TEST_ASSERT_EQUAL(CborNoError, err);

    err = cbor_encoder_close_container(&root, &array);
    TEST_ASSERT_EQUAL(CborNoError, err);

    return cbor_encoder_get_buffer_size(&root, buf);
}

/* Convert the encoded document in `buf` to JSON into `out`. */
static void to_json(const uint8_t *buf, size_t buf_size, char *out, size_t out_size, int flags)
{
    CborParser parser;
    CborValue it;
    FILE *stream;
    CborError err;

    stream = fmemopen(out, out_size, "w");
    TEST_ASSERT_NOT_NULL(stream);

    err = cbor_parser_init(buf, buf_size, 0, &parser, &it);
    TEST_ASSERT_EQUAL(CborNoError, err);

    err = cbor_value_to_json(stream, &it, flags);
    TEST_ASSERT_EQUAL(CborNoError, err);

    TEST_ASSERT_EQUAL(0, fclose(stream));
}

TEST_CASE("cbor: encoder and parser round trip", "[cbor]")
{
    uint8_t buf[CBOR_TEST_BUF_SIZE] = {};
    CborParser parser;
    CborValue it, recursed;
    CborError err;
    size_t encoded;

    encoded = encode_reference_document(buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, encoded);

    err = cbor_parser_init(buf, encoded, 0, &parser, &it);
    TEST_ASSERT_EQUAL(CborNoError, err);

    // Outermost array with 5 elements
    TEST_ASSERT_TRUE(cbor_value_is_array(&it));
    err = cbor_value_enter_container(&it, &recursed);
    TEST_ASSERT_EQUAL(CborNoError, err);

    // 1. the map
    TEST_ASSERT_TRUE(cbor_value_is_map(&recursed));
    CborValue map;
    err = cbor_value_enter_container(&recursed, &map);
    TEST_ASSERT_EQUAL(CborNoError, err);

    char key[16] = {};
    size_t key_len = sizeof(key);
    err = cbor_value_copy_text_string(&map, key, &key_len, &map);
    TEST_ASSERT_EQUAL(CborNoError, err);
    TEST_ASSERT_EQUAL_STRING("chip", key);
    char chip[16] = {};
    size_t chip_len = sizeof(chip);
    err = cbor_value_copy_text_string(&map, chip, &chip_len, &map);
    TEST_ASSERT_EQUAL(CborNoError, err);
    TEST_ASSERT_EQUAL_STRING("esp32", chip);

    bool unicore = true;
    key_len = sizeof(key);
    err = cbor_value_copy_text_string(&map, key, &key_len, &map);
    TEST_ASSERT_EQUAL(CborNoError, err);
    TEST_ASSERT_EQUAL_STRING("unicore", key);
    err = cbor_value_get_boolean(&map, &unicore);
    TEST_ASSERT_EQUAL(CborNoError, err);
    TEST_ASSERT_FALSE(unicore);
    err = cbor_value_advance_fixed(&map);
    TEST_ASSERT_EQUAL(CborNoError, err);

    key_len = sizeof(key);
    err = cbor_value_copy_text_string(&map, key, &key_len, &map);
    TEST_ASSERT_EQUAL(CborNoError, err);
    TEST_ASSERT_EQUAL_STRING("ip", key);
    TEST_ASSERT_TRUE(cbor_value_is_array(&map));
    CborValue ip;
    err = cbor_value_enter_container(&map, &ip);
    TEST_ASSERT_EQUAL(CborNoError, err);
    static const uint64_t expected_octets[] = {192, 168, 1, 100};
    for (unsigned i = 0; i < 4; i++) {
        uint64_t value = 0;
        err = cbor_value_get_uint64(&ip, &value);
        TEST_ASSERT_EQUAL(CborNoError, err);
        /* Cast to int: unity only asserts 64-bit values with
         * CONFIG_UNITY_ENABLE_64BIT, which is off by default. */
        TEST_ASSERT_EQUAL((int)expected_octets[i], (int)value);
        cbor_value_advance_fixed(&ip);
    }
    err = cbor_value_leave_container(&map, &ip);
    TEST_ASSERT_EQUAL(CborNoError, err);
    err = cbor_value_leave_container(&recursed, &map);
    TEST_ASSERT_EQUAL(CborNoError, err);

    // 2. the float
    TEST_ASSERT_TRUE(cbor_value_is_float(&recursed));
    float f = 0.0f;
    err = cbor_value_get_float(&recursed, &f);
    TEST_ASSERT_EQUAL(CborNoError, err);
    TEST_ASSERT_EQUAL_FLOAT(3.14f, f);
    cbor_value_advance_fixed(&recursed);

    // 3. the simple value
    TEST_ASSERT_TRUE(cbor_value_is_simple_type(&recursed));
    uint8_t simple = 0;
    err = cbor_value_get_simple_type(&recursed, &simple);
    TEST_ASSERT_EQUAL(CborNoError, err);
    TEST_ASSERT_EQUAL_UINT8(99, simple);
    cbor_value_advance_fixed(&recursed);

    // 4. the text string
    char text[32] = {};
    size_t text_len = sizeof(text);
    err = cbor_value_copy_text_string(&recursed, text, &text_len, &recursed);
    TEST_ASSERT_EQUAL(CborNoError, err);
    TEST_ASSERT_EQUAL_STRING("2019-07-10 09:00:00+0000", text);

    // 5. undefined
    TEST_ASSERT_TRUE(cbor_value_is_undefined(&recursed));
    cbor_value_advance_fixed(&recursed);

    TEST_ASSERT_TRUE(cbor_value_at_end(&recursed));
    err = cbor_value_leave_container(&it, &recursed);
    TEST_ASSERT_EQUAL(CborNoError, err);
    TEST_ASSERT_TRUE(cbor_value_at_end(&it));
}

TEST_CASE("cbor: cbor_to_json produces valid JSON", "[cbor]")
{
    uint8_t buf[CBOR_TEST_BUF_SIZE] = {};
    char json[512] = {};
    size_t encoded = encode_reference_document(buf, sizeof(buf));

    to_json(buf, encoded, json, sizeof(json), CborConvertDefaultFlags);

    TEST_ASSERT_EQUAL_STRING("[{\"chip\":\"esp32\",\"unicore\":false,\"ip\":[192,168,1,100]},"
                             "3.1400001049041748,\"simple(99)\",\"2019-07-10 09:00:00+0000\",\"undefined\"]",
                             json);
}

/*
 * Upstream 7.0 prints integers, booleans and null through the pretty printer to
 * keep the full integer precision. Its default flags only apply to containers, so
 * the output stays plain JSON: "[1,2]" and "42", not "[_ 1,2]" or "42_0".
 * Downstream components such as esp_delta_ota parse this output strictly.
 */
TEST_CASE("cbor: to_json produces JSON without pretty encoding indicators", "[cbor]")
{
    uint8_t buf[CBOR_TEST_BUF_SIZE] = {};
    char json[128] = {};
    size_t encoded;

    /* Indeterminate length array */
    {
        CborEncoder root, array;
        cbor_encoder_init(&root, buf, sizeof(buf), 0);
        TEST_ASSERT_EQUAL(CborNoError, cbor_encoder_create_array(&root, &array, CborIndefiniteLength));
        TEST_ASSERT_EQUAL(CborNoError, cbor_encode_uint(&array, 1));
        TEST_ASSERT_EQUAL(CborNoError, cbor_encode_uint(&array, 2));
        TEST_ASSERT_EQUAL(CborNoError, cbor_encoder_close_container(&root, &array));
        encoded = cbor_encoder_get_buffer_size(&root, buf);
    }
    to_json(buf, encoded, json, sizeof(json), CborConvertDefaultFlags);
    TEST_ASSERT_EQUAL_STRING("[1,2]", json);

    /* An integer encoded with a longer form than needed */
    {
        CborEncoder root;
        cbor_encoder_init(&root, buf, sizeof(buf), 0);
        TEST_ASSERT_EQUAL(CborNoError, cbor_encode_int(&root, 42));
        encoded = cbor_encoder_get_buffer_size(&root, buf);
    }
    to_json(buf, encoded, json, sizeof(json), CborConvertDefaultFlags);
    TEST_ASSERT_EQUAL_STRING("42", json);
}

/*
 * CborConvertStringifyMapKeys stringifies non-string map keys through
 * open_memstream(), which the host libc provides natively and the chip targets
 * get from the component's fallback.
 */
TEST_CASE("cbor: to_json stringifies non-string map keys", "[cbor]")
{
    uint8_t buf[CBOR_TEST_BUF_SIZE] = {};
    char json[128] = {};
    CborEncoder root, map;
    CborError err;

    cbor_encoder_init(&root, buf, sizeof(buf), 0);
    err = cbor_encoder_create_map(&root, &map, 2);
    TEST_ASSERT_EQUAL(CborNoError, err);
    err = cbor_encode_uint(&map, 1);
    TEST_ASSERT_EQUAL(CborNoError, err);
    err = cbor_encode_text_stringz(&map, "one");
    TEST_ASSERT_EQUAL(CborNoError, err);
    err = cbor_encode_float(&map, 1.5f);
    TEST_ASSERT_EQUAL(CborNoError, err);
    err = cbor_encode_text_stringz(&map, "one and a half");
    TEST_ASSERT_EQUAL(CborNoError, err);
    err = cbor_encoder_close_container(&root, &map);
    TEST_ASSERT_EQUAL(CborNoError, err);

    to_json(buf, cbor_encoder_get_buffer_size(&root, buf), json, sizeof(json),
            CborConvertStringifyMapKeys);

    /* Note the "1.5f" key: upstream keeps the float encoding indicator in the
     * stringified key. The key stays quoted and escaped, so the JSON is valid. */
    TEST_ASSERT_EQUAL_STRING("{\"1\":\"one\",\"1.5f\":\"one and a half\"}", json);
}

/* The full precision of 64-bit integers has to survive the conversion. */
TEST_CASE("cbor: to_json keeps the full 64-bit precision", "[cbor]")
{
    uint8_t buf[CBOR_TEST_BUF_SIZE] = {};
    char json[128] = {};
    CborEncoder root;
    CborError err;

    cbor_encoder_init(&root, buf, sizeof(buf), 0);
    err = cbor_encode_uint(&root, UINT64_MAX);
    TEST_ASSERT_EQUAL(CborNoError, err);

    to_json(buf, cbor_encoder_get_buffer_size(&root, buf), json, sizeof(json),
            CborConvertDefaultFlags);

    TEST_ASSERT_EQUAL_STRING("18446744073709551615", json);
}

/* Conversion errors have to be reported instead of producing invalid JSON. */
TEST_CASE("cbor: to_json reports invalid input", "[cbor]")
{
    /* A truncated definite length text string: "\x61" means one byte follows
     * but the document ends right after it. */
    static const uint8_t broken[] = {0x61};
    CborParser parser;
    CborValue it;
    char json[64] = {};
    FILE *stream;
    CborError err;

    stream = fmemopen(json, sizeof(json), "w");
    TEST_ASSERT_NOT_NULL(stream);

    err = cbor_parser_init(broken, sizeof(broken), 0, &parser, &it);
    TEST_ASSERT_EQUAL(CborNoError, err);

    err = cbor_value_to_json(stream, &it, CborConvertDefaultFlags);
    TEST_ASSERT_NOT_EQUAL(CborNoError, err);

    TEST_ASSERT_EQUAL(0, fclose(stream));
}
