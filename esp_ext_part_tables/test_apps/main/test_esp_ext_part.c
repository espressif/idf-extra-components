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
#include "esp_heap_caps.h"
#include "esp_idf_version.h"

#if !CONFIG_IDF_TARGET_LINUX
#include "esp_newlib.h"
#endif // !CONFIG_IDF_TARGET_LINUX

#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"

#include "esp_ext_part_tables.h"
#include "esp_mbr.h"
#include "esp_mbr_utils.h"

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
            // 10000 sectors -> aligned up to 10240 (1 MiB). Note this is expressed in
            // sectors (converted to bytes); using a raw byte value like 10000 here
            // would be only ~20 sectors and would align back to 2048, overlapping
            // item1 - which the generator's overlap validation now rejects.
            .address = esp_ext_part_sector_count_to_bytes(10000, mbr_args.sector_size),
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

// ---------------------------------------------------------------------------
// Alignment policy, alignment sentinels, and layout validation tests
// ---------------------------------------------------------------------------

// Helper: generate a single-partition MBR and return the raw partition entry 0.
static esp_err_t gen_single_partition(mbr_t *mbr,
                                      uint64_t address_bytes,
                                      uint64_t size_bytes,
                                      esp_ext_part_type_known_t type,
                                      esp_mbr_generate_extra_args_t *args)
{
    esp_ext_part_list_t part_list = {0};
    esp_ext_part_list_item_t item = {
        .info = {
            .address = address_bytes,
            .size = size_bytes,
            .type = type,
            .label = NULL,
        }
    };
    esp_err_t err = esp_ext_part_list_insert(&part_list, &item);
    if (err != ESP_OK) {
        esp_ext_part_list_deinit(&part_list);
        return err;
    }
    err = esp_mbr_generate(mbr, &part_list, args);
    esp_ext_part_list_deinit(&part_list);
    return err;
}

// Test 5: KEEP_SIZE (default) - unaligned start is aligned up, size (sector_count) stays unchanged.
TEST_CASE("Test align policy KEEP_SIZE keeps size when start is aligned up", "[esp_ext_part_table]")
{
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
        .align_policy = ESP_EXT_PART_ALIGN_POLICY_KEEP_SIZE,
    };

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    // Start at sector 8 (=> aligned up to 2048), size 7953 sectors.
    TEST_ESP_OK(gen_single_partition(mbr,
                                     esp_ext_part_sector_count_to_bytes(8, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     esp_ext_part_sector_count_to_bytes(7953, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     ESP_EXT_PART_TYPE_FAT12, &args));

    TEST_ASSERT_EQUAL_UINT32(2048, mbr->partition_table[0].lba_start);
    TEST_ASSERT_EQUAL_UINT32(7953, mbr->partition_table[0].sector_count); // size unchanged
    free(mbr);
}

// Test 1: PRESERVE_END (opt-in) - start aligned up, size shrunk so end == original end.
TEST_CASE("Test align policy PRESERVE_END shrinks size to keep the end", "[esp_ext_part_table]")
{
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
        .align_policy = ESP_EXT_PART_ALIGN_POLICY_PRESERVE_END,
    };

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    // Start at sector 8 => aligned to 2048. Original end (exclusive) = 8 + 7953 = 7961.
    // New sector_count should be 7961 - 2048 = 5913.
    TEST_ESP_OK(gen_single_partition(mbr,
                                     esp_ext_part_sector_count_to_bytes(8, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     esp_ext_part_sector_count_to_bytes(7953, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     ESP_EXT_PART_TYPE_FAT12, &args));

    TEST_ASSERT_EQUAL_UINT32(2048, mbr->partition_table[0].lba_start);
    TEST_ASSERT_EQUAL_UINT32(5913, mbr->partition_table[0].sector_count);
    // End preserved:
    TEST_ASSERT_EQUAL_UINT32(7961, mbr->partition_table[0].lba_start + mbr->partition_table[0].sector_count);
    free(mbr);
}

// Test 2: PRESERVE_END - alignment consumes the whole partition -> error.
TEST_CASE("Test align policy PRESERVE_END errors when alignment eats the partition", "[esp_ext_part_table]")
{
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
        .align_policy = ESP_EXT_PART_ALIGN_POLICY_PRESERVE_END,
    };

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    // Start at sector 8 => aligned up to 2048, but size is only 10 sectors (end = 18 < 2048).
    esp_err_t err = gen_single_partition(mbr,
                                         esp_ext_part_sector_count_to_bytes(8, ESP_EXT_PART_SECTOR_SIZE_512B),
                                         esp_ext_part_sector_count_to_bytes(10, ESP_EXT_PART_SECTOR_SIZE_512B),
                                         ESP_EXT_PART_TYPE_FAT12, &args);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);
    free(mbr);
}

