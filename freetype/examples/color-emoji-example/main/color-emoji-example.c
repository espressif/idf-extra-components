/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <inttypes.h>
#include <stdlib.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_littlefs.h"
#include "ft2build.h"
#include FT_FREETYPE_H

#ifndef FT_CONFIG_OPTION_USE_PNG
#error "This example requires FreeType to be built with libpng support"
#endif

static const char *TAG = "color-emoji";
static const FT_ULong EMOJI_CODEPOINT = 0x1F642; // 🙂

static FT_Library s_library;
static FT_Face s_face;

static void check_ft_error(FT_Error error, const char *operation)
{
    if (error != 0) {
        ESP_LOGE(TAG, "%s failed: %d", operation, error);
        abort();
    }
}

static uint32_t fnv1a_bitmap(const FT_Bitmap *bitmap)
{
    uint32_t hash = 2166136261U;
    int pitch = bitmap->pitch < 0 ? -bitmap->pitch : bitmap->pitch;

    for (unsigned int row = 0; row < bitmap->rows; row++) {
        const uint8_t *data = bitmap->buffer + row * pitch;
        for (int column = 0; column < pitch; column++) {
            hash ^= data[column];
            hash *= 16777619U;
        }
    }

    return hash;
}

void app_main(void)
{
    esp_vfs_littlefs_conf_t filesystem_conf = {
        .base_path = "/fonts",
        .partition_label = "fonts",
        .format_if_mount_failed = true,
    };
    ESP_ERROR_CHECK(esp_vfs_littlefs_register(&filesystem_conf));

    check_ft_error(FT_Init_FreeType(&s_library), "Initializing FreeType");
    check_ft_error(FT_New_Face(s_library,
                               "/fonts/NotoColorEmoji-smile.ttf",
                               0,
                               &s_face),
                   "Opening color emoji font");
    if (s_face->num_fixed_sizes == 0) {
        ESP_LOGE(TAG, "Color emoji font has no bitmap strikes");
        abort();
    }

    const FT_Bitmap_Size *strike = &s_face->available_sizes[0];
    check_ft_error(FT_Select_Size(s_face, 0), "Selecting color emoji bitmap strike");
    ESP_LOGI(TAG, "Selected %dx%d color emoji bitmap strike", strike->width, strike->height);

    FT_Error error = FT_Load_Char(s_face, EMOJI_CODEPOINT, FT_LOAD_COLOR);
    check_ft_error(error, "Loading U+1F642");

    FT_GlyphSlot glyph = s_face->glyph;
    if (glyph->format != FT_GLYPH_FORMAT_BITMAP ||
            glyph->bitmap.pixel_mode != FT_PIXEL_MODE_BGRA) {
        ESP_LOGE(TAG, "Expected a decoded BGRA bitmap, got format 0x%" PRIx32
                 " and pixel mode %u",
                 (uint32_t)glyph->format, glyph->bitmap.pixel_mode);
        abort();
    }

    ESP_LOGI(TAG, "Loaded U+%04" PRIX32 " as %ux%u BGRA bitmap",
             (uint32_t)EMOJI_CODEPOINT, glyph->bitmap.width, glyph->bitmap.rows);
    ESP_LOGI(TAG, "FreeType decoded embedded PNG data using libpng; bitmap FNV-1a: %08" PRIx32,
             fnv1a_bitmap(&glyph->bitmap));

    FT_Done_Face(s_face);
    FT_Done_FreeType(s_library);
}
