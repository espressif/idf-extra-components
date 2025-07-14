/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <string.h>
#include "decode_image.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"

#include "esp_cpu.h"

#include "jpeglib.h"
#include "jerror.h"

/* Reference the binary-included jpeg file */
extern const uint8_t image_jpg_start[] asm("_binary_image_jpg_start");
extern const uint8_t image_jpg_end[] asm("_binary_image_jpg_end");
extern const uint8_t image32x32_jpg_start[] asm("_binary_image32x32_jpg_start");
extern const uint8_t image32x32_jpg_end[] asm("_binary_image32x32_jpg_end");
/* Define the height and width of the jpeg file. Make sure this matches the actual jpeg
  dimensions. */

const char *TAG = "ImageDec";

struct my_error_mgr {
    struct jpeg_error_mgr pub;    /* "public" fields */

    void *setjmp_buffer;        /* for return to caller */
};

typedef struct my_error_mgr *my_error_ptr;


void my_error_exit(j_common_ptr cinfo)
{
    printf("Error - my_error_exit()! \n");
}

/* Decode the embedded image into pixel lines that can be used with the rest of the logic.
*/
esp_err_t decode_image(uint16_t **pixels)
{
    *pixels = NULL;
    esp_err_t ret = ESP_OK;

    struct my_error_mgr jerr;
    JSAMPARRAY buffer = NULL;     /* Output row buffer */

    struct jpeg_decompress_struct jpeg_info;
    struct jpeg_decompress_struct *cinfo = &jpeg_info;

    cinfo->err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;

    struct jpeg_decompress_struct jpeg_info_print;
    struct jpeg_decompress_struct *cinfo_print = &jpeg_info_print;

    cinfo_print->err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = my_error_exit;


    jpeg_create_decompress(cinfo);

    jpeg_mem_src(cinfo, image_jpg_start, (image_jpg_end - image_jpg_start));

    (void)jpeg_read_header(cinfo, TRUE);
    /* We can ignore the return value from jpeg_read_header since
     *   (a) suspension is not possible with the stdio data source, and
     *   (b) we passed TRUE to reject a tables-only JPEG file as an error.
     * See libjpeg.txt for more info.
     */

    /* emit header for raw PPM format */
    printf("P6\n%u %u\n%d\n", cinfo->image_width, cinfo->image_height,
           cinfo->data_precision == 12 ? MAXJ12SAMPLE : MAXJSAMPLE);

    printf("jpeg_start_decompress\n");
    unsigned int start_b = esp_cpu_get_cycle_count();
    jpeg_start_decompress(cinfo);

    int row_stride = cinfo->output_width * cinfo->output_components;
    buffer = (*cinfo->mem->alloc_sarray)
             ((j_common_ptr)cinfo, JPOOL_IMAGE, row_stride, 1);

    while (cinfo->output_scanline < cinfo->output_height) {
        /* jpeg_read_scanlines expects an array of pointers to scanlines.
         * Here the array is only one element long, but you could ask for
         * more than one scanline at a time if that's more convenient.
         */
        (void)jpeg_read_scanlines(cinfo, buffer, 1);
    }
    unsigned int end_b = esp_cpu_get_cycle_count();
    printf("jpeg_finish_decompress, time = %i\n", ((unsigned int)(end_b - start_b) / 100000) * 100000);
    jpeg_finish_decompress(cinfo);

    printf("jpeg_destroy_decompress\n");
    jpeg_destroy_decompress(cinfo);

    jpeg_create_decompress(cinfo_print);

    jpeg_mem_src(cinfo_print, image32x32_jpg_start, (image32x32_jpg_end - image32x32_jpg_start));
    jpeg_read_header(cinfo_print, TRUE);
    printf("P6\n%u %u\n%d\n", cinfo_print->image_width, cinfo_print->image_height,
           cinfo_print->data_precision == 12 ? MAXJ12SAMPLE : MAXJSAMPLE);

    jpeg_start_decompress(cinfo_print);

    row_stride = cinfo_print->output_width * cinfo_print->output_components;
    buffer = (*cinfo_print->mem->alloc_sarray)
             ((j_common_ptr)cinfo_print, JPOOL_IMAGE, row_stride, 1);

    printf("Decoded image 32x32:\n");
    while (cinfo_print->output_scanline < cinfo_print->output_height) {
        (void)jpeg_read_scanlines(cinfo_print, buffer, 1);
        for (int ix = 0; ix < cinfo_print->image_width; ix++) {
            int val = 0;
            for (int i = 0; i < cinfo_print->output_components; i++) {
                val += buffer[0][ix * cinfo_print->output_components + i];
            }
            val = val / cinfo->output_components;
            if (val > 127) {
                putchar('#');
            } else if (val > 64) {
                putchar('+');
            } else if (val > 32) {
                putchar('.');
            } else {
                putchar(' ');
            }
        }
        putchar('\n');
    }
    jpeg_finish_decompress(cinfo_print);
    jpeg_destroy_decompress(cinfo_print);
    printf("done\n");

    return ret;
}