// Test 3 & 4: REJECT policy.
TEST_CASE("Test align policy REJECT errors on unaligned start, ok when aligned", "[esp_ext_part_table]")
{
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
        .align_policy = ESP_EXT_PART_ALIGN_POLICY_REJECT,
    };

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    // Unaligned start (sector 8) -> error.
    esp_err_t err = gen_single_partition(mbr,
                                         esp_ext_part_sector_count_to_bytes(8, ESP_EXT_PART_SECTOR_SIZE_512B),
                                         esp_ext_part_sector_count_to_bytes(7953, ESP_EXT_PART_SECTOR_SIZE_512B),
                                         ESP_EXT_PART_TYPE_FAT12, &args);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    // Pre-aligned start (sector 2048) -> ok, unchanged.
    memset(mbr, 0, sizeof(mbr_t));
    TEST_ESP_OK(gen_single_partition(mbr,
                                     esp_ext_part_sector_count_to_bytes(2048, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     esp_ext_part_sector_count_to_bytes(7953, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     ESP_EXT_PART_TYPE_FAT12, &args));
    TEST_ASSERT_EQUAL_UINT32(2048, mbr->partition_table[0].lba_start);
    TEST_ASSERT_EQUAL_UINT32(7953, mbr->partition_table[0].sector_count);
    free(mbr);
}

// Test 6: ALIGN_NONE performs no relocation.
TEST_CASE("Test ALIGN_NONE leaves the start untouched", "[esp_ext_part_table]")
{
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_NONE,
    };

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    // Start at sector 8, no alignment -> lba_start stays 8.
    TEST_ESP_OK(gen_single_partition(mbr,
                                     esp_ext_part_sector_count_to_bytes(8, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     esp_ext_part_sector_count_to_bytes(100, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     ESP_EXT_PART_TYPE_FAT12, &args));
    TEST_ASSERT_EQUAL_UINT32(8, mbr->partition_table[0].lba_start);
    TEST_ASSERT_EQUAL_UINT32(100, mbr->partition_table[0].sector_count);
    free(mbr);
}

// Test 7: ALIGN_AUTO (and zero-initialized alignment) applies the 1 MiB default.
TEST_CASE("Test ALIGN_AUTO applies the 1MiB default alignment", "[esp_ext_part_table]")
{
    // Explicit AUTO
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_AUTO,
    };
    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ESP_OK(gen_single_partition(mbr,
                                     esp_ext_part_sector_count_to_bytes(8, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     esp_ext_part_sector_count_to_bytes(100, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     ESP_EXT_PART_TYPE_FAT12, &args));
    TEST_ASSERT_EQUAL_UINT32(2048, mbr->partition_table[0].lba_start); // aligned to 1 MiB
    free(mbr);

    // Zero-initialized alignment field must also select AUTO (=> 1 MiB), not NONE.
    esp_mbr_generate_extra_args_t args0 = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
    };
    mbr_t *mbr2 = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr2);
    TEST_ESP_OK(gen_single_partition(mbr2,
                                     esp_ext_part_sector_count_to_bytes(8, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     esp_ext_part_sector_count_to_bytes(100, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     ESP_EXT_PART_TYPE_FAT12, &args0));
    TEST_ASSERT_EQUAL_UINT32(2048, mbr2->partition_table[0].lba_start);
    free(mbr2);
}

// Test 8: overlap detection (always on).
TEST_CASE("Test overlapping partitions are rejected", "[esp_ext_part_table]")
{
    esp_ext_part_list_t part_list = {0};
    // Two partitions that overlap: p0 = [2048, 2048+4096), p1 starts at 4096 (< 6144) with NONE alignment.
    esp_ext_part_list_item_t item1 = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(2048, ESP_EXT_PART_SECTOR_SIZE_512B),
            .size = esp_ext_part_sector_count_to_bytes(4096, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_FAT12,
        }
    };
    esp_ext_part_list_item_t item2 = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(4096, ESP_EXT_PART_SECTOR_SIZE_512B),
            .size = esp_ext_part_sector_count_to_bytes(4096, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_FAT12,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item1));
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item2));

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    // With NONE alignment the two partitions overlap -> overlap error.
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_NONE,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_mbr_generate(mbr, &part_list, &args));

    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

