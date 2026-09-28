/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 *
 * libjpeg-turbo host tests: the in-memory decode/encode pipeline
 * (jpeg_mem_src / jpeg_mem_dest). Progressive-scan decoding is covered by
 * examples/progressive_jpeg; this file stays on the simple, full-buffer path.
 *
 * The two bundled JPEGs are compiled in with EMBED_FILES: image.jpg is
 * 320x240 baseline, image32x32.jpg is 32x32 baseline.
 */

#include <setjmp.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jpeglib.h"
#include "unity.h"

extern const uint8_t image_jpg_start[] asm("_binary_image_jpg_start");
extern const uint8_t image_jpg_end[] asm("_binary_image_jpg_end");
extern const uint8_t image32x32_jpg_start[] asm("_binary_image32x32_jpg_start");
extern const uint8_t image32x32_jpg_end[] asm("_binary_image32x32_jpg_end");

/*
 * Pixel checksums of the bundled files, folded with the FNV-1a prime starting
 * from 0 over every output byte. libjpeg-turbo's C IDCT is bit-exact with
 * SIMD disabled, so these are stable across the Linux target.
 */
#define CHECKSUM_IMAGE_RGB        0x062c9a1fu
#define CHECKSUM_IMAGE_GRAY       0xf5e71731u
#define CHECKSUM_IMAGE32_RGB      0x8fa39b10u

struct jpeg_error_mgr_ext {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
    bool *error_flag;
};

static void jpeg_error_exit(j_common_ptr cinfo)
{
    struct jpeg_error_mgr_ext *err = (struct jpeg_error_mgr_ext *)cinfo->err;

    (*cinfo->err->output_message)(cinfo);
    if (err->error_flag) {
        *err->error_flag = true;
    }
    longjmp(err->setjmp_buffer, 1);
}

struct decoded_image {
    unsigned int width;
    unsigned int height;
    int components;
    size_t bytes;
    bool decode_failed;
    bool error_exit_called;
};

static uint32_t decode_image(const uint8_t *data, size_t len, J_COLOR_SPACE out_color_space,
                             struct decoded_image *info)
{
    struct jpeg_error_mgr_ext jerr;
    struct jpeg_decompress_struct cinfo;
    /* volatile: setjmp/longjmp can otherwise clobber a register copy. */
    volatile uint32_t checksum = 0;

    memset(info, 0, sizeof(*info));
    memset(&cinfo, 0, sizeof(cinfo));

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;
    jerr.error_flag = &info->error_exit_called;

    if (setjmp(jerr.setjmp_buffer)) {
        info->decode_failed = true;
        jpeg_destroy_decompress(&cinfo);
        return 0;
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (const unsigned char *)data, (unsigned long)len);
    (void)jpeg_read_header(&cinfo, TRUE);

    if (out_color_space != JCS_UNKNOWN) {
        cinfo.out_color_space = out_color_space;
    }
    jpeg_start_decompress(&cinfo);

    info->width = cinfo.output_width;
    info->height = cinfo.output_height;
    info->components = cinfo.output_components;
    info->bytes = (size_t)cinfo.output_width * cinfo.output_height * cinfo.output_components;

    const int row_stride = (int)cinfo.output_width * cinfo.output_components;
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE,
                                                   (JDIMENSION)row_stride, 1);

    while (cinfo.output_scanline < cinfo.output_height) {
        (void)jpeg_read_scanlines(&cinfo, buffer, 1);
        for (int i = 0; i < row_stride; i++) {
            checksum = (checksum ^ buffer[0][i]) * 16777619U;
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return checksum;
}

static size_t embedded_len(const uint8_t *start, const uint8_t *end)
{
    return (size_t)(end - start);
}

/* --------------------------------------------------------------------------
 * Header parsing
 * -------------------------------------------------------------------------- */

TEST_CASE("libjpeg-turbo: read header of bundled 320x240 JPEG", "[libjpeg-turbo]")
{
    struct jpeg_error_mgr_ext jerr;
    struct jpeg_decompress_struct cinfo;
    int header_status = JPEG_SUSPENDED;
    unsigned int width = 0;
    unsigned int height = 0;
    int precision = 0;
    J_COLOR_SPACE color_space = JCS_UNKNOWN;
    boolean progressive = TRUE;

    memset(&cinfo, 0, sizeof(cinfo));
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;
    jerr.error_flag = NULL;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        TEST_FAIL_MESSAGE("jpeg_read_header() reported a fatal error");
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, image_jpg_start, (unsigned long)embedded_len(image_jpg_start, image_jpg_end));
    header_status = jpeg_read_header(&cinfo, TRUE);
    width = cinfo.image_width;
    height = cinfo.image_height;
    precision = cinfo.data_precision;
    color_space = cinfo.jpeg_color_space;
    progressive = cinfo.progressive_mode;
    jpeg_destroy_decompress(&cinfo);

    TEST_ASSERT_EQUAL_INT(JPEG_HEADER_OK, header_status);
    TEST_ASSERT_EQUAL_UINT(320, width);
    TEST_ASSERT_EQUAL_UINT(240, height);
    TEST_ASSERT_EQUAL_INT(8, precision);
    TEST_ASSERT_EQUAL_INT(JCS_YCbCr, color_space);
    TEST_ASSERT_FALSE(progressive);
}

