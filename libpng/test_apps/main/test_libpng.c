/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "png.h"

extern const uint8_t in_png_start[] asm("_binary_in_png_start");
extern const uint8_t in_png_end[]   asm("_binary_in_png_end");

extern const uint8_t out_pgm_start[] asm("_binary_out_pgm_start");
extern const uint8_t out_pgm_end[]   asm("_binary_out_pgm_end");

TEST_CASE("load a png image", "[libpng]")
{
    png_image image;
    memset(&image, 0, (sizeof image));
    image.version = PNG_IMAGE_VERSION;

    const uint8_t *buf = &in_png_start[0];
    const size_t buf_len = in_png_end - in_png_start;

    const size_t expected_width = 522;
    const size_t expected_height = 52;

    TEST_ASSERT(png_image_begin_read_from_memory(&image, buf, buf_len));

    image.format = PNG_FORMAT_GRAY;
    int stride = PNG_IMAGE_ROW_STRIDE(image);
    int buf_size = PNG_IMAGE_SIZE(image);

    TEST_ASSERT_EQUAL(expected_width, image.width);
    TEST_ASSERT_EQUAL(expected_height, image.height);

    png_bytep buffer = malloc(buf_size);
    TEST_ASSERT_NOT_NULL(buffer);

    TEST_ASSERT(png_image_finish_read(&image, NULL, buffer, stride, NULL));

    FILE *expected = fmemopen((void *)out_pgm_start, out_pgm_end - out_pgm_start, "r");
    TEST_ASSERT_NOT_NULL(expected);
    // skip the header
    fscanf(expected, "P5\n%*d %*d\n%*d\n");
    // check the binary data
    for (int i = 0; i < buf_size; i++) {
        uint8_t expected_byte;
        TEST_ASSERT_EQUAL(1, fread(&expected_byte, 1, 1, expected));
        TEST_ASSERT_EQUAL(expected_byte, buffer[i]);
    }
    fclose(expected);

    free(buffer);
}