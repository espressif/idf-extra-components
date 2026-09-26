/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stddef.h>

#include "esp_heap_caps.h"

/*
 * libjpeg-turbo's memory manager (jmemmgr.c) gets all of its memory through
 * jpeg_get_small() and jpeg_get_large() in jmemnobs.c, which call malloc().
 * The component links with --wrap for both symbols, so these replacements
 * serve every request from jmemmgr.c. The matching jpeg_free_small() and
 * jpeg_free_large() call free(), which releases heap_caps memory as well.
 *
 * Both return NULL on failure, as jmemsys.h requires.
 */

/* jpeglib.h is only installed by the sub-build, so declare j_common_ptr's struct. */
struct jpeg_common_struct;

static void *jpeg_caps_malloc(size_t size)
{
    return heap_caps_malloc_prefer(size, 2,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void *__wrap_jpeg_get_small(struct jpeg_common_struct *cinfo, size_t sizeofobject)
{
    (void)cinfo;
    return jpeg_caps_malloc(sizeofobject);
}

void *__wrap_jpeg_get_large(struct jpeg_common_struct *cinfo, size_t sizeofobject)
{
    (void)cinfo;
    return jpeg_caps_malloc(sizeofobject);
}
