/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "png.h"

#if CONFIG_LIBPNG_PREFER_PSRAM
/*
 * Every libpng allocation funnels through png_malloc_base(), which calls the
 * malloc_fn stored in png_struct when PNG_USER_MEM_SUPPORTED is defined. That
 * covers the png_struct itself, the zlib stream state (allocated via
 * png_zalloc), the I/O buffer and the per-row buffers.
 */
static void *png_port_malloc(png_structp png_ptr, png_alloc_size_t size)
{
    (void)png_ptr;
    return heap_caps_malloc_prefer((size_t)size, 2,
                                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT,
                                   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static void png_port_free(png_structp png_ptr, png_voidp ptr)
{
    (void)png_ptr;
    heap_caps_free(ptr);
}

/*
 * png_create_read_struct()/png_create_write_struct() and the png_image API
 * call png_create_png_struct() with NULL malloc/free handlers, so injecting
 * them here covers callers that do not use the _struct_2() entry points.
 * Handlers supplied by the application are kept.
 *
 * Declared here rather than taken from pngpriv.h, which is not a public
 * header and refuses to be included outside the library.
 */
png_structp __real_png_create_png_struct(png_const_charp user_png_ver,
                                         png_voidp error_ptr,
                                         png_error_ptr error_fn,
                                         png_error_ptr warn_fn,
                                         png_voidp mem_ptr,
                                         png_malloc_ptr malloc_fn,
                                         png_free_ptr free_fn);

png_structp __wrap_png_create_png_struct(png_const_charp user_png_ver,
                                         png_voidp error_ptr,
                                         png_error_ptr error_fn,
                                         png_error_ptr warn_fn,
                                         png_voidp mem_ptr,
                                         png_malloc_ptr malloc_fn,
                                         png_free_ptr free_fn)
{
    if (!malloc_fn) {
        malloc_fn = png_port_malloc;
    }
    if (!free_fn) {
        free_fn = png_port_free;
    }
    return __real_png_create_png_struct(user_png_ver, error_ptr, error_fn, warn_fn,
                                        mem_ptr, malloc_fn, free_fn);
}
#endif /* CONFIG_LIBPNG_PREFER_PSRAM */
