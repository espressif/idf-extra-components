/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <inttypes.h>
#include <string.h>

#include "esp_log.h"
#include "esp_nand_ubi.h"
#include "esp_nand_blockdev.h"

#include "nand_ubi_priv.h"
#include "nand_ubi_media.h"
#include "nand_ubi_io.h"
#include "nand_ubi_eba.h"

static const char *TAG = "nand_ubi";

static bool page_is_blank(const uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        if (buf[i] != 0xFFu) {
            return false;
        }
    }
    return true;
}

/* Verifies a copy_flag PEB's LEB data against vid_hdr->data_crc. Reads in page_size
 * multiples (nand_bdl->ops->read() rejects a partial-page tail on a multi-page span)
 * and hashes only the first data_size bytes, matching how data_crc was computed.
 * peb_size/page_size come from nand_bdl->geometry rather than as extra parameters:
 * attach() hasn't finished populating dev's copies of them at the point this is
 * called, and re-deriving avoids stacking more same-typed uint32_t parameters. */
static bool verify_copy_data(esp_blockdev_handle_t nand_bdl, uint32_t pnum,
                              uint32_t data_offset, const nand_ubi_vid_hdr_t *vid_hdr)
{
    uint32_t peb_size = (uint32_t)nand_bdl->geometry.erase_size;
    uint32_t page_size = (uint32_t)nand_bdl->geometry.read_size;
    uint32_t data_size = nand_ubi_be32(vid_hdr->data_size);
    uint32_t data_crc = nand_ubi_be32(vid_hdr->data_crc);
    uint32_t leb_size = peb_size - data_offset;

    if (data_size == 0 || data_size > leb_size) {
        ESP_LOGW(TAG, "pnum=%" PRIu32 ": copy_flag data_size %" PRIu32 " out of range", pnum, data_size);
        return false;
    }

    uint32_t read_len = ((data_size + page_size - 1) / page_size) * page_size;
    if (read_len > leb_size) {
        read_len = leb_size;
    }

    uint8_t *buf = ubi_alloc(read_len);
    if (!buf) {
        ESP_LOGE(TAG, "pnum=%" PRIu32 ": no memory (%" PRIu32 " B) to verify copy_flag data", pnum, read_len);
        return false;
    }

    esp_err_t ret = nand_bdl->ops->read(nand_bdl, buf, read_len,
                                         (uint64_t)pnum * peb_size + data_offset, read_len);
    bool ok = false;
    if (ret == ESP_OK) {
        ok = (nand_ubi_crc32(buf, data_size) == data_crc);
    } else {
        ESP_LOGE(TAG, "pnum=%" PRIu32 ": LEB data read failed for copy_flag verification: 0x%x", pnum, ret);
    }
    free(buf);
    return ok;
}