// Test 10 & 11: disk-bounds (total_size) check.
TEST_CASE("Test total_size bounds check rejects off-disk partitions", "[esp_ext_part_table]")
{
    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    // Partition ends at sector 2048+100 = 2148 (= 1099776 bytes). Set total_size just below that.
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
        .total_size = esp_ext_part_sector_count_to_bytes(2000, ESP_EXT_PART_SECTOR_SIZE_512B), // too small
    };
    esp_err_t err = gen_single_partition(mbr,
                                         esp_ext_part_sector_count_to_bytes(8, ESP_EXT_PART_SECTOR_SIZE_512B),
                                         esp_ext_part_sector_count_to_bytes(100, ESP_EXT_PART_SECTOR_SIZE_512B),
                                         ESP_EXT_PART_TYPE_FAT12, &args);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);

    // total_size large enough -> ok.
    memset(mbr, 0, sizeof(mbr_t));
    args.total_size = esp_ext_part_sector_count_to_bytes(4096, ESP_EXT_PART_SECTOR_SIZE_512B);
    TEST_ESP_OK(gen_single_partition(mbr,
                                     esp_ext_part_sector_count_to_bytes(8, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     esp_ext_part_sector_count_to_bytes(100, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     ESP_EXT_PART_TYPE_FAT12, &args));

    // total_size == 0 -> check skipped even for a huge partition.
    memset(mbr, 0, sizeof(mbr_t));
    args.total_size = 0;
    TEST_ESP_OK(gen_single_partition(mbr,
                                     esp_ext_part_sector_count_to_bytes(8, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     esp_ext_part_sector_count_to_bytes(100, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     ESP_EXT_PART_TYPE_FAT12, &args));
    free(mbr);
}

// total_size that is NOT a whole multiple of the sector size must be floored, not
// ceiled: a trailing partial sector is not usable capacity. A partition that ends on
// the last WHOLE sector is accepted; one that ends on the (non-existent) partial
// sector beyond it must be rejected. With the old ceiling behavior the latter was
// wrongly accepted (off-disk by up to one sector).
TEST_CASE("Test total_size is floored to whole sectors", "[esp_ext_part_table]")
{
    // 100 whole 512 B sectors + 100 extra bytes -> floor = 100 sectors (usable),
    // ceiling would have been 101.
    const uint64_t total = (uint64_t) 100 * 512 + 100;

    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_NONE, // keep the exact start, no rounding
        .total_size = total,
    };

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    // Ends exactly at sector 100 (start 50 + 50): within the floored capacity -> OK.
    TEST_ESP_OK(gen_single_partition(mbr,
                                     esp_ext_part_sector_count_to_bytes(50, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     esp_ext_part_sector_count_to_bytes(50, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     ESP_EXT_PART_TYPE_FAT12, &args));
    TEST_ASSERT_EQUAL_UINT32(100, mbr->partition_table[0].lba_start + mbr->partition_table[0].sector_count);

    // Ends at sector 101 (start 50 + 51): past the floored capacity of 100 -> rejected.
    // (Ceiling would have made total_sectors 101 and wrongly accepted this.)
    memset(mbr, 0, sizeof(mbr_t));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE,
                      gen_single_partition(mbr,
                                           esp_ext_part_sector_count_to_bytes(50, ESP_EXT_PART_SECTOR_SIZE_512B),
                                           esp_ext_part_sector_count_to_bytes(51, ESP_EXT_PART_SECTOR_SIZE_512B),
                                           ESP_EXT_PART_TYPE_FAT12, &args));
    free(mbr);
}

// A partition whose start and count each fit in 32 bits but whose END (start + count)
// exceeds UINT32_MAX must be rejected: the sum would overflow the 32-bit MBR fields
// and wrap the auto-placement cursor. Use ALIGN_NONE so the start is not rounded.
TEST_CASE("Test partition whose end exceeds the 32-bit MBR range is rejected", "[esp_ext_part_table]")
{
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_NONE,
    };
    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    // start = UINT32_MAX - 10 sectors, count = 20 sectors -> end = UINT32_MAX + 10 (overflow).
    // Both start and count individually fit in 32 bits, but the end does not.
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      gen_single_partition(mbr,
                                           esp_ext_part_sector_count_to_bytes((uint64_t) UINT32_MAX - 10, ESP_EXT_PART_SECTOR_SIZE_512B),
                                           esp_ext_part_sector_count_to_bytes(20, ESP_EXT_PART_SECTOR_SIZE_512B),
                                           ESP_EXT_PART_TYPE_FAT12, &args));
    free(mbr);
}

// esp_mbr_lba_align must round up correctly even when alignment / sector_size is
// not a power of two. The defined enum values all happen to yield a power-of-two
// number of sectors, but a caller may cast a custom alignment value, so the
// rounding must not rely on the power-of-two bitmask idiom.
TEST_CASE("Test esp_mbr_lba_align rounds up for non-power-of-two alignment", "[esp_ext_part_table]")
{
    // alignment = 1536 bytes, sector_size = 512 => alignment_sectors = 3 (not a power of two).
    esp_ext_part_align_t align3 = (esp_ext_part_align_t) 1536;

    // Already-aligned values stay put.
    TEST_ASSERT_EQUAL_UINT32(0, esp_mbr_lba_align(0, ESP_EXT_PART_SECTOR_SIZE_512B, align3));
    TEST_ASSERT_EQUAL_UINT32(3, esp_mbr_lba_align(3, ESP_EXT_PART_SECTOR_SIZE_512B, align3));
    TEST_ASSERT_EQUAL_UINT32(6, esp_mbr_lba_align(6, ESP_EXT_PART_SECTOR_SIZE_512B, align3));

    // Unaligned values round UP to the next multiple of 3.
    TEST_ASSERT_EQUAL_UINT32(3, esp_mbr_lba_align(1, ESP_EXT_PART_SECTOR_SIZE_512B, align3));
    TEST_ASSERT_EQUAL_UINT32(6, esp_mbr_lba_align(4, ESP_EXT_PART_SECTOR_SIZE_512B, align3));
    TEST_ASSERT_EQUAL_UINT32(6, esp_mbr_lba_align(5, ESP_EXT_PART_SECTOR_SIZE_512B, align3));
    TEST_ASSERT_EQUAL_UINT32(9, esp_mbr_lba_align(7, ESP_EXT_PART_SECTOR_SIZE_512B, align3));

    // Sanity: a power-of-two combo (1 MiB / 512 = 2048) still works.
    TEST_ASSERT_EQUAL_UINT32(2048, esp_mbr_lba_align(8, ESP_EXT_PART_SECTOR_SIZE_512B, ESP_EXT_PART_ALIGN_1MiB));
    TEST_ASSERT_EQUAL_UINT32(2048, esp_mbr_lba_align(2048, ESP_EXT_PART_SECTOR_SIZE_512B, ESP_EXT_PART_ALIGN_1MiB));

    // ESP_EXT_PART_ALIGN_NONE and a zero alignment leave the LBA untouched.
    TEST_ASSERT_EQUAL_UINT32(7, esp_mbr_lba_align(7, ESP_EXT_PART_SECTOR_SIZE_512B, ESP_EXT_PART_ALIGN_NONE));
    TEST_ASSERT_EQUAL_UINT32(7, esp_mbr_lba_align(7, ESP_EXT_PART_SECTOR_SIZE_512B, (esp_ext_part_align_t) 0));

    // Overflow guard: aligning an LBA that is already within one alignment_sectors
    // of UINT32_MAX must not wrap; the saturated result is UINT32_MAX.
    // alignment_sectors = 2048 (1 MiB / 512 B); last aligned LBA below UINT32_MAX is
    // 0xFFFFF800 (= 4294965248). The next LBA after that would overflow, so
    // esp_mbr_lba_align must return UINT32_MAX instead.
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, esp_mbr_lba_align(0xFFFFF801, ESP_EXT_PART_SECTOR_SIZE_512B, ESP_EXT_PART_ALIGN_1MiB));
    TEST_ASSERT_EQUAL_UINT32(UINT32_MAX, esp_mbr_lba_align(UINT32_MAX,  ESP_EXT_PART_SECTOR_SIZE_512B, ESP_EXT_PART_ALIGN_1MiB));
}

