/*
 * SPDX-FileCopyrightText: 2022-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_check.h"
#include "cbor.h"
#include "cborjson.h"

static const char *TAG = "example";

static void indent(int nestingLevel)
{
    while (nestingLevel--) {
        printf("  ");
    }
}

static void dumpbytes(const uint8_t *buf, size_t len)
{
    while (len--) {
        printf("%02X ", *buf++);
    }
}

/**
 * Decode CBOR data manually
 */
static CborError example_dump_cbor_buffer(CborValue *it, int nestingLevel)
{
    while (!cbor_value_at_end(it)) {
        CborType type = cbor_value_get_type(it);
        CborError err;

        indent(nestingLevel);
        switch (type) {
        case CborArrayType: {
            CborValue recursed;
            assert(cbor_value_is_container(it));
            puts("Array[");
            err = cbor_value_enter_container(it, &recursed);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "enter container failed");
            err = example_dump_cbor_buffer(&recursed, nestingLevel + 1);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "recursive dump failed");
            err = cbor_value_leave_container(it, &recursed);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "leave container failed");
            indent(nestingLevel);
            puts("]");
            continue;
        }
        case CborMapType: {
            CborValue recursed;
            assert(cbor_value_is_container(it));
            puts("Map{");
            err = cbor_value_enter_container(it, &recursed);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "enter container failed");
            err = example_dump_cbor_buffer(&recursed, nestingLevel + 1);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "recursive dump failed");
            err = cbor_value_leave_container(it, &recursed);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "leave container failed");
            indent(nestingLevel);
            puts("}");
            continue;
        }
        case CborIntegerType: {
            int64_t val;
            err = cbor_value_get_int64(it, &val);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "parse int64 failed");
            printf("%lld\n", (long long)val);
            break;
        }
        case CborByteStringType: {
            uint8_t *buf;
            size_t n;
            err = cbor_value_dup_byte_string(it, &buf, &n, it);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "parse byte string failed");
            dumpbytes(buf, n);
            puts("");
            free(buf);
            continue;
        }
        case CborTextStringType: {
            char *buf;
            size_t n;
            err = cbor_value_dup_text_string(it, &buf, &n, it);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "parse text string failed");
            puts(buf);
            free(buf);
            continue;
        }
        case CborTagType: {
            CborTag tag;
            err = cbor_value_get_tag(it, &tag);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "parse tag failed");
            printf("Tag(%lld)\n", (long long)tag);
            break;
        }
        case CborSimpleType: {
            uint8_t simple;
            err = cbor_value_get_simple_type(it, &simple);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "parse simple type failed");
            printf("simple(%u)\n", simple);
            break;
        }
        case CborNullType:
            puts("null");
            break;
        case CborUndefinedType:
            puts("undefined");
            break;
        case CborBooleanType: {
            bool val;
            err = cbor_value_get_boolean(it, &val);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "parse boolean type failed");
            puts(val ? "true" : "false");
            break;
        }
        case CborHalfFloatType: {
            uint16_t val;
            err = cbor_value_get_half_float(it, &val);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "parse half float failed");
            printf("__f16(%04x)\n", val);
            break;
        }
        case CborFloatType: {
            float val;
            err = cbor_value_get_float(it, &val);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "parse float failed");
            printf("%g\n", val);
            break;
        }
        case CborDoubleType: {
            double val;
            err = cbor_value_get_double(it, &val);
            ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "parse double failed");
            printf("%g\n", val);
            break;
        }
        case CborInvalidType:
            ESP_RETURN_ON_FALSE(false, CborErrorUnknownType, TAG, "unknown cbor type");
        }

        err = cbor_value_advance_fixed(it);
        ESP_RETURN_ON_FALSE(err == CborNoError, err, TAG, "advance value failed");
    }
    return CborNoError;
}

void app_main(void)
{
    CborEncoder root_encoder;
    CborParser root_parser;
    CborValue it;
    uint8_t buf[100];

    // Initialize the outermost cbor encoder
    cbor_encoder_init(&root_encoder, buf, sizeof(buf), 0);

    // Create an array containing several items
    CborEncoder array_encoder;
    CborEncoder map_encoder;
    cbor_encoder_create_array(&root_encoder, &array_encoder, 5); // [
    // 1. Create a map containing several pairs
    cbor_encoder_create_map(&array_encoder, &map_encoder, 3); // {
    // chip:esp32
    cbor_encode_text_stringz(&map_encoder, "chip");
    cbor_encode_text_stringz(&map_encoder, "esp32");
    // unicore:false
    cbor_encode_text_stringz(&map_encoder, "unicore");
    cbor_encode_boolean(&map_encoder, false);
    // ip:[192,168,1,100]
    cbor_encode_text_stringz(&map_encoder, "ip");
    CborEncoder array2;
    cbor_encoder_create_array(&map_encoder, &array2, 4); // [
    // Encode several numbers
    cbor_encode_uint(&array2, 192);
    cbor_encode_uint(&array2, 168);
    cbor_encode_uint(&array2, 1);
    cbor_encode_uint(&array2, 100);
    cbor_encoder_close_container(&map_encoder, &array2);        // ]
    cbor_encoder_close_container(&array_encoder, &map_encoder); // }
    // 2. Encode float number
    cbor_encode_float(&array_encoder, 3.14);
    // 3. Encode simple value
    cbor_encode_simple_value(&array_encoder, 99);
    // 4. Encode a string
    cbor_encode_text_stringz(&array_encoder, "2019-07-10 09:00:00+0000");
    // 5. Encode a undefined value
    cbor_encode_undefined(&array_encoder);
    cbor_encoder_close_container(&root_encoder, &array_encoder); // ]

    // If error happened when encoding, then this value should be meaningless
    ESP_LOGI(TAG, "encoded buffer size %d", cbor_encoder_get_buffer_size(&root_encoder, buf));

    // Initialize the cbor parser and the value iterator
    cbor_parser_init(buf, sizeof(buf), 0, &root_parser, &it);

    ESP_LOGI(TAG, "convert CBOR to JSON");
    // Dump the values in JSON format
    cbor_value_to_json(stdout, &it, 0);
    puts("");

    ESP_LOGI(TAG, "decode CBOR manually");
    // Decode CBOR data manually
    example_dump_cbor_buffer(&it, 0);
}