TEST_CASE("libjpeg-turbo: read header of bundled 32x32 JPEG", "[libjpeg-turbo]")
{
    struct jpeg_error_mgr_ext jerr;
    struct jpeg_decompress_struct cinfo;
    int header_status = JPEG_SUSPENDED;
    unsigned int width = 0;
    unsigned int height = 0;
    J_COLOR_SPACE color_space = JCS_UNKNOWN;

    memset(&cinfo, 0, sizeof(cinfo));
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;
    jerr.error_flag = NULL;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_decompress(&cinfo);
        TEST_FAIL_MESSAGE("jpeg_read_header() reported a fatal error");
    }

    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, image32x32_jpg_start,
                 (unsigned long)embedded_len(image32x32_jpg_start, image32x32_jpg_end));
    header_status = jpeg_read_header(&cinfo, TRUE);
    width = cinfo.image_width;
    height = cinfo.image_height;
    color_space = cinfo.jpeg_color_space;
    jpeg_destroy_decompress(&cinfo);

    TEST_ASSERT_EQUAL_INT(JPEG_HEADER_OK, header_status);
    TEST_ASSERT_EQUAL_UINT(32, width);
    TEST_ASSERT_EQUAL_UINT(32, height);
    TEST_ASSERT_EQUAL_INT(JCS_YCbCr, color_space);
}

/* --------------------------------------------------------------------------
 * Full decode of the bundled images
 * -------------------------------------------------------------------------- */

TEST_CASE("libjpeg-turbo: decode bundled 320x240 JPEG", "[libjpeg-turbo]")
{
    struct decoded_image info;
    const size_t len = embedded_len(image_jpg_start, image_jpg_end);
    const uint32_t checksum = decode_image(image_jpg_start, len, JCS_RGB, &info);

    TEST_ASSERT_FALSE(info.decode_failed);
    TEST_ASSERT_EQUAL_UINT(320, info.width);
    TEST_ASSERT_EQUAL_UINT(240, info.height);
    TEST_ASSERT_EQUAL_INT(3, info.components);
    TEST_ASSERT_EQUAL_UINT(320 * 240 * 3, info.bytes);
    TEST_ASSERT_EQUAL_HEX32(CHECKSUM_IMAGE_RGB, checksum);
}

TEST_CASE("libjpeg-turbo: decode bundled 32x32 JPEG", "[libjpeg-turbo]")
{
    struct decoded_image info;
    const size_t len = embedded_len(image32x32_jpg_start, image32x32_jpg_end);
    const uint32_t checksum = decode_image(image32x32_jpg_start, len, JCS_UNKNOWN, &info);

    TEST_ASSERT_FALSE(info.decode_failed);
    TEST_ASSERT_EQUAL_UINT(32, info.width);
    TEST_ASSERT_EQUAL_UINT(32, info.height);
    TEST_ASSERT_EQUAL_INT(3, info.components);
    TEST_ASSERT_EQUAL_HEX32(CHECKSUM_IMAGE32_RGB, checksum);
}

