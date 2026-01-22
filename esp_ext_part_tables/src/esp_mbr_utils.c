/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdint.h>
#include <string.h>
#include "esp_log.h"
#include "esp_ext_part_tables.h"
#include "esp_mbr_utils.h"

static const char *TAG = "esp_mbr_utils";

void esp_mbr_chs_arr_val_set(uint8_t chs[3], uint32_t val)
{
    chs[0] = val & 0xFF;
    chs[1] = (val >> 8) & 0xFF;
    chs[2] = (val >> 16) & 0xFF;
}

uint32_t esp_mbr_chs_arr_val_get(const uint8_t chs[3])
{
    return chs[0] | (chs[1] << 8) | (chs[2] << 16);
}

void esp_mbr_lba_to_chs_arr(uint8_t chs[3], uint32_t lba)
{
    uint16_t cylinder;
    uint8_t head;
    uint8_t sector;

    uint32_t sectors_per_cylinder = MBR_CHS_HEADS * MBR_CHS_SECTORS_PER_TRACK;
    uint32_t temp;

    cylinder = lba / sectors_per_cylinder;
    temp = lba % sectors_per_cylinder;
    head = temp / MBR_CHS_SECTORS_PER_TRACK;
    sector = (temp % MBR_CHS_SECTORS_PER_TRACK) + 1;

    // Clamp to BIOS CHS limits
    if (cylinder > MBR_CHS_MAX_CYLINDER) {
        cylinder = MBR_CHS_MAX_CYLINDER;
    }
    if (head > MBR_CHS_MAX_HEAD) {
        head = MBR_CHS_MAX_HEAD;
    }
    if (sector > MBR_CHS_MAX_SECTOR) {
        sector = MBR_CHS_MAX_SECTOR;
    }

    uint8_t chs_bytes[3];
    chs_bytes[0] = head & 0xFF;
    chs_bytes[1] = ((cylinder >> 2) & 0xC0) | (sector & 0x3F); // high 2 bits of cylinder + 6-bit sector
    chs_bytes[2] = cylinder & 0xFF;

    if (chs != NULL) {
        memcpy(chs, chs_bytes, 3);
    }
}

uint32_t esp_mbr_lba_align(uint32_t lba, esp_ext_part_sector_size_t sector_size, esp_ext_part_align_t alignment)
{
    if (sector_size == 0 || alignment == 0) {
        return lba; // No alignment
    }
    uint32_t alignment_sectors = alignment / sector_size;
    return (lba + alignment_sectors - 1) & ~(alignment_sectors - 1);
}

static bool default_known_supported_partition_types(uint8_t type, esp_ext_part_type_known_t *out_type_parsed)
{
    bool supported = true;
    esp_ext_part_type_known_t parsed_type = ESP_EXT_PART_TYPE_NONE;
    switch (type) {
    // Supported types:
    case 0x01: // FAT12
        parsed_type = ESP_EXT_PART_TYPE_FAT12;
        break;
    case 0x04: __attribute__((fallthrough));
    case 0x06: __attribute__((fallthrough));
    case 0x0E: // FAT16B with LBA addressing (also uses 0x04 and 0x06 but on modern system shouldn't matter)
        parsed_type = ESP_EXT_PART_TYPE_FAT16;
        break;
    case 0x0B: __attribute__((fallthrough));
    case 0x0C: // FAT32 with LBA addressing (0x0B is for CHS addressing)
        parsed_type = ESP_EXT_PART_TYPE_FAT32;
        break;
    case 0xC3: // Possibly LittleFS (MBR CHS field => LittleFS block size hack)
        parsed_type = ESP_EXT_PART_TYPE_LITTLEFS;
        break;

    // Unsupported types:
    case 0x07: // exFAT or NTFS
        parsed_type = ESP_EXT_PART_TYPE_EXFAT_OR_NTFS;
        supported = false; // Not supported
        break;
    case 0x83: // Linux partition (any type)
        parsed_type = ESP_EXT_PART_TYPE_LINUX_ANY;
        supported = false; // Not supported
        break;
    case 0xEE: // GPT protective MBR
        parsed_type = ESP_EXT_PART_TYPE_GPT_PROTECTIVE_MBR;
        supported = false; // Not supported
        break;
    case 0x05: __attribute__((fallthrough)); // Extended partition with CHS addressing
    case 0x0F: __attribute__((fallthrough)); // Extended partition with LBA addressing
    default:
        supported = false;
        break;
    }

    if (out_type_parsed != NULL) {
        *out_type_parsed = parsed_type;
    }
    if (supported == false) {
        ESP_LOGD(TAG, "Unknown or unsupported partition type: 0x%02X", type);
    }
    return supported;
}

bool esp_mbr_parse_default_supported_partition_types(uint8_t type, uint8_t *out_type_parsed)
{
    esp_ext_part_type_known_t parsed_type = ESP_EXT_PART_TYPE_NONE;
    bool is_supported = default_known_supported_partition_types(type, &parsed_type);

    if (out_type_parsed != NULL) {
        *out_type_parsed = (uint8_t) parsed_type;
    }
    return is_supported;
}

uint8_t esp_mbr_generate_default_supported_partition_types(uint8_t type)
{
    switch ((esp_ext_part_type_known_t) type) {
    case ESP_EXT_PART_TYPE_FAT12:
        return 0x01; // FAT12
    case ESP_EXT_PART_TYPE_FAT16:
        return 0x0E; // FAT16B with LBA addressing
    case ESP_EXT_PART_TYPE_FAT32:
        return 0x0C; // FAT32 with LBA addressing
    /*
    LittleFS is not a standard MBR partition type, but we can use a custom type `0xC3`, which is not usually used nowadays.
    This allows us to identify LittleFS partitions in the MBR.

    Explanation why `0xC3` was chosen:
    0xC    3
      1100 0011
      ↑↑ ↑ ↑↑↑↑
      └│─│─┴┴┴┴── 0x83 => a modern filesystem (e.g. Linux)
       └─│─────── 0x40 => CHS used as LittleFS block size
         └─────── 0x10 => a hidden filesystem
    */
    case ESP_EXT_PART_TYPE_LITTLEFS:
        return 0xC3; // Possibly LittleFS (MBR CHS field => LittleFS block size hack)
    case ESP_EXT_PART_TYPE_EXFAT_OR_NTFS: // Not supported, but we can return a type for it
        return 0x07; // exFAT or NTFS
    case ESP_EXT_PART_TYPE_LINUX_ANY: // Not supported, but we can return a type for it
        return 0x83; // Linux partition (any type)
    case ESP_EXT_PART_TYPE_GPT_PROTECTIVE_MBR: // Not supported, but we can return a type for it
        return 0xEE; // GPT protective MBR
    default:
        return 0x00; // Unknown type
    }
}