// ---------------------------------------------------------------------------
// Automatic partition placement (ESP_EXT_PART_FLAG_AUTO_ADDRESS / _FILL) tests
// ---------------------------------------------------------------------------

// Test 1: a single AUTO_ADDRESS partition (first in the list) lands at the first aligned LBA.
TEST_CASE("Test auto-placement: first partition placed at first aligned LBA", "[esp_ext_part_table]")
{
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
    };
    esp_ext_part_list_t part_list = {0};
    esp_ext_part_list_item_t item = {
        .info = {
            .size = esp_ext_part_sector_count_to_bytes(100, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_FAT12,
            .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item));

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ESP_OK(esp_mbr_generate(mbr, &part_list, &args));

    TEST_ASSERT_EQUAL_UINT32(2048, mbr->partition_table[0].lba_start); // first 1 MiB-aligned LBA
    TEST_ASSERT_EQUAL_UINT32(100, mbr->partition_table[0].sector_count);
    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

// Test 2 & 3: AUTO partitions chain contiguously after their predecessor (aligned).
TEST_CASE("Test auto-placement: partitions chain after the previous one", "[esp_ext_part_table]")
{
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
    };
    esp_ext_part_list_t part_list = {0};

    // p0 explicit at sector 2048, size 3000 sectors -> ends at 5048.
    esp_ext_part_list_item_t p0 = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(2048, ESP_EXT_PART_SECTOR_SIZE_512B),
            .size = esp_ext_part_sector_count_to_bytes(3000, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_FAT12,
        }
    };
    // p1 AUTO, size 1000 -> placed at align_up(5048) = 6144.
    esp_ext_part_list_item_t p1 = {
        .info = {
            .size = esp_ext_part_sector_count_to_bytes(1000, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_FAT12,
            .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS,
        }
    };
    // p2 AUTO, size 500 -> placed at align_up(6144+1000=7144) = 8192.
    esp_ext_part_list_item_t p2 = {
        .info = {
            .size = esp_ext_part_sector_count_to_bytes(500, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_FAT12,
            .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &p0));
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &p1));
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &p2));

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ESP_OK(esp_mbr_generate(mbr, &part_list, &args));

    TEST_ASSERT_EQUAL_UINT32(2048, mbr->partition_table[0].lba_start);
    TEST_ASSERT_EQUAL_UINT32(6144, mbr->partition_table[1].lba_start);
    TEST_ASSERT_EQUAL_UINT32(1000, mbr->partition_table[1].sector_count);
    TEST_ASSERT_EQUAL_UINT32(8192, mbr->partition_table[2].lba_start);
    TEST_ASSERT_EQUAL_UINT32(500, mbr->partition_table[2].sector_count);
    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

