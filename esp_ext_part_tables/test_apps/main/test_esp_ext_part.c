/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "esp_err.h"
#include "esp_ext_part_tables.h"
#include "esp_mbr.h"

#include "unity.h"

// MBR with 2 FAT12 entries
uint8_t mbr_bin[512] = {
    [440] = 0xc4, 0x9d, 0x92, 0x4d, 0x00, 0x00, 0x00, 0x20, 0x21, 0x00,
    0x01, 0x9e, 0x2f, 0x00, 0x00, 0x08, 0x00, 0x00, 0x11, 0x1f,
    0x00, 0x00, 0x00, 0xa2, 0x23, 0x00, 0x01, 0x46, 0x05, 0x01,
    0x00, 0x28, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x55, 0xaa
};

unsigned int mbr_bin_len = 512;

void print_esp_ext_part_list_item(esp_ext_part_list_item_t* head)
{
    esp_ext_part_list_item_t* it = head;
    int i = 0;
    do {
        printf("Partition %d:\n\tLBA start sector: %" PRIu64 ", address: %" PRIu64 ",\n\tsector count: %" PRIu64 ", size: %" PRIu64 ",\n\ttype: %" PRIu32 "\n\n",
               i,
               esp_ext_part_bytes_to_sector_count(it->info.address, ESP_EXT_PART_SECTOR_SIZE_512B), it->info.address,
               esp_ext_part_bytes_to_sector_count(it->info.size, ESP_EXT_PART_SECTOR_SIZE_512B), it->info.size,
               (uint32_t) (it->info.type));
        i++;
    } while ((it = esp_ext_part_list_item_next(it)) != NULL);
}

TEST_CASE("Test mbr_bin struct", "[esp_ext_part_table]")
{
    mbr_t* mbr = (mbr_t*) mbr_bin;
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ASSERT_EQUAL(MBR_SIGNATURE, mbr->boot_signature);
    printf("MBR boot signature: 0x%" PRIX16 "\n", mbr->boot_signature);
    printf("MBR disk signature: 0x%" PRIX32 "\n", mbr->disk_signature);
}

TEST_CASE("Test esp_mbr_parse", "[esp_ext_part_table]")
{
    esp_ext_part_list_t part_list = {0};
    TEST_ESP_OK(esp_mbr_parse((void*) mbr_bin, &part_list, NULL));
    esp_ext_part_list_item_t* it = esp_ext_part_list_item_head(&part_list);
    TEST_ASSERT_NOT_NULL(it);

    print_esp_ext_part_list_item(it);
    fflush(stdout);

    do {
        TEST_ASSERT_NOT_EQUAL(0, it->info.address);
        TEST_ASSERT_NOT_EQUAL(0, it->info.size);
        TEST_ASSERT_NOT_EQUAL(0, it->info.type);
    } while ((it = esp_ext_part_list_item_next(it)) != NULL);
    esp_ext_part_list_deinit(&part_list);
    TEST_ASSERT_EQUAL(0, part_list.head.slh_first);
}

void generate_original_mbr(mbr_t* mbr)
{
    esp_mbr_generate_extra_args_t mbr_args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB
    };

    esp_ext_part_list_t part_list = {0};

    // 2 FAT12 partitions with same parameters as in the original MBR in the array
    esp_ext_part_list_item_t item1 = {
        .info = {
            // Original MBR starts at 2048, but we use 8 for testing ->
            .address = esp_ext_part_sector_count_to_bytes(8, mbr_args.sector_size), // Should be round up to 2048 sectors (aligned to 1MiB) due to defined sector size and alignment in `esp_mbr_generate_extra_args_t args` below
            .size = esp_ext_part_sector_count_to_bytes(7953, mbr_args.sector_size),
            .type = ESP_EXT_PART_TYPE_FAT12,
            .label = NULL,
        }
    };
    esp_ext_part_list_item_t item2 = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(10240, mbr_args.sector_size),
            .size = esp_ext_part_sector_count_to_bytes(10240, mbr_args.sector_size),
            .type = ESP_EXT_PART_TYPE_FAT12,
            .label = NULL,
        }
    };

    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item1));
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item2));

    // Generate the MBR
    TEST_ESP_OK(esp_mbr_generate(mbr, &part_list, &mbr_args));

    // Deinitialize the part list
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

