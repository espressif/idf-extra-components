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
#include "unity_test_runner.h"
#include "esp_heap_caps.h"

#if !CONFIG_IDF_TARGET_LINUX
#include "esp_newlib.h"
#endif // !CONFIG_IDF_TARGET_LINUX

#include "unity_test_utils_memory.h"

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
#if !CONFIG_IDF_TARGET_LINUX
    esp_reent_cleanup();    //clean up some of the newlib's lazy allocations
#endif // !CONFIG_IDF_TARGET_LINUX
    unity_utils_evaluate_leaks_direct(0);
}

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

static void print_esp_ext_part_list_items(esp_ext_part_list_item_t *head)
{
    esp_ext_part_list_item_t *it = head;
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
    mbr_t *mbr = (mbr_t *) mbr_bin;
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ASSERT_EQUAL(MBR_SIGNATURE, mbr->boot_signature);
    printf("MBR boot signature: 0x%" PRIX16 "\n", mbr->boot_signature);
    printf("MBR disk signature: 0x%" PRIX32 "\n", mbr->disk_signature);
}

TEST_CASE("Test esp_mbr_parse", "[esp_ext_part_table]")
{
    esp_ext_part_list_t part_list = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr_bin, &part_list, NULL));
    esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(&part_list);
    TEST_ASSERT_NOT_NULL(it);

    print_esp_ext_part_list_items(it);
    fflush(stdout);

    do {
        TEST_ASSERT_NOT_EQUAL(0, it->info.address);
        TEST_ASSERT_NOT_EQUAL(0, it->info.size);
        TEST_ASSERT_NOT_EQUAL(0, it->info.type);
    } while ((it = esp_ext_part_list_item_next(it)) != NULL);
    esp_ext_part_list_deinit(&part_list);
    TEST_ASSERT_EQUAL(0, part_list.head.slh_first);
}

void generate_original_mbr(mbr_t *mbr)
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
    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    generate_original_mbr(mbr);

    esp_ext_part_list_t part_list1 = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &part_list1, NULL));
    esp_ext_part_list_item_t *it1 = esp_ext_part_list_item_head(&part_list1);
    TEST_ASSERT_NOT_NULL(it1);

    esp_ext_part_list_t part_list2 = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr_bin, &part_list2, NULL));
    esp_ext_part_list_item_t *it2 = esp_ext_part_list_item_head(&part_list2);
    TEST_ASSERT_NOT_NULL(it2);

    print_esp_ext_part_list_items(it1);
    print_esp_ext_part_list_items(it2);
    fflush(stdout);

    uint8_t *mbr_bin_from_part_table = (uint8_t *) mbr_bin + MBR_PARTITION_TABLE_OFFSET;
    uint8_t *mbr_from_part_table = (uint8_t *) mbr + MBR_PARTITION_TABLE_OFFSET;
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
    mbr_t *mbr;
    mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    generate_original_mbr(mbr);

    esp_ext_part_list_t part_list = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &part_list, NULL));
    free(mbr);

    esp_ext_part_list_item_t *it;
    it = esp_ext_part_list_item_head(&part_list);
    TEST_ASSERT_NOT_NULL(it);

    // Print the partition list
    print_esp_ext_part_list_items(it);
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

    mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
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
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &part_list, NULL));
    free(mbr);

    // Print the partition list
    it = esp_ext_part_list_item_head(&part_list);
    TEST_ASSERT_NOT_NULL(it);
    print_esp_ext_part_list_items(it);
    fflush(stdout);

    // Deinitialize the part list
    esp_ext_part_list_deinit(&part_list);
    it = NULL;
}

TEST_CASE("Test esp_ext_part_list_signature_t get and set", "[esp_ext_part_table]")
{
    esp_ext_part_list_t part_list = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr_bin, &part_list, NULL));
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

TEST_CASE("Test esp_mbr_partition_set and esp_mbr_remove_gaps_between_partiton_entries", "[esp_ext_part_table]")
{
    esp_ext_part_list_t part_list = {0};

    esp_mbr_generate_extra_args_t mbr_args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB
    };

    // 4 FAT12 partitions
    esp_ext_part_list_item_t item = {
        .info = {
            .size = 10 * 1024 * 1024, // 10 MiB
            .type = ESP_EXT_PART_TYPE_FAT12,
        }
    };

    for (int i = 0; i < 4; i++) {
        item.info.address = 1024 * 1024 + i * item.info.size; // First partition starts at 1 MiB offset, next partitions are 10 MiB apart
        TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item));
    }

    printf("Partition list after creation:\n");
    esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(&part_list);
    TEST_ASSERT_NOT_NULL(it);
    print_esp_ext_part_list_items(it);
    fflush(stdout);

    // Generate the MBR
    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    TEST_ESP_OK(esp_mbr_generate(mbr, &part_list, &mbr_args));
    // Deinitialize the part list
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));

    // Create gaps in MBR at index 1 and 2
    // This will remove the second and third partitions from the MBR
    esp_ext_part_list_item_t empty_item = {
        .info = {
            .type = ESP_EXT_PART_TYPE_NONE, // No type
        }
    };
    esp_mbr_partition_set(mbr, 1, &empty_item, &mbr_args);
    esp_mbr_partition_set(mbr, 2, &empty_item, &mbr_args);
    printf("Partition 1 and 2 removed, 0 and 3 remained, gaps created\n\n");

    // Parse the MBR to get the partition list without removing the gaps
    esp_ext_part_list_t part_list_from_mbr = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &part_list_from_mbr, NULL));

    it = esp_ext_part_list_item_head(&part_list_from_mbr);
    TEST_ASSERT_NOT_NULL(it);
    int partition_count = 1;
    while ((it = esp_ext_part_list_item_next(it)) != NULL) {
        partition_count++;
    }
    TEST_ASSERT_EQUAL(partition_count, 1);

    // Print the partition list
    printf("Partition list after creating gaps (partition 3 is missing because the gaps were created and not shifted out):\n");
    it = esp_ext_part_list_item_head(&part_list_from_mbr);
    TEST_ASSERT_NOT_NULL(it);
    print_esp_ext_part_list_items(it);
    fflush(stdout);

    // Deinitialize the part list
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list_from_mbr));

    // Now remove the gaps between partition entries
    esp_mbr_remove_gaps_between_partiton_entries(mbr);
    // Parse the MBR to get the partition list with gaps removed
    esp_ext_part_list_t part_list_from_mbr_correct = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &part_list_from_mbr_correct, NULL));
    free(mbr);

    // Now the partition list should contain 2 partitions (originally partition 0 and 3, now partition 0 and 1)
    it = esp_ext_part_list_item_head(&part_list_from_mbr_correct);
    TEST_ASSERT_NOT_NULL(it);
    partition_count = 1;
    while ((it = esp_ext_part_list_item_next(it)) != NULL) {
        partition_count++;
    }
    TEST_ASSERT_EQUAL(partition_count, 2);

    // Print the partition list
    printf("Partition list after removing gaps (partition 0 stayed the same, partition 3 was shifted and now is partition 1):\n");
    it = esp_ext_part_list_item_head(&part_list_from_mbr_correct);
    TEST_ASSERT_NOT_NULL(it);
    print_esp_ext_part_list_items(it);
    fflush(stdout);

    // Deinitialize the part list
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list_from_mbr_correct));
}

void app_main(void)
{
    printf("Running esp_ext_part_tables component tests\n");
    unity_run_menu();
}