// Test 5: AUTO + size==0 without FILL -> error.
TEST_CASE("Test auto-placement: size 0 without FILL is rejected", "[esp_ext_part_table]")
{
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
    };
    esp_ext_part_list_t part_list = {0};
    esp_ext_part_list_item_t item = {
        .info = {
            .size = 0,
            .type = ESP_EXT_PART_TYPE_FAT12,
            .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item));

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mbr_generate(mbr, &part_list, &args));
    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

// Test 6 & 7: FILL behavior.
TEST_CASE("Test auto-placement: FILL sizes to the end of the disk", "[esp_ext_part_table]")
{
    esp_ext_part_list_t part_list = {0};
    // p0 AUTO explicit-size 100 -> [2048, 2148). p1 AUTO + FILL -> [align_up(2148)=4096, total).
    esp_ext_part_list_item_t p0 = {
        .info = {
            .size = esp_ext_part_sector_count_to_bytes(100, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_FAT12,
            .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS,
        }
    };
    esp_ext_part_list_item_t p1 = {
        .info = {
            .size = 0,
            .type = ESP_EXT_PART_TYPE_FAT12,
            .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS | ESP_EXT_PART_FLAG_FILL,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &p0));
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &p1));

    // total_size = 20000 sectors.
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
        .total_size = esp_ext_part_sector_count_to_bytes(20000, ESP_EXT_PART_SECTOR_SIZE_512B),
    };

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ESP_OK(esp_mbr_generate(mbr, &part_list, &args));

    TEST_ASSERT_EQUAL_UINT32(4096, mbr->partition_table[1].lba_start);
    // Fills to disk end: start + count == total_sectors (20000).
    TEST_ASSERT_EQUAL_UINT32(20000, mbr->partition_table[1].lba_start + mbr->partition_table[1].sector_count);
    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));

    // Test 7: FILL with no total_size -> error.
    esp_ext_part_list_t pl2 = {0};
    TEST_ESP_OK(esp_ext_part_list_insert(&pl2, &p1));
    esp_mbr_generate_extra_args_t args_no_total = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
    };
    mbr_t *mbr2 = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr2);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, esp_mbr_generate(mbr2, &pl2, &args_no_total));
    free(mbr2);
    TEST_ESP_OK(esp_ext_part_list_deinit(&pl2));
}