TEST_CASE("Test esp_mbr_generate generates the (almost) same MBR as the original", "[esp_ext_part_table]")
{
    mbr_t* mbr = (mbr_t*) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    generate_original_mbr(mbr);

    esp_ext_part_list_t part_list1 = {0};
    TEST_ESP_OK(esp_mbr_parse((void*) mbr, &part_list1, NULL));
    esp_ext_part_list_item_t* it1 = esp_ext_part_list_item_head(&part_list1);
    TEST_ASSERT_NOT_NULL(it1);

    esp_ext_part_list_t part_list2 = {0};
    TEST_ESP_OK(esp_mbr_parse((void*) mbr_bin, &part_list2, NULL));
    esp_ext_part_list_item_t* it2 = esp_ext_part_list_item_head(&part_list2);
    TEST_ASSERT_NOT_NULL(it2);

    print_esp_ext_part_list_item(it1);
    print_esp_ext_part_list_item(it2);
    fflush(stdout);

    uint8_t* mbr_bin_from_part_table = (uint8_t*) mbr_bin + MBR_PARTITION_TABLE_OFFSET;
    uint8_t* mbr_from_part_table = (uint8_t*) mbr + MBR_PARTITION_TABLE_OFFSET;
    uint8_t compare_size = mbr_bin_len - MBR_PARTITION_TABLE_OFFSET;
    // Test if the generated MBR is the same as the original MBR - only from partition table part
    // Disk signature is randomly generated, so we don't compare it
    TEST_ASSERT_EQUAL_MEMORY(mbr_bin_from_part_table, mbr_from_part_table, compare_size);
    free(mbr);
    esp_ext_part_list_deinit(&part_list1);
    esp_ext_part_list_deinit(&part_list2);
}

TEST_CASE("Test esp_mbr_generate with esp_mbr_parse", "[esp_ext_part_table]")
{
    mbr_t* mbr;
    mbr = (mbr_t*) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    generate_original_mbr(mbr);

    esp_ext_part_list_t part_list = {0};
    TEST_ESP_OK(esp_mbr_parse((void*) mbr, &part_list, NULL));
    free(mbr);

    esp_ext_part_list_item_t* it;
    it = esp_ext_part_list_item_head(&part_list);
    TEST_ASSERT_NOT_NULL(it);

    // Print the partition list
    print_esp_ext_part_list_item(it);
    fflush(stdout);

    do {
        TEST_ASSERT_NOT_EQUAL(0, it->info.address);
        TEST_ASSERT_NOT_EQUAL(0, it->info.size);
        TEST_ASSERT_NOT_EQUAL(0, it->info.type);
    } while ((it = esp_ext_part_list_item_next(it)) != NULL);
    // Deinitialize the part list
    esp_ext_part_list_deinit(&part_list);
    it = NULL;
    TEST_ASSERT_EQUAL(0, part_list.head.slh_first);

    // Another MBR

    mbr = (mbr_t*) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    esp_mbr_generate_extra_args_t mbr_args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB
    };

    // 2 FAT12 partitions with same parameters as in the original MBR in the array
    esp_ext_part_list_item_t item1 = {
        .info = {
            .address = 8, // Should be round up to 2048 (aligned to 1MiB) due to defined sector size and alignment in `esp_mbr_generate_extra_args_t args` below
            .size = esp_ext_part_sector_count_to_bytes(7953, mbr_args.sector_size),
            .type = ESP_EXT_PART_TYPE_FAT12,
            .label = NULL,
        }
    };
    esp_ext_part_list_item_t item2 = {
        .info = {
            .address = 10000, // Should be round up to 10240 (aligned to 1MiB) due to defined sector size and alignment in `esp_mbr_generate_extra_args_t args` below
            .size = esp_ext_part_sector_count_to_bytes(2 * 10240, mbr_args.sector_size),
            .type = ESP_EXT_PART_TYPE_LITTLEFS,
            .label = NULL,
            .extra = 4096, // LittleFS block size stored in CHS hack
            .flags = ESP_EXT_PART_FLAG_EXTRA, // Extra flag set to indicate that the extra field is used
        }
    };

    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item1));
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item2));

    // Generate the MBR
    TEST_ESP_OK(esp_mbr_generate(mbr, &part_list, &mbr_args));
    // Deinitialize the part list
    esp_ext_part_list_deinit(&part_list);

    // Parse the MBR
    TEST_ESP_OK(esp_mbr_parse((void*) mbr, &part_list, NULL));
    free(mbr);

    // Print the partition list
    it = esp_ext_part_list_item_head(&part_list);
    TEST_ASSERT_NOT_NULL(it);
    print_esp_ext_part_list_item(it);
    fflush(stdout);

    // Deinitialize the part list
    esp_ext_part_list_deinit(&part_list);
    it = NULL;
}

TEST_CASE("Test esp_ext_part_list_signature_t get and set", "[esp_ext_part_table]")
{
    esp_ext_part_list_t part_list = {0};
    TEST_ESP_OK(esp_mbr_parse((void*) mbr_bin, &part_list, NULL));
    TEST_ASSERT_EQUAL(part_list.signature.type, ESP_EXT_PART_LIST_SIGNATURE_MBR);

    uint32_t disk_signature = 0;
    uint32_t new_signature = 0x12345678;
    TEST_ESP_OK(esp_ext_part_list_signature_get(&part_list, &disk_signature));
    TEST_ASSERT_NOT_EQUAL(disk_signature, new_signature);

    TEST_ESP_OK(esp_ext_part_list_signature_set(&part_list, &new_signature, ESP_EXT_PART_LIST_SIGNATURE_MBR));
    TEST_ESP_OK(esp_ext_part_list_signature_get(&part_list, &disk_signature));
    TEST_ASSERT_EQUAL(disk_signature, new_signature);

    // Deinitialize the part list
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

void app_main(void)
{
    unity_run_menu();
}
