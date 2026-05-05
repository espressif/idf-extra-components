/*
 * SPDX-FileCopyrightText: 2022 mikkeldamsgaard project
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * SPDX-FileContributor: 2015-2026 Espressif Systems (Shanghai) CO LTD
 */

#include "nand_oob_xfer.h"

#include <assert.h>
#include <string.h>

#include "nand_oob_layout_default.h"

static bool region_matches_class(const spi_nand_oob_region_desc_t *r, spi_nand_oob_class_t cls)
{
    if (!r->programmable) {
        return false;
    }
    if (cls == SPI_NAND_OOB_CLASS_FREE_ECC) {
        return r->ecc_protected;
    }
    return !r->ecc_protected;
}

esp_err_t nand_oob_xfer_ctx_init(spi_nand_oob_xfer_ctx_t *ctx,
                                 const spi_nand_oob_layout_t *layout,
                                 const void *chip_ctx,
                                 spi_nand_oob_class_t cls,
                                 uint8_t *oob_raw,
                                 uint16_t oob_size)
{
    if (ctx == NULL || layout == NULL || layout->ops == NULL || layout->ops->free_region == NULL || oob_raw == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->layout = layout;
    ctx->cls = cls;
    ctx->oob_raw = oob_raw;
    ctx->oob_size = oob_size;

    for (int section = 0;; section++) {
        spi_nand_oob_region_desc_t desc;
        esp_err_t err = layout->ops->free_region(chip_ctx, section, &desc);
        if (err == ESP_ERR_NOT_FOUND) {
            break;
        }
        if (err != ESP_OK) {
            memset(ctx, 0, sizeof(*ctx));
            return err;
        }
        if (!region_matches_class(&desc, cls)) {
            continue;
        }
        if (ctx->reg_count >= SPI_NAND_OOB_MAX_REGIONS) {
            memset(ctx, 0, sizeof(*ctx));
            return ESP_ERR_NO_MEM;
        }
        if ((uint32_t)desc.offset + (uint32_t)desc.length > (uint32_t)oob_size) {
            memset(ctx, 0, sizeof(*ctx));
            return ESP_ERR_INVALID_SIZE;
        }
        ctx->regs[ctx->reg_count] = desc;
        ctx->total_logical_len += desc.length;
        ctx->reg_count++;
    }

#ifndef NDEBUG
    if (layout == nand_oob_layout_get_default() && cls == SPI_NAND_OOB_CLASS_FREE_ECC) {
        assert(ctx->total_logical_len == 2);
        assert(ctx->reg_count == 1);
        assert(ctx->regs[0].offset == 2 && ctx->regs[0].length == 2);
    }
#endif

    return ESP_OK;
}

esp_err_t nand_oob_gather(const spi_nand_oob_xfer_ctx_t *ctx,
                          size_t logical_off,
                          void *dst,
                          size_t len)
{
    if (ctx == NULL || dst == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ESP_OK;
    }
    if (logical_off > ctx->total_logical_len || len > ctx->total_logical_len - logical_off) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *dst_u8 = (uint8_t *)dst;
    const uint8_t *oob = ctx->oob_raw;
    size_t logical_base = 0;

    for (unsigned i = 0; i < ctx->reg_count; i++) {
        const spi_nand_oob_region_desc_t *r = &ctx->regs[i];
        size_t next_base = logical_base + r->length;
        if (logical_off + len <= logical_base) {
            break;
        }
        if (logical_off >= next_base) {
            logical_base = next_base;
            continue;
        }
        size_t seg_start = logical_off > logical_base ? logical_off : logical_base;
        size_t seg_end = logical_off + len < next_base ? logical_off + len : next_base;
        if (seg_start < seg_end) {
            size_t local = seg_start - logical_base;
            size_t run = seg_end - seg_start;
            size_t dst_off = seg_start - logical_off;
            memcpy(dst_u8 + dst_off, oob + r->offset + local, run);
        }
        logical_base = next_base;
    }
    return ESP_OK;
}

esp_err_t nand_oob_scatter(spi_nand_oob_xfer_ctx_t *ctx,
                           size_t logical_off,
                           const void *src,
                           size_t len)
{
    if (ctx == NULL || src == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ESP_OK;
    }
    if (logical_off > ctx->total_logical_len || len > ctx->total_logical_len - logical_off) {
        return ESP_ERR_INVALID_SIZE;
    }

    const uint8_t *src_u8 = (const uint8_t *)src;
    uint8_t *oob = ctx->oob_raw;
    size_t logical_base = 0;

    for (unsigned i = 0; i < ctx->reg_count; i++) {
        const spi_nand_oob_region_desc_t *r = &ctx->regs[i];
        size_t next_base = logical_base + r->length;
        if (logical_off + len <= logical_base) {
            break;
        }
        if (logical_off >= next_base) {
            logical_base = next_base;
            continue;
        }
        size_t seg_start = logical_off > logical_base ? logical_off : logical_base;
        size_t seg_end = logical_off + len < next_base ? logical_off + len : next_base;
        if (seg_start < seg_end) {
            size_t local = seg_start - logical_base;
            size_t run = seg_end - seg_start;
            size_t src_off = seg_start - logical_off;
            memcpy(oob + r->offset + local, src_u8 + src_off, run);
        }
        logical_base = next_base;
    }
    return ESP_OK;
}