// Test 9: the caller's partition items are not mutated by generation.
TEST_CASE("Test auto-placement: caller items are not mutated", "[esp_ext_part_table]")
{
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
    };
    esp_ext_part_list_t part_list = {0};
    esp_ext_part_list_item_t item = {
        .info = {
            .address = 0,
            .size = esp_ext_part_sector_count_to_bytes(100, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_FAT12,
            .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item));

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ESP_OK(esp_mbr_generate(mbr, &part_list, &args));

    // The list item must still have its original address (0) and AUTO flag set.
    esp_ext_part_list_item_t *stored = esp_ext_part_list_item_head(&part_list);
    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_EQUAL_UINT64(0, stored->info.address);
    TEST_ASSERT_TRUE(stored->info.flags & ESP_EXT_PART_FLAG_AUTO_ADDRESS);
    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

// FILL without AUTO_ADDRESS + size 0 must be rejected (it would silently produce a
// zero-sector entry). FILL without AUTO_ADDRESS + non-zero size is allowed (with a
// warning) since the non-zero size still produces a valid entry.
TEST_CASE("Test FILL without AUTO_ADDRESS: size 0 is rejected", "[esp_ext_part_table]")
{
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
    };
    esp_ext_part_list_t part_list = {0};
    esp_ext_part_list_item_t item = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(2048, ESP_EXT_PART_SECTOR_SIZE_512B),
            .size = 0, // size 0 + FILL but no AUTO_ADDRESS -> error
            .type = ESP_EXT_PART_TYPE_FAT12,
            .flags = ESP_EXT_PART_FLAG_FILL,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item));
    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mbr_generate(mbr, &part_list, &args));
    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

// ---------------------------------------------------------------------------
// Empty (ESP_EXT_PART_TYPE_NONE) list-item handling in esp_mbr_generate
// ---------------------------------------------------------------------------