esp_err_t nand_ubi_attach(esp_blockdev_handle_t nand_bdl,
                           const nand_ubi_config_t *config,
                           nand_ubi_device_t **out_ubi_dev)
{
    if (nand_bdl == NULL || out_ubi_dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *out_ubi_dev = NULL;
    (void)config; /* reserved_pebs/read_only are not consumed by attach(); see Task 5/6 */

    uint32_t page_size = (uint32_t)nand_bdl->geometry.read_size;
    uint32_t peb_size = (uint32_t)nand_bdl->geometry.erase_size;
    if (page_size == 0 || peb_size == 0 || peb_size % page_size != 0) {
        ESP_LOGE(TAG, "invalid geometry: read_size=%" PRIu32 " erase_size=%" PRIu32, page_size, peb_size);
        return ESP_ERR_INVALID_ARG;
    }
    if (nand_bdl->geometry.disk_size % peb_size != 0) {
        ESP_LOGW(TAG, "disk_size 0x%016" PRIx64 " is not a multiple of erase_size %" PRIu32,
                 nand_bdl->geometry.disk_size, peb_size);
    }
    uint32_t peb_count = (uint32_t)(nand_bdl->geometry.disk_size / peb_size);
    if (peb_count == 0) {
        ESP_LOGE(TAG, "disk_size 0x%016" PRIx64 " yields zero PEBs", nand_bdl->geometry.disk_size);
        return ESP_ERR_INVALID_ARG;
    }

    nand_ubi_device_t *dev = calloc(1, sizeof(*dev));
    if (!dev) {
        return ESP_ERR_NO_MEM;
    }

    /* leb_count is unknown until the scan below finds max_lnum, so eba[] is
     * over-allocated to peb_count (leb_count <= peb_count always). */
    esp_err_t ret = nand_ubi_eba_alloc(peb_count, peb_count, &dev->eba);
    if (ret != ESP_OK) {
        free(dev);
        return ret;
    }

    uint64_t *sqnum_seen = ubi_alloc((size_t)peb_count * sizeof(uint64_t));
    uint8_t *page_buf = ubi_alloc(page_size);
    if (!sqnum_seen || !page_buf) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }
    memset(sqnum_seen, 0, (size_t)peb_count * sizeof(uint64_t));

    uint32_t image_seq = 0;
    bool have_image_seq = false;
    int32_t max_lnum = -1;

    for (uint32_t pnum = 0; pnum < peb_count; pnum++) {
        esp_blockdev_cmd_arg_status_t bad_arg = { .num = pnum, .status = false };
        ret = nand_bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_IS_BAD_BLOCK, &bad_arg);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "IS_BAD_BLOCK ioctl failed for pnum=%" PRIu32 ": 0x%x", pnum, ret);
            goto fail;
        }
        if (bad_arg.status) {
            nand_ubi_eba_peb_set_bad(&dev->eba, pnum);
            continue;
        }

        ret = nand_bdl->ops->read(nand_bdl, page_buf, page_size, (uint64_t)pnum * peb_size, page_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "EC header read failed for pnum=%" PRIu32 ": 0x%x", pnum, ret);
            goto fail;
        }
        if (page_is_blank(page_buf, page_size)) {
            continue; /* peb_state defaults to FREE */
        }

        const nand_ubi_ec_hdr_t *ec_hdr = (const nand_ubi_ec_hdr_t *)page_buf;
        if (!nand_ubi_ec_hdr_valid(ec_hdr)) {
            ESP_LOGW(TAG, "pnum=%" PRIu32 ": corrupt EC header, scheduling erase", pnum);
            nand_ubi_eba_peb_set_erase_pending(&dev->eba, pnum);
            continue;
        }

        uint32_t peb_image_seq = nand_ubi_be32(ec_hdr->image_seq);
        if (!have_image_seq) {
            image_seq = peb_image_seq;
            have_image_seq = true;
        } else if (peb_image_seq != image_seq) {
            ESP_LOGW(TAG, "pnum=%" PRIu32 ": image_seq 0x%08" PRIx32 " != 0x%08" PRIx32 ", treating as stale",
                     pnum, peb_image_seq, image_seq);
            nand_ubi_eba_peb_set_erase_pending(&dev->eba, pnum);
            continue;
        }

        uint32_t vid_hdr_offset = nand_ubi_be32(ec_hdr->vid_hdr_offset);
        uint32_t data_offset = nand_ubi_be32(ec_hdr->data_offset);
        if (vid_hdr_offset == 0 || vid_hdr_offset % page_size != 0 ||
                data_offset % page_size != 0 || data_offset < vid_hdr_offset + page_size ||
                data_offset >= peb_size) {
            ESP_LOGW(TAG, "pnum=%" PRIu32 ": bad header offsets vid=%" PRIu32 " data=%" PRIu32
                     ", scheduling erase", pnum, vid_hdr_offset, data_offset);
            nand_ubi_eba_peb_set_erase_pending(&dev->eba, pnum);
            continue;
        }
        if (dev->vid_hdr_offset == 0) {
            /* First valid EC header seen defines this attach's header layout. */
            dev->vid_hdr_offset = vid_hdr_offset;
            dev->data_offset = data_offset;
        }

        ret = nand_bdl->ops->read(nand_bdl, page_buf, page_size,
                                   (uint64_t)pnum * peb_size + vid_hdr_offset, page_size);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "VID header read failed for pnum=%" PRIu32 ": 0x%x", pnum, ret);
            goto fail;
        }
        if (page_is_blank(page_buf, page_size)) {
            continue; /* EC header present but no VID header yet -> still FREE */
        }

        const nand_ubi_vid_hdr_t *vid_hdr = (const nand_ubi_vid_hdr_t *)page_buf;
        if (!nand_ubi_vid_hdr_valid(vid_hdr)) {
            ESP_LOGW(TAG, "pnum=%" PRIu32 ": corrupt VID header, scheduling erase", pnum);
            nand_ubi_eba_peb_set_erase_pending(&dev->eba, pnum);
            continue;
        }

        uint32_t lnum = nand_ubi_be32(vid_hdr->lnum);
        uint64_t sqnum = nand_ubi_be64(vid_hdr->sqnum);
        if (lnum >= peb_count) {
            ESP_LOGW(TAG, "pnum=%" PRIu32 ": lnum %" PRIu32 " out of range, scheduling erase", pnum, lnum);
            nand_ubi_eba_peb_set_erase_pending(&dev->eba, pnum);
            continue;
        }

        /* This lnum exists in the volume's logical address space regardless of which
         * physical replica ultimately wins below, so leb_count/global_sqnum track it
         * unconditionally rather than only on the accepted branch. */
        if ((int32_t)lnum > max_lnum) {
            max_lnum = (int32_t)lnum;
        }
        if (sqnum > dev->global_sqnum) {
            dev->global_sqnum = sqnum;
        }

        int32_t existing_pnum = nand_ubi_eba_get_pnum(&dev->eba, lnum);
        if (existing_pnum == UBI_LEB_UNMAPPED) {
            if (vid_hdr->copy_flag && !verify_copy_data(nand_bdl, pnum, dev->data_offset, vid_hdr)) {
                ESP_LOGW(TAG, "pnum=%" PRIu32 " lnum=%" PRIu32 ": copy_flag data_crc mismatch, scheduling erase",
                         pnum, lnum);
                nand_ubi_eba_peb_set_erase_pending(&dev->eba, pnum);
                continue;
            }
            nand_ubi_eba_set(&dev->eba, lnum, (int32_t)pnum);
            sqnum_seen[lnum] = sqnum;
            nand_ubi_eba_peb_set_used(&dev->eba, pnum);
        } else if (sqnum > sqnum_seen[lnum]) {
            uint32_t old_pnum = (uint32_t)existing_pnum;
            if (vid_hdr->copy_flag && !verify_copy_data(nand_bdl, pnum, dev->data_offset, vid_hdr)) {
                ESP_LOGW(TAG, "pnum=%" PRIu32 " lnum=%" PRIu32 ": copy_flag data_crc mismatch, keeping pnum %" PRIu32,
                         pnum, lnum, old_pnum);
                nand_ubi_eba_peb_set_erase_pending(&dev->eba, pnum);
            } else {
                nand_ubi_eba_peb_set_erase_pending(&dev->eba, old_pnum);
                nand_ubi_eba_set(&dev->eba, lnum, (int32_t)pnum);
                sqnum_seen[lnum] = sqnum;
                nand_ubi_eba_peb_set_used(&dev->eba, pnum);
            }
        } else {
            ESP_LOGW(TAG, "pnum=%" PRIu32 ": stale duplicate of lnum=%" PRIu32 ", scheduling erase", pnum, lnum);
            nand_ubi_eba_peb_set_erase_pending(&dev->eba, pnum);
        }
    }

    free(page_buf);
    free(sqnum_seen);
    page_buf = NULL;
    sqnum_seen = NULL;

    if (dev->vid_hdr_offset == 0) {
        /* Blank/unformatted chip: no valid EC header was ever found. Fall back to the
         * same layout esp_ubinize.py uses so leb_size is still well-defined. */
        dev->vid_hdr_offset = page_size;
        dev->data_offset = 2u * page_size;
    }

    dev->nand_bdl = nand_bdl;
    dev->peb_count = peb_count;
    dev->peb_size = peb_size;
    dev->page_size = page_size;
    dev->leb_size = dev->peb_size - dev->data_offset;
    dev->image_seq = image_seq;
    dev->leb_count = (uint32_t)(max_lnum + 1);

    dev->lock = xSemaphoreCreateMutex();
    if (dev->lock == NULL) {
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }

    ESP_LOGI(TAG, "attached: peb_count=%" PRIu32 " leb_count=%" PRIu32 " peb_size=%" PRIu32
             " leb_size=%" PRIu32 " image_seq=0x%08" PRIx32,
             dev->peb_count, dev->leb_count, dev->peb_size, dev->leb_size, dev->image_seq);

    *out_ubi_dev = dev;
    return ESP_OK;

fail:
    free(page_buf);
    free(sqnum_seen);
    nand_ubi_eba_free(&dev->eba);
    free(dev);
    return ret;
}

esp_err_t nand_ubi_detach(nand_ubi_device_t *ubi_dev)
{
    if (ubi_dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    /* Task 5 adds volume-open tracking; once it exists this must reject with
     * ESP_ERR_INVALID_STATE while any volume opened from ubi_dev is still open. */

    if (ubi_dev->lock) {
        vSemaphoreDelete(ubi_dev->lock);
    }
    nand_ubi_eba_free(&ubi_dev->eba);
    free(ubi_dev);
    return ESP_OK;
}