TEST_CASE("libjpeg-turbo: decode bundled 320x240 JPEG as grayscale", "[libjpeg-turbo]")
{
    struct decoded_image info;
    const size_t len = embedded_len(image_jpg_start, image_jpg_end);
    const uint32_t checksum = decode_image(image_jpg_start, len, JCS_GRAYSCALE, &info);

    TEST_ASSERT_FALSE(info.decode_failed);
    TEST_ASSERT_EQUAL_UINT(320, info.width);
    TEST_ASSERT_EQUAL_UINT(240, info.height);
    TEST_ASSERT_EQUAL_INT(1, info.components);
    TEST_ASSERT_EQUAL_UINT(320 * 240, info.bytes);
    TEST_ASSERT_EQUAL_HEX32(CHECKSUM_IMAGE_GRAY, checksum);
}

/* --------------------------------------------------------------------------
 * Encode then decode
 * -------------------------------------------------------------------------- */

TEST_CASE("libjpeg-turbo: RGB encode/decode roundtrip", "[libjpeg-turbo]")
{
    enum { WIDTH = 16, HEIGHT = 16 };
    uint8_t rgb[WIDTH * HEIGHT * 3];
    unsigned char *jpeg_buf = NULL;
    unsigned long jpeg_len = 0;
    struct jpeg_error_mgr_ext jerr;
    struct jpeg_compress_struct cinfo;
    struct decoded_image info;

    /* Two solid 8x8 MCU tiles: red and blue. */
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            const int i = (y * WIDTH + x) * 3;
            const bool left = (x < 8);

            rgb[i + 0] = left ? 255 : 0;
            rgb[i + 1] = 0;
            rgb[i + 2] = left ? 0 : 255;
        }
    }

    memset(&cinfo, 0, sizeof(cinfo));
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;
    jerr.error_flag = NULL;

    if (setjmp(jerr.setjmp_buffer)) {
        jpeg_destroy_compress(&cinfo);
        free(jpeg_buf);
        TEST_FAIL_MESSAGE("jpeg compress reported a fatal error");
    }

    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, &jpeg_buf, &jpeg_len);
    cinfo.image_width = WIDTH;
    cinfo.image_height = HEIGHT;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 100, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row = rgb + cinfo.next_scanline * WIDTH * 3;

        (void)jpeg_write_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    TEST_ASSERT_NOT_NULL(jpeg_buf);
    TEST_ASSERT_GREATER_THAN(0, jpeg_len);

    const uint32_t checksum = decode_image(jpeg_buf, (size_t)jpeg_len, JCS_RGB, &info);

    free(jpeg_buf);

    TEST_ASSERT_FALSE(info.decode_failed);
    TEST_ASSERT_EQUAL_UINT(WIDTH, info.width);
    TEST_ASSERT_EQUAL_UINT(HEIGHT, info.height);
    TEST_ASSERT_EQUAL_INT(3, info.components);
    TEST_ASSERT_NOT_EQUAL(0, checksum);
}

/* --------------------------------------------------------------------------
 * Error handling: malformed input must come back through error_exit()
 * -------------------------------------------------------------------------- */

TEST_CASE("libjpeg-turbo: truncated JPEG reports an error", "[libjpeg-turbo]")
{
    struct decoded_image info;
    const size_t len = 16;

    TEST_ASSERT_GREATER_THAN(0, embedded_len(image_jpg_start, image_jpg_end));

    (void)decode_image(image_jpg_start, len, JCS_RGB, &info);

    TEST_ASSERT_TRUE(info.decode_failed);
    TEST_ASSERT_TRUE(info.error_exit_called);
}

TEST_CASE("libjpeg-turbo: empty input reports an error", "[libjpeg-turbo]")
{
    struct decoded_image info;
    static const uint8_t empty[1] = { 0 };

    /* jpeg_mem_src() treats a zero-length buffer as a fatal error. */
    (void)decode_image(empty, 0, JCS_RGB, &info);

    TEST_ASSERT_TRUE(info.decode_failed);
    TEST_ASSERT_TRUE(info.error_exit_called);
}

TEST_CASE("libjpeg-turbo: garbage input reports an error", "[libjpeg-turbo]")
{
    static const uint8_t garbage[64] = { 0x5a };
    struct decoded_image info;

    (void)decode_image(garbage, sizeof(garbage), JCS_RGB, &info);

    TEST_ASSERT_TRUE(info.decode_failed);
    TEST_ASSERT_TRUE(info.error_exit_called);
}