// A ESP_EXT_PART_TYPE_NONE item in the middle of the list would, if written as a
// zeroed slot, create a gap that esp_mbr_parse silently truncates at (data loss)
// and that misplaces auto-placed partitions. esp_mbr_generate must reject such an
// item instead of producing a bad MBR.
TEST_CASE("Test empty partition in list is rejected", "[esp_ext_part_table]")
{
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
    };
    esp_ext_part_list_t part_list = {0};

    esp_ext_part_list_item_t p0 = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(2048, ESP_EXT_PART_SECTOR_SIZE_512B),
            .size = esp_ext_part_sector_count_to_bytes(1000, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_FAT12,
        }
    };
    esp_ext_part_list_item_t empty = {
        .info = {
            .type = ESP_EXT_PART_TYPE_NONE, // gap
        }
    };
    esp_ext_part_list_item_t p2 = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(6144, ESP_EXT_PART_SECTOR_SIZE_512B),
            .size = esp_ext_part_sector_count_to_bytes(1000, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_FAT12,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &p0));
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &empty));
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &p2));

    mbr_t *mbr = (mbr_t *) calloc(1, sizeof(mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mbr_generate(mbr, &part_list, &args));
    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
#include "esp_blockdev.h"

// BDL simulated block device implementation for testing

static esp_err_t bdl_simulated_read(esp_blockdev_handle_t handle, uint8_t *dst_buf, size_t dst_buf_size, uint64_t src_addr, size_t data_read_len)
{
    if (handle == NULL || dst_buf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *buffer = (uint8_t *) handle->ctx;

    if (src_addr + data_read_len > handle->geometry.disk_size || data_read_len > dst_buf_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(dst_buf, buffer + src_addr, data_read_len);
    return ESP_OK;
}

static esp_err_t bdl_simulated_write(esp_blockdev_handle_t handle, const uint8_t *src_buf, uint64_t dst_addr, size_t data_write_len)
{
    if (handle == NULL || src_buf == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t *buffer = (uint8_t *) handle->ctx;

    if (dst_addr + data_write_len > handle->geometry.disk_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(buffer + dst_addr, src_buf, data_write_len);
    return ESP_OK;
}

static esp_err_t bdl_simulated_release_blockdev(esp_blockdev_handle_t handle)
{
    if (handle != NULL) {
        free(handle);
    }
    return ESP_OK;
}

static const esp_blockdev_ops_t bdl_simulated_blockdev_ops = {
    .read = bdl_simulated_read,
    .write = bdl_simulated_write,
    .erase = NULL, // Not recommended to leave as NULL; just for test purposes
    .ioctl = NULL,
    .sync = NULL,
    .release = bdl_simulated_release_blockdev,
};

static esp_err_t bdl_simulated_get_blockdev(uint8_t *buffer, size_t buffer_size, esp_blockdev_handle_t *out_handle)
{
    if (buffer == NULL || out_handle == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_blockdev_handle_t out = (esp_blockdev_handle_t) calloc(1, sizeof(esp_blockdev_t));
    if (out == NULL) {
        return ESP_ERR_NO_MEM;
    }
    out->ctx = (void *) buffer;

    out->device_flags.default_val_after_erase = 0;

    out->geometry.disk_size = buffer_size;
    out->geometry.read_size = 1;
    out->geometry.write_size = 1;
    out->geometry.erase_size = 1;

    out->ops = &bdl_simulated_blockdev_ops;

    *out_handle = out;
    return ESP_OK;
}

TEST_CASE("Test with BDL (simulated in RAM) - basic operations", "[esp_ext_part_table]")
{
    size_t buffer_size = 3 * 1024;
    uint8_t *buffer = (uint8_t *) malloc(buffer_size);
    TEST_ASSERT_NOT_NULL(buffer);
    esp_blockdev_handle_t handle = NULL;
    esp_err_t err = bdl_simulated_get_blockdev(buffer, buffer_size, &handle);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    size_t sector_size = 512;

    // Write As to the first sector
    uint8_t buf[] = {[0 ... 511] = 'A'}; // Fill buffer with 'A'
    err = handle->ops->write(handle, buf, 0, sector_size);
    TEST_ESP_OK(err);

    uint8_t read_buf[512] = {0};
    err = handle->ops->read(handle, read_buf, sector_size, 0, sector_size);
    TEST_ESP_OK(err);
    TEST_ASSERT_EQUAL_MEMORY(buf, read_buf, sizeof(buf));

    // Write Bs to the emulated "first sector" (0 + start sector offset (2) == sector size (512) * 2)
    {
        uint8_t buf2[] = {[0 ... 511] = 'B'}; // Fill buffer with 'B'
        err = handle->ops->write(handle, buf2, sector_size * 2, sector_size);
        TEST_ESP_OK(err);

        err = handle->ops->read(handle, read_buf, sector_size, sector_size * 2, sector_size);
        TEST_ESP_OK(err);
        TEST_ASSERT_EQUAL_MEMORY(buf2, read_buf, sizeof(buf2));
    }

    // Read the first sector again, it should be 'A's
    err = handle->ops->read(handle, read_buf, sector_size, 0, sector_size);
    TEST_ESP_OK(err);
    TEST_ASSERT_EQUAL_MEMORY(buf, read_buf, sizeof(buf));

    // Visualize the first 5 sectors
    for (int i = 0; i < 5; i++) {
        // Read the first sector, it should be 'A's
        err = handle->ops->read(handle, read_buf, sector_size, sector_size * i, sector_size);
        TEST_ESP_OK(err);
        for (int j = 0; j < sizeof(read_buf); j++) {
            printf("%c", read_buf[j]);
        }
        printf("\n");
        fflush(stdout);
    }

    handle->ops->release(handle);
    handle = NULL;
    free(buffer);
    buffer = NULL;
}

TEST_CASE("Test with BDL (simulated in RAM) - MBR related", "[esp_ext_part_table]")
{
    size_t buffer_size = 512;
    uint8_t *buffer = (uint8_t *) malloc(buffer_size);
    TEST_ASSERT_NOT_NULL(buffer);
    esp_blockdev_handle_t handle = NULL;
    esp_err_t err = bdl_simulated_get_blockdev(buffer, buffer_size, &handle);
    TEST_ESP_OK(err);
    TEST_ASSERT_NOT_NULL(handle);

    // The backing buffer is intentionally tiny (one MBR sector) to stay within
    // limited on-target RAM, but the MBR we write declares partitions at high LBAs.
    // Report a realistic disk size so those partitions are within-bounds for the
    // new bdl_write disk-bounds validation. Only the MBR sector (offset 0) is ever
    // actually read/written, so the small backing buffer is never over-indexed.
    handle->geometry.disk_size = 40 * 1024 * 1024; // 40 MiB (reported only, no allocation)

    err = handle->ops->write(handle, mbr_bin, 0, mbr_bin_len);
    TEST_ESP_OK(err);

    esp_mbr_parse_extra_args_t mbr_parse_args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B
    };

    esp_ext_part_list_t part_list = {0};
    esp_ext_part_list_item_t *it = NULL;
    err = esp_ext_part_list_bdl_read(handle, &part_list, ESP_EXT_PART_LIST_SIGNATURE_MBR, (void *) &mbr_parse_args);
    TEST_ESP_OK(err);

    it = esp_ext_part_list_item_head(&part_list);
    TEST_ASSERT_NOT_NULL(it);
    printf("Partition list read from BDL simulated MBR:\n");
    print_esp_ext_part_list_items(it);
    fflush(stdout);

    esp_mbr_generate_extra_args_t mbr_gen_args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB
    };

    esp_ext_part_list_item_t partition_for_insertion = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(20480, mbr_gen_args.sector_size), // 10 MiB offset
            .size = 10 * 1024 * 1024, // 10 MiB
            .type = ESP_EXT_PART_TYPE_LITTLEFS,
            .extra = 4096, // LittleFS block size stored in CHS hack
            .flags = ESP_EXT_PART_FLAG_EXTRA, // Extra flag set to indicate that the extra field is used
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &partition_for_insertion));

    err = esp_ext_part_list_bdl_write(handle, &part_list, ESP_EXT_PART_LIST_SIGNATURE_MBR, (void *) &mbr_gen_args);
    TEST_ESP_OK(err);

    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));

    err = esp_ext_part_list_bdl_read(handle, &part_list, ESP_EXT_PART_LIST_SIGNATURE_MBR, (void *) &mbr_parse_args);
    TEST_ESP_OK(err);

    it = esp_ext_part_list_item_head(&part_list);
    TEST_ASSERT_NOT_NULL(it);
    printf("Partition list after writing new partition to BDL simulated MBR:\n");
    print_esp_ext_part_list_items(it);
    fflush(stdout);

    // Negative case: prove the disk-bounds validation fires through the BDL write
    // path. bdl_write auto-fills total_size from handle->geometry.disk_size (40 MiB
    // reported above) when the caller leaves it 0, so a partition placed past the
    // end of the (reported) disk must be rejected with ESP_ERR_INVALID_SIZE.
    // This is a pure arithmetic check - it does not allocate a 40 MiB buffer.
    esp_ext_part_list_item_t off_disk_partition = {
        .info = {
            .address = 50 * 1024 * 1024, // 50 MiB offset - beyond the 40 MiB reported disk
            .size = 1 * 1024 * 1024,
            .type = ESP_EXT_PART_TYPE_FAT12,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &off_disk_partition));
    err = esp_ext_part_list_bdl_write(handle, &part_list, ESP_EXT_PART_LIST_SIGNATURE_MBR, (void *) &mbr_gen_args);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);

    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

