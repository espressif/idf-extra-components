/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "unity.h"
#include "cbor.h"
#include "cborjson.h"

#define CBOR_TEST_BUF_SIZE 256

static size_t encode_reference_document(uint8_t *buf, size_t buf_size)
{
    CborEncoder root, array, map, ip;

    cbor_encoder_init(&root, buf, buf_size, 0);
    TEST_ASSERT_EQUAL(CborNoError, cbor_encoder_create_array(&root, &array, 5));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encoder_create_map(&array, &map, 3));

    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_text_stringz(&map, "chip"));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_text_stringz(&map, "esp32"));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_text_stringz(&map, "unicore"));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_boolean(&map, false));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_text_stringz(&map, "ip"));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encoder_create_array(&map, &ip, 4));
    static const uint64_t octets[] = {192, 168, 1, 100};
    for (unsigned i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL(CborNoError, cbor_encode_uint(&ip, octets[i]));
    }
    TEST_ASSERT_EQUAL(CborNoError, cbor_encoder_close_container(&map, &ip));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encoder_close_container(&array, &map));

    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_float(&array, 3.14f));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_simple_value(&array, 99));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_text_stringz(&array, "2019-07-10 09:00:00+0000"));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_undefined(&array));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encoder_close_container(&root, &array));

    return cbor_encoder_get_buffer_size(&root, buf);
}

static CborError to_json(const uint8_t *buf, size_t buf_size, char *out, size_t out_size, int flags)
{
    CborParser parser;
    CborValue it;
    FILE *stream;
    CborError err;
    char *ptr = NULL;
    size_t sz = 0;

    TEST_ASSERT_GREATER_THAN(0, out_size);
    stream = open_memstream(&ptr, &sz);
    TEST_ASSERT_NOT_NULL(stream);
    err = cbor_parser_init(buf, buf_size, 0, &parser, &it);
    if (err == CborNoError) {
        err = cbor_value_to_json(stream, &it, flags);
    }
    TEST_ASSERT_EQUAL(0, fclose(stream));
    if (ptr != NULL) {
        snprintf(out, out_size, "%s", ptr);
        free(ptr);
    } else {
        out[0] = '\0';
    }
    return err;
}

/* Encoder, parser and cborjson on the document the example advertises. */
TEST_CASE("cbor: example document converts to JSON", "[cbor]")
{
    uint8_t buf[CBOR_TEST_BUF_SIZE] = {};
    char json[512] = {};
    size_t encoded = encode_reference_document(buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, encoded);

    TEST_ASSERT_EQUAL(CborNoError, to_json(buf, encoded, json, sizeof(json), CborConvertDefaultFlags));
    TEST_ASSERT_EQUAL_STRING("[{\"chip\":\"esp32\",\"unicore\":false,\"ip\":[192,168,1,100]},"
                             "3.1400001049041748,\"simple(99)\",\"2019-07-10 09:00:00+0000\",\"undefined\"]",
                             json);
}

/*
 * Default flags must not leak pretty-printer markers ("[_ 1,2]"). Downstream
 * such as esp_delta_ota parses this as JSON.
 */
TEST_CASE("cbor: to_json does not emit pretty encoding indicators", "[cbor]")
{
    uint8_t buf[CBOR_TEST_BUF_SIZE] = {};
    char json[64] = {};
    CborEncoder root, array;

    cbor_encoder_init(&root, buf, sizeof(buf), 0);
    TEST_ASSERT_EQUAL(CborNoError, cbor_encoder_create_array(&root, &array, CborIndefiniteLength));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_uint(&array, 1));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_uint(&array, 2));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encoder_close_container(&root, &array));

    TEST_ASSERT_EQUAL(CborNoError, to_json(buf, cbor_encoder_get_buffer_size(&root, buf),
                                           json, sizeof(json), CborConvertDefaultFlags));
    TEST_ASSERT_EQUAL_STRING("[1,2]", json);
}

/* v7 prints integers in full; UINT64_MAX is past JSON/IEEE754 exactness. */
TEST_CASE("cbor: to_json keeps UINT64_MAX as a decimal", "[cbor]")
{
    uint8_t buf[CBOR_TEST_BUF_SIZE] = {};
    char json[64] = {};
    CborEncoder root;

    cbor_encoder_init(&root, buf, sizeof(buf), 0);
    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_uint(&root, UINT64_MAX));

    TEST_ASSERT_EQUAL(CborNoError, to_json(buf, cbor_encoder_get_buffer_size(&root, buf),
                                           json, sizeof(json), CborConvertDefaultFlags));
    TEST_ASSERT_EQUAL_STRING("18446744073709551615", json);
}

/* Public cborjson flag; also the open_memstream() path stringify uses. */
TEST_CASE("cbor: to_json stringifies non-string map keys", "[cbor]")
{
    uint8_t buf[CBOR_TEST_BUF_SIZE] = {};
    char json[64] = {};
    CborEncoder root, map;

    cbor_encoder_init(&root, buf, sizeof(buf), 0);
    TEST_ASSERT_EQUAL(CborNoError, cbor_encoder_create_map(&root, &map, 1));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_uint(&map, 1));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encode_text_stringz(&map, "one"));
    TEST_ASSERT_EQUAL(CborNoError, cbor_encoder_close_container(&root, &map));

    TEST_ASSERT_EQUAL(CborNoError, to_json(buf, cbor_encoder_get_buffer_size(&root, buf),
                                           json, sizeof(json), CborConvertStringifyMapKeys));
    TEST_ASSERT_EQUAL_STRING("{\"1\":\"one\"}", json);
}

/* Truncated CBOR must fail rather than emit a partial JSON document. */
TEST_CASE("cbor: to_json reports truncated input", "[cbor]")
{
    static const uint8_t broken[] = {0x61}; /* text string of length 1, no payload */
    char json[64] = {};

    TEST_ASSERT_NOT_EQUAL(CborNoError, to_json(broken, sizeof(broken), json, sizeof(json),
                                               CborConvertDefaultFlags));
}