// Test 10: AUTO_ADDRESS + FILL through the BDL write path, with total_size
// auto-filled from the device geometry (no explicit total_size passed).
TEST_CASE("Test auto-placement: FILL via BDL uses device geometry", "[esp_ext_part_table]")
{
    const uint32_t sector_size = 512;
    const uint64_t disk_sectors = 30000; // reported disk size in sectors

    size_t buffer_size = 512; // tiny backing buffer; only the MBR sector is touched
    uint8_t *buffer = (uint8_t *) malloc(buffer_size);
    TEST_ASSERT_NOT_NULL(buffer);
    esp_blockdev_handle_t handle = NULL;
    TEST_ESP_OK(bdl_simulated_get_blockdev(buffer, buffer_size, &handle));
    TEST_ASSERT_NOT_NULL(handle);
    handle->geometry.disk_size = disk_sectors * sector_size; // realistic reported size

    esp_ext_part_list_t part_list = {0};
    esp_ext_part_list_item_t fill_part = {
        .info = {
            .size = 0,
            .type = ESP_EXT_PART_TYPE_FAT12,
            .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS | ESP_EXT_PART_FLAG_FILL,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &fill_part));

    // No total_size in args -> bdl_write auto-fills it from handle->geometry.disk_size.
    esp_mbr_generate_extra_args_t mbr_gen_args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
    };
    TEST_ESP_OK(esp_ext_part_list_bdl_write(handle, &part_list, ESP_EXT_PART_LIST_SIGNATURE_MBR, (void *) &mbr_gen_args));
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));

    // Read the MBR back and confirm the FILL partition reaches the disk end.
    esp_mbr_parse_extra_args_t mbr_parse_args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B
    };
    TEST_ESP_OK(esp_ext_part_list_bdl_read(handle, &part_list, ESP_EXT_PART_LIST_SIGNATURE_MBR, (void *) &mbr_parse_args));
    esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(&part_list);
    TEST_ASSERT_NOT_NULL(it);

    uint64_t start_sec = esp_ext_part_bytes_to_sector_count(it->info.address, ESP_EXT_PART_SECTOR_SIZE_512B);
    uint64_t count_sec = esp_ext_part_bytes_to_sector_count(it->info.size, ESP_EXT_PART_SECTOR_SIZE_512B);
    TEST_ASSERT_EQUAL_UINT32(2048, start_sec); // first aligned LBA
    TEST_ASSERT_EQUAL_UINT64(disk_sectors, start_sec + count_sec); // fills to disk end

    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
    handle->ops->release(handle);
    free(buffer);
}
#endif // (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))

void app_main(void)
{
    printf("Running esp_ext_part_tables component tests\n");
    unity_run_menu();
}
