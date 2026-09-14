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
               (uint32_t)(it->info.type));
        i++;
    } while ((it = esp_ext_part_list_item_next(it)) != NULL);
}

TEST_CASE("Test mbr_bin struct", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) mbr_bin;
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ASSERT_EQUAL(ESP_MBR_SIGNATURE, mbr->boot_signature);
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
    TEST_ASSERT_NULL(part_list.head.slh_first);
}

TEST_CASE("Test esp_mbr_parse rejects a non-empty partition list", "[esp_ext_part_table]")
{
    esp_ext_part_list_t part_list = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr_bin, &part_list, NULL));

    int count_before = 0;
    for (esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(&part_list); it != NULL; it = esp_ext_part_list_item_next(it)) {
        count_before++;
    }
    TEST_ASSERT_GREATER_THAN(0, count_before);

    // Parsing into the same list again would merge two tables and leak the first one's
    // items, so it must be refused and leave the list untouched.
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_mbr_parse((void *) mbr_bin, &part_list, NULL));

    int count_after = 0;
    for (esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(&part_list); it != NULL; it = esp_ext_part_list_item_next(it)) {
        count_after++;
    }
    TEST_ASSERT_EQUAL(count_before, count_after);

    // After deinit the same list can be reused
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
    TEST_ESP_OK(esp_mbr_parse((void *) mbr_bin, &part_list, NULL));
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

void generate_original_mbr(esp_mbr_t *mbr)
{
    esp_mbr_generate_extra_args_t mbr_args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
        .align_policy = ESP_EXT_PART_ALIGN_POLICY_KEEP_SIZE, // Opt in to moving the explicit start below
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
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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

    uint8_t *mbr_bin_from_part_table = (uint8_t *) mbr_bin + ESP_MBR_PARTITION_TABLE_OFFSET;
    uint8_t *mbr_from_part_table = (uint8_t *) mbr + ESP_MBR_PARTITION_TABLE_OFFSET;
    uint8_t compare_size = mbr_bin_len - ESP_MBR_PARTITION_TABLE_OFFSET;
    // Test if the generated MBR is the same as the original MBR - only from partition table part
    // Disk signature is randomly generated, so we don't compare it
    TEST_ASSERT_EQUAL_MEMORY(mbr_bin_from_part_table, mbr_from_part_table, compare_size);
    free(mbr);
    esp_ext_part_list_deinit(&part_list1);
    esp_ext_part_list_deinit(&part_list2);
}

TEST_CASE("Test esp_mbr_generate overwrites stale content in a non-blank buffer", "[esp_ext_part_table]")
{
    // The partition list is the single source of truth for the generated table:
    // regenerating into a buffer that still holds an older MBR must not leave any
    // trace of it behind, while the bootstrap code must be preserved.
    esp_ext_part_list_t part_list = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr_bin, &part_list, NULL)); // 2 FAT12 partitions
    TEST_ASSERT_EQUAL(0, part_list.flags & ESP_EXT_PART_LIST_FLAG_READ_ONLY);

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    memcpy(mbr, mbr_bin, ESP_MBR_SIZE);

    // Dirty the buffer the way a previously loaded MBR would
    mbr->partition_table[2].type = 0x0b;
    mbr->partition_table[2].lba_start = 0x11223344;
    mbr->partition_table[2].sector_count = 0x55667788;
    esp_mbr_chs_arr_val_set(mbr->partition_table[2].chs_start, 0xAABBCC);
    mbr->partition_table[3] = mbr->partition_table[2];
    mbr->partition_table[0].status = ESP_MBR_PARTITION_STATUS_ACTIVE; // Stale "bootable" flag
    mbr->copy_protected = ESP_MBR_COPY_PROTECTED;                     // Stale read-only marker
    memset(mbr->bootstrap_code_modern_part1, 0xEE, sizeof(mbr->bootstrap_code_modern_part1));

    TEST_ESP_OK(esp_mbr_generate(mbr, &part_list, NULL));

    // Entries the list does not cover must be zeroed, not left over
    const esp_mbr_partition_t empty_entry = {0};
    TEST_ASSERT_EQUAL_MEMORY(&empty_entry, &mbr->partition_table[2], sizeof(empty_entry));
    TEST_ASSERT_EQUAL_MEMORY(&empty_entry, &mbr->partition_table[3], sizeof(empty_entry));

    // Per-entry fields must not keep stale values either
    TEST_ASSERT_EQUAL(0, mbr->partition_table[0].status);

    // Copy protection follows the list, which is not read-only
    TEST_ASSERT_EQUAL(0, mbr->copy_protected);

    // Bootstrap code is preserved (that is the point of read-modify-write)
    for (size_t i = 0; i < sizeof(mbr->bootstrap_code_modern_part1); i++) {
        TEST_ASSERT_EQUAL_HEX8(0xEE, mbr->bootstrap_code_modern_part1[i]);
    }

    // The two real partitions still match the source table
    TEST_ASSERT_EQUAL_MEMORY((uint8_t *) mbr_bin + ESP_MBR_PARTITION_TABLE_OFFSET,
                             (uint8_t *) mbr + ESP_MBR_PARTITION_TABLE_OFFSET,
                             2 * sizeof(esp_mbr_partition_t));

    // Re-parsing must yield exactly the two original partitions, not four
    esp_ext_part_list_t reparsed = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &reparsed, NULL));
    int count = 0;
    for (esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(&reparsed); it != NULL; it = esp_ext_part_list_item_next(it)) {
        count++;
    }
    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL(0, reparsed.flags & ESP_EXT_PART_LIST_FLAG_READ_ONLY);

    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&reparsed));
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

TEST_CASE("Test esp_mbr_generate with esp_mbr_parse", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr;
    mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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
    TEST_ASSERT_NULL(part_list.head.slh_first);

    // Another MBR

    mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    esp_mbr_generate_extra_args_t mbr_args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB
    };

    // 2 FAT12 partitions with same parameters as in the original MBR in the array
    esp_ext_part_list_item_t item1 = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(8, mbr_args.sector_size), // Explicit address, kept as is (default align_policy is KEEP_ADDRESS)
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

TEST_CASE("Test esp_ext_part_list_deep_copy", "[esp_ext_part_table]")
{
    esp_ext_part_list_t src = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr_bin, &src, NULL));
    src.sector_size = ESP_EXT_PART_SECTOR_SIZE_4KiB; // Also check the non-item fields travel

    // Labels are owned by the list (MBR itself has none), so add one to prove the copy
    // duplicates the string instead of sharing the pointer.
    char label[] = "data";
    esp_ext_part_list_item_t labelled = {
        .info = {
            .address = 64 * 1024 * 1024,
            .size = 8 * 1024 * 1024,
            .type = ESP_EXT_PART_TYPE_RAW_DATA,
            .label = label,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&src, &labelled));

    esp_ext_part_list_t dst = {0};
    TEST_ESP_OK(esp_ext_part_list_deep_copy(&dst, &src));

    // Same contents, including the fields outside the item list
    TEST_ASSERT_EQUAL(src.sector_size, dst.sector_size);
    TEST_ASSERT_EQUAL(src.flags, dst.flags);
    TEST_ASSERT_EQUAL(src.signature.data[0], dst.signature.data[0]);

    esp_ext_part_list_item_t *s = esp_ext_part_list_item_head(&src);
    esp_ext_part_list_item_t *d = esp_ext_part_list_item_head(&dst);
    int count = 0;
    while (s != NULL && d != NULL) {
        TEST_ASSERT_NOT_EQUAL(s, d); // Distinct items, not shared pointers
        TEST_ASSERT_EQUAL_UINT64(s->info.address, d->info.address);
        TEST_ASSERT_EQUAL_UINT64(s->info.size, d->info.size);
        TEST_ASSERT_EQUAL(s->info.type, d->info.type);
        TEST_ASSERT_EQUAL(s->info.flags, d->info.flags);
        if (s->info.label != NULL) {
            TEST_ASSERT_NOT_NULL(d->info.label);
            TEST_ASSERT_NOT_EQUAL(s->info.label, d->info.label); // Duplicated, not shared
            TEST_ASSERT_EQUAL_STRING(s->info.label, d->info.label);
        } else {
            TEST_ASSERT_NULL(d->info.label);
        }
        s = esp_ext_part_list_item_next(s);
        d = esp_ext_part_list_item_next(d);
        count++;
    }
    TEST_ASSERT_NULL(s);
    TEST_ASSERT_NULL(d);
    TEST_ASSERT_EQUAL(3, count); // 2 from the MBR + the labelled one

    // The copy is independent: editing it must not touch the source
    esp_ext_part_list_item_head(&dst)->info.size = 1;
    TEST_ASSERT_NOT_EQUAL(1, esp_ext_part_list_item_head(&src)->info.size);

    // Copying into a list that still holds items would drop them, so it is refused
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_ext_part_list_deep_copy(&dst, &src));

    // NULL arguments
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ext_part_list_deep_copy(NULL, &src));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ext_part_list_deep_copy(&dst, NULL));

    // Freeing both must not double free the duplicated label
    TEST_ESP_OK(esp_ext_part_list_deinit(&dst));
    TEST_ESP_OK(esp_ext_part_list_deinit(&src));

    // An emptied destination can be copied into again
    esp_ext_part_list_t src2 = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr_bin, &src2, NULL));
    TEST_ESP_OK(esp_ext_part_list_deep_copy(&dst, &src2));
    TEST_ASSERT_NOT_NULL(esp_ext_part_list_item_head(&dst));
    TEST_ESP_OK(esp_ext_part_list_deinit(&dst));
    TEST_ESP_OK(esp_ext_part_list_deinit(&src2));
}

TEST_CASE("Test esp_ext_part_list_signature_t get and set", "[esp_ext_part_table]")
{
    esp_ext_part_list_t part_list = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr_bin, &part_list, NULL));
    TEST_ASSERT_EQUAL(part_list.signature.type, ESP_EXT_PART_LIST_SIGNATURE_MBR);

    esp_ext_part_list_signature_t disk_signature = {0};
    const esp_ext_part_list_signature_t new_signature = {
        .data = { 0x12345678 },
        .type = ESP_EXT_PART_LIST_SIGNATURE_MBR,
    };
    TEST_ESP_OK(esp_ext_part_list_signature_get(&part_list, &disk_signature));
    TEST_ASSERT_EQUAL(disk_signature.type, ESP_EXT_PART_LIST_SIGNATURE_MBR);
    TEST_ASSERT_NOT_EQUAL(disk_signature.data[0], new_signature.data[0]);

    TEST_ESP_OK(esp_ext_part_list_signature_set(&part_list, &new_signature));
    TEST_ESP_OK(esp_ext_part_list_signature_get(&part_list, &disk_signature));
    TEST_ASSERT_EQUAL(disk_signature.data[0], new_signature.data[0]);
    TEST_ASSERT_EQUAL(disk_signature.type, ESP_EXT_PART_LIST_SIGNATURE_MBR);

    // A signature with an unsupported type must be rejected and must not modify the list
    const esp_ext_part_list_signature_t bad_signature = {
        .data = { 0xDEADBEEF },
        .type = (esp_ext_part_signature_type_t) 0xFF,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_ext_part_list_signature_set(&part_list, &bad_signature));
    TEST_ESP_OK(esp_ext_part_list_signature_get(&part_list, &disk_signature));
    TEST_ASSERT_EQUAL(disk_signature.data[0], new_signature.data[0]);

    // NULL arguments
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ext_part_list_signature_get(&part_list, NULL));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ext_part_list_signature_set(&part_list, NULL));

    // Deinitialize the part list
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

TEST_CASE("Test esp_mbr_partition_set and esp_mbr_remove_gaps_between_partition_entries", "[esp_ext_part_table]")
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
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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
    TEST_ASSERT_EQUAL(2, partition_count); // Partitions in slots 1 and 4 are both read despite the empty slots
    it = esp_ext_part_list_item_head(&part_list_from_mbr);
    TEST_ASSERT_EQUAL(1, it->info.slot);
    TEST_ASSERT_EQUAL(4, esp_ext_part_list_item_next(it)->info.slot);

    // Print the partition list
    printf("Partition list after creating gaps (partitions 0 and 3 are still read):\n");
    it = esp_ext_part_list_item_head(&part_list_from_mbr);
    TEST_ASSERT_NOT_NULL(it);
    print_esp_ext_part_list_items(it);
    fflush(stdout);

    // Deinitialize the part list
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list_from_mbr));

    // Now remove the gaps between partition entries
    esp_mbr_remove_gaps_between_partition_entries(mbr);
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

TEST_CASE("Test esp_mbr_partition_set requires a concrete sector size", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    esp_ext_part_list_item_t item = {
        .info = {
            .address = 1024 * 1024,
            .size = 10 * 1024 * 1024,
            .type = ESP_EXT_PART_TYPE_FAT12,
        }
    };

    // A zero-initialized args struct means "all defaults" for esp_mbr_generate, but this
    // low-level function resolves nothing: without a sector size the byte<->sector math
    // would collapse to a zero-length entry at LBA 0, so it must be rejected instead.
    esp_mbr_generate_extra_args_t no_sector_size = {0};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mbr_partition_set(mbr, 0, &item, &no_sector_size));
    // The rejected call must not have touched the entry
    const esp_mbr_partition_t empty_entry = {0};
    TEST_ASSERT_EQUAL_MEMORY(&empty_entry, &mbr->partition_table[0], sizeof(empty_entry));

    // Clearing an entry needs no sector size and stays allowed
    const esp_ext_part_list_item_t none_item = { .info = { .type = ESP_EXT_PART_TYPE_NONE } };
    TEST_ESP_OK(esp_mbr_partition_set(mbr, 0, &none_item, &no_sector_size));

    // With a concrete sector size the same item is accepted
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_NONE,
    };
    TEST_ESP_OK(esp_mbr_partition_set(mbr, 0, &item, &args));
    TEST_ASSERT_EQUAL(2048, mbr->partition_table[0].lba_start);
    TEST_ASSERT_EQUAL(20480, mbr->partition_table[0].sector_count);

    free(mbr);
}

// ---------------------------------------------------------------------------
// Alignment policy, alignment sentinels, and layout validation tests
// ---------------------------------------------------------------------------

// Helper: generate a single-partition MBR and return the raw partition entry 0.
static esp_err_t gen_single_partition(esp_mbr_t *mbr,
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

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    // Unaligned start (sector 8) -> error.
    esp_err_t err = gen_single_partition(mbr,
                                         esp_ext_part_sector_count_to_bytes(8, ESP_EXT_PART_SECTOR_SIZE_512B),
                                         esp_ext_part_sector_count_to_bytes(7953, ESP_EXT_PART_SECTOR_SIZE_512B),
                                         ESP_EXT_PART_TYPE_FAT12, &args);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, err);

    // Pre-aligned start (sector 2048) -> ok, unchanged.
    memset(mbr, 0, sizeof(esp_mbr_t));
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

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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
    // KEEP_SIZE: explicit addresses are only aligned when the policy asks for it
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_AUTO,
        .align_policy = ESP_EXT_PART_ALIGN_POLICY_KEEP_SIZE,
    };
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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
        .align_policy = ESP_EXT_PART_ALIGN_POLICY_KEEP_SIZE,
    };
    esp_mbr_t *mbr2 = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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

TEST_CASE("Test more than 4 partitions are rejected, not truncated", "[esp_ext_part_table]")
{
    esp_ext_part_list_t part_list = {0};
    for (int i = 0; i < ESP_MBR_MAX_PARTITION_COUNT + 1; i++) {
        esp_ext_part_list_item_t item = {
            .info = {
                .size = 1024 * 1024,
                .type = ESP_EXT_PART_TYPE_FAT32,
                .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS,
            }
        };
        TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item));
    }

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, esp_mbr_generate(mbr, &part_list, NULL));
    // Nothing was written, not even the boot signature.
    TEST_ASSERT_EQUAL(0, mbr->boot_signature);
    TEST_ASSERT_EQUAL(0, mbr->partition_table[0].type);

    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

TEST_CASE("Test failed generate leaves the MBR buffer untouched", "[esp_ext_part_table]")
{
    // Pre-load the buffer with an "existing" MBR, as in a read-modify-write.
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    esp_mbr_t *orig = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ASSERT_NOT_NULL(orig);
    memset(mbr->bootstrap_code_classical, 0xA5, sizeof(mbr->bootstrap_code_classical));
    mbr->boot_signature = ESP_MBR_SIGNATURE;
    mbr->disk_signature = 0x12345678;
    for (int i = 0; i < ESP_MBR_MAX_PARTITION_COUNT; i++) {
        mbr->partition_table[i].type = 0x0C;
        mbr->partition_table[i].lba_start = 2048 + i * 4096;
        mbr->partition_table[i].sector_count = 2048;
    }
    memcpy(orig, mbr, sizeof(esp_mbr_t));

    // The first entry is valid and gets built; the second overlaps it, which is only
    // detected by the final layout validation.
    esp_ext_part_list_t part_list = {0};
    esp_ext_part_list_item_t item1 = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(2048, ESP_EXT_PART_SECTOR_SIZE_512B),
            .size = esp_ext_part_sector_count_to_bytes(4096, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_FAT16,
        }
    };
    esp_ext_part_list_item_t item2 = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(4096, ESP_EXT_PART_SECTOR_SIZE_512B),
            .size = esp_ext_part_sector_count_to_bytes(4096, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_FAT16,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item1));
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &item2));

    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_NONE,
    };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, esp_mbr_generate(mbr, &part_list, &args));
    TEST_ASSERT_EQUAL_MEMORY(orig, mbr, sizeof(esp_mbr_t));

    free(orig);
    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

// Test 10 & 11: disk-bounds (total_size) check.
TEST_CASE("Test total_size bounds check rejects off-disk partitions", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    // Partition ends at sector 2048+100 = 2148 (= 1099776 bytes). Set total_size just below that.
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
        .align_policy = ESP_EXT_PART_ALIGN_POLICY_KEEP_SIZE, // start 8 is moved to 2048
        .total_size = esp_ext_part_sector_count_to_bytes(2000, ESP_EXT_PART_SECTOR_SIZE_512B), // too small
    };
    esp_err_t err = gen_single_partition(mbr,
                                         esp_ext_part_sector_count_to_bytes(8, ESP_EXT_PART_SECTOR_SIZE_512B),
                                         esp_ext_part_sector_count_to_bytes(100, ESP_EXT_PART_SECTOR_SIZE_512B),
                                         ESP_EXT_PART_TYPE_FAT12, &args);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);

    // total_size large enough -> ok.
    memset(mbr, 0, sizeof(esp_mbr_t));
    args.total_size = esp_ext_part_sector_count_to_bytes(4096, ESP_EXT_PART_SECTOR_SIZE_512B);
    TEST_ESP_OK(gen_single_partition(mbr,
                                     esp_ext_part_sector_count_to_bytes(8, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     esp_ext_part_sector_count_to_bytes(100, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     ESP_EXT_PART_TYPE_FAT12, &args));

    // total_size == 0 -> check skipped even for a huge partition.
    memset(mbr, 0, sizeof(esp_mbr_t));
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

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    // Ends exactly at sector 100 (start 50 + 50): within the floored capacity -> OK.
    TEST_ESP_OK(gen_single_partition(mbr,
                                     esp_ext_part_sector_count_to_bytes(50, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     esp_ext_part_sector_count_to_bytes(50, ESP_EXT_PART_SECTOR_SIZE_512B),
                                     ESP_EXT_PART_TYPE_FAT12, &args));
    TEST_ASSERT_EQUAL_UINT32(100, mbr->partition_table[0].lba_start + mbr->partition_table[0].sector_count);

    // Ends at sector 101 (start 50 + 51): past the floored capacity of 100 -> rejected.
    // (Ceiling would have made total_sectors 101 and wrongly accepted this.)
    memset(mbr, 0, sizeof(esp_mbr_t));
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
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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
TEST_CASE("Test esp_mbr_chs_arr_val_set/get round-trip", "[esp_ext_part_table]")
{
    uint8_t chs[3] = {0};

    // The 3 CHS bytes are stored little endian, which is how the LittleFS block size
    // hack reads and writes them.
    esp_mbr_chs_arr_val_set(chs, 0x00AABBCC);
    TEST_ASSERT_EQUAL_HEX8(0xCC, chs[0]);
    TEST_ASSERT_EQUAL_HEX8(0xBB, chs[1]);
    TEST_ASSERT_EQUAL_HEX8(0xAA, chs[2]);
    TEST_ASSERT_EQUAL_HEX32(0x00AABBCC, esp_mbr_chs_arr_val_get(chs));

    // Only the low 24 bits fit; the top byte is dropped
    esp_mbr_chs_arr_val_set(chs, 0xFF123456);
    TEST_ASSERT_EQUAL_HEX32(0x00123456, esp_mbr_chs_arr_val_get(chs));

    esp_mbr_chs_arr_val_set(chs, 0);
    TEST_ASSERT_EQUAL_HEX32(0, esp_mbr_chs_arr_val_get(chs));
}

TEST_CASE("Test esp_mbr_lba_to_chs_arr", "[esp_ext_part_table]")
{
    // Geometry is fixed at 255 heads x 63 sectors/track = 16065 sectors per cylinder.
    // Packing is: byte0 = head, byte1 = high 2 bits of cylinder | 6-bit sector,
    // byte2 = low 8 bits of cylinder. Sector numbering is 1 based.
    uint8_t chs[3];

    // LBA 0 -> C0 H0 S1
    esp_mbr_lba_to_chs_arr(chs, 0);
    TEST_ASSERT_EQUAL_HEX8(0x00, chs[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01, chs[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, chs[2]);

    // Last sector of the first track, then the first sector of the next head
    esp_mbr_lba_to_chs_arr(chs, 62);
    TEST_ASSERT_EQUAL_HEX8(0x00, chs[0]);
    TEST_ASSERT_EQUAL_HEX8(0x3F, chs[1]); // sector 63, no cylinder bits
    TEST_ASSERT_EQUAL_HEX8(0x00, chs[2]);

    esp_mbr_lba_to_chs_arr(chs, 63);
    TEST_ASSERT_EQUAL_HEX8(0x01, chs[0]); // head 1
    TEST_ASSERT_EQUAL_HEX8(0x01, chs[1]); // sector 1
    TEST_ASSERT_EQUAL_HEX8(0x00, chs[2]);

    // Cross-check against the real MBR in mbr_bin: its first partition starts at LBA
    // 2048 and ends at LBA 10000, and carries the CHS values a real formatter wrote.
    const esp_mbr_t *ref = (const esp_mbr_t *) mbr_bin;
    esp_mbr_lba_to_chs_arr(chs, 2048);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ref->partition_table[0].chs_start, chs, 3);
    esp_mbr_lba_to_chs_arr(chs, 2048 + 7953 - 1);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(ref->partition_table[0].chs_end, chs, 3);

    // A cylinder above 255 must put its top 2 bits into byte 1
    esp_mbr_lba_to_chs_arr(chs, 300 * 16065);
    TEST_ASSERT_EQUAL_HEX8(0x00, chs[0]);          // head 0
    TEST_ASSERT_EQUAL_HEX8(0x41, chs[1]);          // cylinder bits 0b01 << 6 | sector 1
    TEST_ASSERT_EQUAL_HEX8(0x2C, chs[2]);          // 300 & 0xFF

    // Beyond the BIOS limit everything saturates at cylinder 1023
    esp_mbr_lba_to_chs_arr(chs, UINT32_MAX);
    TEST_ASSERT_EQUAL_HEX8(0xFF, chs[2]);          // low 8 bits of 1023
    TEST_ASSERT_EQUAL_HEX8(0xC0, chs[1] & 0xC0);   // high 2 bits of 1023
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(ESP_MBR_CHS_MAX_SECTOR, chs[1] & 0x3F);
    TEST_ASSERT_LESS_OR_EQUAL_UINT8(ESP_MBR_CHS_MAX_HEAD, chs[0]);

    // A NULL destination is ignored rather than dereferenced
    esp_mbr_lba_to_chs_arr(NULL, 2048);
}

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

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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
    esp_mbr_t *mbr2 = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
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

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mbr_generate(mbr, &part_list, &args));
    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

// ---------------------------------------------------------------------------
// Parse-time filtering (esp_mbr_parse_extra_args_t.match) and the LOSSY flag
// ---------------------------------------------------------------------------

// Build an MBR with one FAT32, one exFAT/NTFS and one raw-data (0xDA) partition, so
// parse-time filtering behavior can be exercised.
static void generate_mixed_usage_mbr(esp_mbr_t *mbr)
{
    esp_mbr_generate_extra_args_t args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B,
        .alignment = ESP_EXT_PART_ALIGN_1MiB,
    };
    esp_ext_part_list_t part_list = {0};

    esp_ext_part_list_item_t fat = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(2048, ESP_EXT_PART_SECTOR_SIZE_512B),
            .size = esp_ext_part_sector_count_to_bytes(2048, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_FAT32,
        }
    };
    esp_ext_part_list_item_t exfat = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(6144, ESP_EXT_PART_SECTOR_SIZE_512B),
            .size = esp_ext_part_sector_count_to_bytes(2048, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_EXFAT_OR_NTFS,
        }
    };
    esp_ext_part_list_item_t raw = {
        .info = {
            .address = esp_ext_part_sector_count_to_bytes(10240, ESP_EXT_PART_SECTOR_SIZE_512B),
            .size = esp_ext_part_sector_count_to_bytes(2048, ESP_EXT_PART_SECTOR_SIZE_512B),
            .type = ESP_EXT_PART_TYPE_RAW_DATA,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &fat));
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &exfat));
    TEST_ESP_OK(esp_ext_part_list_insert(&part_list, &raw));
    TEST_ESP_OK(esp_mbr_generate(mbr, &part_list, &args));
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
}

static int count_list_items(esp_ext_part_list_t *list)
{
    int count = 0;
    for (esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(list); it != NULL; it = esp_ext_part_list_item_next(it)) {
        count++;
    }
    return count;
}

// Parse-filter predicates.
static bool keep_only_fat32(const esp_ext_part_t *info, void *ctx)
{
    (void) ctx;
    return info->type == ESP_EXT_PART_TYPE_FAT32;
}

static bool keep_fat32_or_raw(const esp_ext_part_t *info, void *ctx)
{
    (void) ctx;
    return info->type == ESP_EXT_PART_TYPE_FAT32 || info->type == ESP_EXT_PART_TYPE_RAW_DATA;
}

// By default (match == NULL) every recognized partition is inserted, including the
// exFAT one; nothing is dropped, so LOSSY must NOT be set.
TEST_CASE("Test parse inserts all recognized types by default (not lossy)", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    generate_mixed_usage_mbr(mbr);

    esp_ext_part_list_t parsed = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &parsed, NULL));

    TEST_ASSERT_EQUAL(3, count_list_items(&parsed));
    TEST_ASSERT_EQUAL(0, parsed.flags & ESP_EXT_PART_LIST_FLAG_LOSSY);

    TEST_ESP_OK(esp_ext_part_list_deinit(&parsed));
    free(mbr);
}

// With a match predicate that keeps only FAT32, the exFAT and raw partitions are
// dropped at parse time, so LOSSY must be set.
TEST_CASE("Test parse match predicate inserts only matching partitions and marks lossy", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    generate_mixed_usage_mbr(mbr);

    esp_mbr_parse_extra_args_t args = {
        .match = { .fn = keep_only_fat32 },
    };
    esp_ext_part_list_t parsed = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &parsed, &args));

    TEST_ASSERT_EQUAL(1, count_list_items(&parsed));
    esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(&parsed);
    TEST_ASSERT_NOT_NULL(it);
    TEST_ASSERT_EQUAL(ESP_EXT_PART_TYPE_FAT32, it->info.type);
    TEST_ASSERT_NOT_EQUAL(0, parsed.flags & ESP_EXT_PART_LIST_FLAG_LOSSY);

    TEST_ESP_OK(esp_ext_part_list_deinit(&parsed));
    free(mbr);
}

// A match predicate keeping FAT32 + raw drops only the exFAT partition (still lossy).
TEST_CASE("Test parse match predicate keeps multiple types", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    generate_mixed_usage_mbr(mbr);

    esp_mbr_parse_extra_args_t args = {
        .match = { .fn = keep_fat32_or_raw },
    };
    esp_ext_part_list_t parsed = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &parsed, &args));

    TEST_ASSERT_EQUAL(2, count_list_items(&parsed));
    TEST_ASSERT_NOT_EQUAL(0, parsed.flags & ESP_EXT_PART_LIST_FLAG_LOSSY);

    TEST_ESP_OK(esp_ext_part_list_deinit(&parsed));
    free(mbr);
}

// A partition with an unknown/extended type (0x05) has no esp_ext_part_type_known_t
// mapping; it is skipped during parse and the list is marked LOSSY.
TEST_CASE("Test parse skips unknown type and marks lossy", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    mbr->boot_signature = ESP_MBR_SIGNATURE;
    // Slot 0: a valid FAT32 (0x0C) partition.
    mbr->partition_table[0].type = 0x0C;
    mbr->partition_table[0].lba_start = 2048;
    mbr->partition_table[0].sector_count = 2048;
    // Slot 1: extended partition (0x05) - unknown to this component.
    mbr->partition_table[1].type = 0x05;
    mbr->partition_table[1].lba_start = 4096;
    mbr->partition_table[1].sector_count = 2048;

    esp_ext_part_list_t parsed = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &parsed, NULL));

    // Only the FAT32 partition made it into the list.
    TEST_ASSERT_EQUAL(1, count_list_items(&parsed));
    TEST_ASSERT_NOT_EQUAL(0, parsed.flags & ESP_EXT_PART_LIST_FLAG_LOSSY);

    TEST_ESP_OK(esp_ext_part_list_deinit(&parsed));
    free(mbr);
}

// --- Predicate-based iteration (esp_ext_part_list_next_matching) ---

// Predicate: match a single fixed type (ignores ctx).
static bool match_is_fat32(const esp_ext_part_t *info, void *ctx)
{
    (void) ctx;
    return info->type == ESP_EXT_PART_TYPE_FAT32;
}

// Predicate: match any type contained in a caller-provided set passed via ctx.
typedef struct {
    const uint8_t *types;
    size_t count;
} type_set_t;

static bool match_type_in_set(const esp_ext_part_t *info, void *ctx)
{
    const type_set_t *set = (const type_set_t *) ctx;
    for (size_t i = 0; i < set->count; i++) {
        if (info->type == set->types[i]) {
            return true;
        }
    }
    return false;
}

// Predicate: match on a runtime field (size at least the threshold passed via ctx).
static bool match_size_at_least(const esp_ext_part_t *info, void *ctx)
{
    uint64_t threshold = *(const uint64_t *) ctx;
    return info->size >= threshold;
}

TEST_CASE("Test esp_ext_part_list_next_matching with a type predicate", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    generate_mixed_usage_mbr(mbr); // FAT32 + exFAT + 0xDA

    esp_ext_part_list_t parsed = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &parsed, NULL));

    // First (and only) FAT32 is found, then NULL.
    esp_ext_part_match_t matcher = { .fn = match_is_fat32 };
    esp_ext_part_list_item_t *it = esp_ext_part_list_next_matching(NULL, &parsed, &matcher);
    TEST_ASSERT_NOT_NULL(it);
    TEST_ASSERT_EQUAL(ESP_EXT_PART_TYPE_FAT32, it->info.type);
    TEST_ASSERT_NULL(esp_ext_part_list_next_matching(it, &parsed, &matcher));

    TEST_ESP_OK(esp_ext_part_list_deinit(&parsed));
    free(mbr);
}

TEST_CASE("Test esp_ext_part_list_next_matching with a ctx set predicate", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    generate_mixed_usage_mbr(mbr); // FAT32 + exFAT + 0xDA

    esp_ext_part_list_t parsed = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &parsed, NULL));

    // "Types I can handle in this build" = FAT32 + raw data. Should match 2 items.
    uint8_t wanted[] = { ESP_EXT_PART_TYPE_FAT32, ESP_EXT_PART_TYPE_RAW_DATA };
    type_set_t set = { .types = wanted, .count = 2 };
    esp_ext_part_match_t matcher = { .fn = match_type_in_set, .ctx = &set };

    int matches = 0;
    for (esp_ext_part_list_item_t *it = esp_ext_part_list_next_matching(NULL, &parsed, &matcher);
            it != NULL;
            it = esp_ext_part_list_next_matching(it, &parsed, &matcher)) {
        matches++;
    }
    TEST_ASSERT_EQUAL(2, matches);

    TEST_ESP_OK(esp_ext_part_list_deinit(&parsed));
    free(mbr);
}

TEST_CASE("Test esp_ext_part_list_next_matching branches on a runtime field", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    generate_mixed_usage_mbr(mbr); // all three partitions are 2048 sectors == 1 MiB

    esp_ext_part_list_t parsed = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &parsed, NULL));

    // Threshold above every partition's size -> no match.
    uint64_t big = esp_ext_part_sector_count_to_bytes(4096, ESP_EXT_PART_SECTOR_SIZE_512B);
    esp_ext_part_match_t big_matcher = { .fn = match_size_at_least, .ctx = &big };
    TEST_ASSERT_NULL(esp_ext_part_list_next_matching(NULL, &parsed, &big_matcher));

    // Threshold at or below every partition's size -> all three match.
    uint64_t small = esp_ext_part_sector_count_to_bytes(2048, ESP_EXT_PART_SECTOR_SIZE_512B);
    esp_ext_part_match_t small_matcher = { .fn = match_size_at_least, .ctx = &small };
    int matches = 0;
    for (esp_ext_part_list_item_t *it = esp_ext_part_list_next_matching(NULL, &parsed, &small_matcher);
            it != NULL;
            it = esp_ext_part_list_next_matching(it, &parsed, &small_matcher)) {
        matches++;
    }
    TEST_ASSERT_EQUAL(3, matches);

    TEST_ESP_OK(esp_ext_part_list_deinit(&parsed));
    free(mbr);
}

TEST_CASE("Test esp_ext_part_list_next_matching handles NULL predicate and list", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    generate_mixed_usage_mbr(mbr);

    esp_ext_part_list_t parsed = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &parsed, NULL));

    // NULL matcher pointer -> nothing matches.
    TEST_ASSERT_NULL(esp_ext_part_list_next_matching(NULL, &parsed, NULL));
    // Matcher with NULL fn -> nothing matches.
    esp_ext_part_match_t empty = {0};
    TEST_ASSERT_NULL(esp_ext_part_list_next_matching(NULL, &parsed, &empty));
    // NULL list with NULL from -> NULL.
    esp_ext_part_match_t matcher = { .fn = match_is_fat32 };
    TEST_ASSERT_NULL(esp_ext_part_list_next_matching(NULL, NULL, &matcher));

    TEST_ESP_OK(esp_ext_part_list_deinit(&parsed));
    free(mbr);
}

// The stock esp_ext_part_match_mountable predicate: FAT* always mountable, the
// non-filesystem / undrivable types never, and LittleFS mountable here because the
// test build defines ESP_EXT_PART_HAS_LITTLEFS.
TEST_CASE("Test esp_ext_part_match_mountable classifies types", "[esp_ext_part_table]")
{
    esp_ext_part_match_t matcher = esp_ext_part_match_mountable();
    TEST_ASSERT_NOT_NULL(matcher.fn);
    esp_ext_part_t info = {0};

    info.type = ESP_EXT_PART_TYPE_FAT12;
    TEST_ASSERT_TRUE(matcher.fn(&info, matcher.ctx));
    info.type = ESP_EXT_PART_TYPE_FAT16;
    TEST_ASSERT_TRUE(matcher.fn(&info, matcher.ctx));
    info.type = ESP_EXT_PART_TYPE_FAT32;
    TEST_ASSERT_TRUE(matcher.fn(&info, matcher.ctx));

    // LittleFS: mountable because ESP_EXT_PART_HAS_LITTLEFS is defined for this build,
    // but only with a usable block size.
    info.type = ESP_EXT_PART_TYPE_LITTLEFS;
    TEST_ASSERT_FALSE(matcher.fn(&info, matcher.ctx)); // no block size
    info.flags = ESP_EXT_PART_FLAG_EXTRA;
    info.extra = 4096;
    TEST_ASSERT_TRUE(matcher.fn(&info, matcher.ctx));
    info.flags = ESP_EXT_PART_FLAG_NONE;
    info.extra = 0;

    // Not mountable: raw data, exFAT/NTFS, Linux, GPT-protective, none.
    info.type = ESP_EXT_PART_TYPE_RAW_DATA;
    TEST_ASSERT_FALSE(matcher.fn(&info, matcher.ctx));
    info.type = ESP_EXT_PART_TYPE_EXFAT_OR_NTFS;
    TEST_ASSERT_FALSE(matcher.fn(&info, matcher.ctx));
    info.type = ESP_EXT_PART_TYPE_LINUX_ANY;
    TEST_ASSERT_FALSE(matcher.fn(&info, matcher.ctx));
    info.type = ESP_EXT_PART_TYPE_GPT_PROTECTIVE_MBR;
    TEST_ASSERT_FALSE(matcher.fn(&info, matcher.ctx));
    info.type = ESP_EXT_PART_TYPE_NONE;
    TEST_ASSERT_FALSE(matcher.fn(&info, matcher.ctx));
}

// The stock predicate composes with the iterator: on a FAT32 + exFAT + 0xDA disk,
// only the FAT32 partition is mountable.
TEST_CASE("Test esp_ext_part_match_mountable via next_matching", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    generate_mixed_usage_mbr(mbr); // FAT32 (mountable) + exFAT + 0xDA (not)

    esp_ext_part_list_t parsed = {0};
    TEST_ESP_OK(esp_mbr_parse((void *) mbr, &parsed, NULL));

    int matches = 0;
    esp_ext_part_list_item_t *first = NULL;
    esp_ext_part_match_t matcher = esp_ext_part_match_mountable();
    for (esp_ext_part_list_item_t *it = esp_ext_part_list_next_matching(NULL, &parsed, &matcher);
            it != NULL;
            it = esp_ext_part_list_next_matching(it, &parsed, &matcher)) {
        if (first == NULL) {
            first = it;
        }
        matches++;
    }
    TEST_ASSERT_EQUAL(1, matches);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_EQUAL(ESP_EXT_PART_TYPE_FAT32, first->info.type);

    TEST_ESP_OK(esp_ext_part_list_deinit(&parsed));
    free(mbr);
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

TEST_CASE("Test esp_ext_part_probe detects the partition table format", "[esp_ext_part_table]")
{
    size_t buffer_size = 512;
    uint8_t *buffer = (uint8_t *) malloc(buffer_size);
    TEST_ASSERT_NOT_NULL(buffer);
    esp_blockdev_handle_t handle = NULL;
    TEST_ESP_OK(bdl_simulated_get_blockdev(buffer, buffer_size, &handle));
    TEST_ASSERT_NOT_NULL(handle);

    esp_ext_part_signature_type_t type = (esp_ext_part_signature_type_t) 0xFF;

    // A blank device has no boot signature at all
    memset(buffer, 0, buffer_size);
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_ext_part_probe(handle, &type));

    // A plain MBR
    TEST_ESP_OK(handle->ops->write(handle, mbr_bin, 0, mbr_bin_len));
    TEST_ESP_OK(esp_ext_part_probe(handle, &type));
    TEST_ASSERT_EQUAL(ESP_EXT_PART_LIST_SIGNATURE_MBR, type);

    // The same sector with a protective entry is a GPT disk. This is the case a caller
    // cannot distinguish without probing: parsing it as MBR still succeeds.
    esp_mbr_t protective = *(const esp_mbr_t *) mbr_bin;
    memset(protective.partition_table, 0, sizeof(protective.partition_table));
    protective.partition_table[0].type = ESP_MBR_PARTITION_TYPE_GPT_PROTECTIVE;
    protective.partition_table[0].lba_start = 1;
    protective.partition_table[0].sector_count = 0xFFFFFFFF;
    TEST_ESP_OK(handle->ops->write(handle, (const uint8_t *) &protective, 0, ESP_MBR_SIZE));
    TEST_ESP_OK(esp_ext_part_probe(handle, &type));
    TEST_ASSERT_EQUAL(ESP_EXT_PART_LIST_SIGNATURE_GPT, type);

    // Parsing a GPT disk as MBR yields only the protective entry, which is why probing
    // first is worthwhile.
    esp_ext_part_list_t part_list = {0};
    TEST_ESP_OK(esp_mbr_bdl_read(handle, &part_list, NULL));
    esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(&part_list);
    TEST_ASSERT_NOT_NULL(it);
    TEST_ASSERT_EQUAL(ESP_EXT_PART_TYPE_GPT_PROTECTIVE_MBR, it->info.type);
    TEST_ASSERT_NULL(esp_ext_part_list_item_next(it));
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));

    // NULL arguments
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ext_part_probe(NULL, &type));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_ext_part_probe(handle, NULL));

    handle->ops->release(handle);
    free(buffer);
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
    err = esp_mbr_bdl_read(handle, &part_list, &mbr_parse_args);
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

    err = esp_mbr_bdl_write(handle, &part_list, &mbr_gen_args);
    TEST_ESP_OK(err);

    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));

    err = esp_mbr_bdl_read(handle, &part_list, &mbr_parse_args);
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
    err = esp_mbr_bdl_write(handle, &part_list, &mbr_gen_args);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, err);

    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));
    handle->ops->release(handle);
    free(buffer);
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
    TEST_ESP_OK(esp_mbr_bdl_write(handle, &part_list, &mbr_gen_args));
    TEST_ESP_OK(esp_ext_part_list_deinit(&part_list));

    // Read the MBR back and confirm the FILL partition reaches the disk end.
    esp_mbr_parse_extra_args_t mbr_parse_args = {
        .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B
    };
    TEST_ESP_OK(esp_mbr_bdl_read(handle, &part_list, &mbr_parse_args));
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

// ---------------------------------------------------------------------------
// Empty slots in the middle of an MBR partition table and slot preservation
// ---------------------------------------------------------------------------

static void set_raw_entry(esp_mbr_t *mbr, int idx, uint8_t type, uint32_t lba, uint32_t count)
{
    memset(&mbr->partition_table[idx], 0, sizeof(esp_mbr_partition_t));
    mbr->partition_table[idx].type = type;
    mbr->partition_table[idx].lba_start = lba;
    mbr->partition_table[idx].sector_count = count;
}

static int list_count(esp_ext_part_list_t *list)
{
    int n = 0;
    for (esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(list); it != NULL; it = esp_ext_part_list_item_next(it)) {
        n++;
    }
    return n;
}

// fdisk/Windows leave a zeroed slot when a non-last partition is deleted; the Linux
// kernel reads all four slots. Every used slot must be parsed, whatever precedes it.
TEST_CASE("Test esp_mbr_parse reads partitions after an empty slot", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    mbr->boot_signature = ESP_MBR_SIGNATURE;
    set_raw_entry(mbr, 1, 0x0C, 2048, 4096);  // slot 2 (1-based)
    set_raw_entry(mbr, 3, 0x0C, 8192, 4096);  // slot 4, slots 1 and 3 empty

    esp_ext_part_list_t list = {0};
    TEST_ESP_OK(esp_mbr_parse(mbr, &list, NULL));
    TEST_ASSERT_EQUAL(2, list_count(&list));

    esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(&list);
    TEST_ASSERT_EQUAL(2, it->info.slot);
    TEST_ASSERT_EQUAL_UINT64(2048 * 512ULL, it->info.address);
    it = esp_ext_part_list_item_next(it);
    TEST_ASSERT_EQUAL(4, it->info.slot);
    TEST_ASSERT_EQUAL_UINT64(8192 * 512ULL, it->info.address);

    // No partition was dropped, so the list is not lossy
    TEST_ASSERT_FALSE(list.flags & ESP_EXT_PART_LIST_FLAG_LOSSY);

    TEST_ESP_OK(esp_ext_part_list_deinit(&list));
    free(mbr);
}

TEST_CASE("Test esp_mbr_generate compacts slots by default", "[esp_ext_part_table]")
{
    esp_ext_part_list_t list = {0};
    esp_ext_part_list_item_t item = {
        .info = {
            .address = 2048 * 512ULL,
            .size = 4096 * 512ULL,
            .type = ESP_EXT_PART_TYPE_FAT32,
            .slot = 3,
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&list, &item));

    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ESP_OK(esp_mbr_generate(mbr, &list, NULL));
    TEST_ASSERT_EQUAL_HEX8(0x0C, mbr->partition_table[0].type);
    TEST_ASSERT_EQUAL_HEX8(0x00, mbr->partition_table[2].type);

    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));
}

TEST_CASE("Test esp_mbr_generate preserve_slots pins and fills free slots", "[esp_ext_part_table]")
{
    esp_ext_part_list_t list = {0};
    esp_ext_part_list_item_t pinned = {
        .info = {
            .address = 8192 * 512ULL,
            .size = 4096 * 512ULL,
            .type = ESP_EXT_PART_TYPE_FAT32,
            .slot = 3,
        }
    };
    esp_ext_part_list_item_t unpinned = {
        .info = {
            .address = 2048 * 512ULL,
            .size = 4096 * 512ULL,
            .type = ESP_EXT_PART_TYPE_FAT12,
            .slot = 0, // auto
        }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&list, &pinned));
    TEST_ESP_OK(esp_ext_part_list_insert(&list, &unpinned));

    esp_mbr_generate_extra_args_t args = { .preserve_slots = true };
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ESP_OK(esp_mbr_generate(mbr, &list, &args));

    TEST_ASSERT_EQUAL_HEX8(0x01, mbr->partition_table[0].type); // unpinned -> lowest free slot
    TEST_ASSERT_EQUAL_UINT32(2048, mbr->partition_table[0].lba_start);
    TEST_ASSERT_EQUAL_HEX8(0x00, mbr->partition_table[1].type);
    TEST_ASSERT_EQUAL_HEX8(0x0C, mbr->partition_table[2].type); // pinned to slot 3
    TEST_ASSERT_EQUAL_UINT32(8192, mbr->partition_table[2].lba_start);
    TEST_ASSERT_EQUAL_HEX8(0x00, mbr->partition_table[3].type);

    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));
}

TEST_CASE("Test esp_mbr_generate preserve_slots rejects bad slots", "[esp_ext_part_table]")
{
    esp_mbr_generate_extra_args_t args = { .preserve_slots = true };
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    // Duplicate slot
    esp_ext_part_list_t list = {0};
    esp_ext_part_list_item_t a = {
        .info = { .address = 2048 * 512ULL, .size = 1024 * 512ULL, .type = ESP_EXT_PART_TYPE_FAT12, .slot = 2 }
    };
    esp_ext_part_list_item_t b = {
        .info = { .address = 4096 * 512ULL, .size = 1024 * 512ULL, .type = ESP_EXT_PART_TYPE_FAT12, .slot = 2 }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&list, &a));
    TEST_ESP_OK(esp_ext_part_list_insert(&list, &b));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mbr_generate(mbr, &list, &args));
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));

    // Out-of-range slot (also rejected without preserve_slots)
    a.info.slot = ESP_MBR_MAX_PARTITION_COUNT + 1;
    TEST_ESP_OK(esp_ext_part_list_insert(&list, &a));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mbr_generate(mbr, &list, &args));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mbr_generate(mbr, &list, NULL));
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));

    free(mbr);
}

// parse -> generate(preserve_slots, keep_signature, no alignment) must reproduce the
// partition table of a holey MBR byte for byte (CHS aside, which is not set here).
TEST_CASE("Test holey MBR round-trips with preserve_slots", "[esp_ext_part_table]")
{
    esp_mbr_t *src = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(src);
    src->boot_signature = ESP_MBR_SIGNATURE;
    src->disk_signature = 0x12345678;
    set_raw_entry(src, 1, 0x0C, 2048, 4096);
    set_raw_entry(src, 3, 0xDA, 8192, 4096);

    esp_ext_part_list_t list = {0};
    TEST_ESP_OK(esp_mbr_parse(src, &list, NULL));

    esp_mbr_t *dst = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(dst);
    esp_mbr_generate_extra_args_t args = {
        .alignment = ESP_EXT_PART_ALIGN_NONE,
        .keep_signature = true,
        .preserve_slots = true,
    };
    TEST_ESP_OK(esp_mbr_generate(dst, &list, &args));

    for (int i = 0; i < ESP_MBR_MAX_PARTITION_COUNT; i++) {
        TEST_ASSERT_EQUAL_HEX8(src->partition_table[i].type, dst->partition_table[i].type);
        TEST_ASSERT_EQUAL_UINT32(src->partition_table[i].lba_start, dst->partition_table[i].lba_start);
        TEST_ASSERT_EQUAL_UINT32(src->partition_table[i].sector_count, dst->partition_table[i].sector_count);
    }
    TEST_ASSERT_EQUAL_HEX32(0x12345678, dst->disk_signature);

    TEST_ESP_OK(esp_ext_part_list_deinit(&list));
    free(src);
    free(dst);
}

// ---------------------------------------------------------------------------
// Explicit addresses are not moved by default (align_policy KEEP_ADDRESS)
// ---------------------------------------------------------------------------

// A partition at LBA 63 (pre-2008 DOS layout) must stay there when a parsed table is
// regenerated with default arguments; moving it would orphan the filesystem.
TEST_CASE("Test default generate keeps explicit unaligned addresses", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ESP_OK(gen_single_partition(mbr, 63 * 512ULL, 100000 * 512ULL, ESP_EXT_PART_TYPE_FAT32, NULL));
    TEST_ASSERT_EQUAL_UINT32(63, mbr->partition_table[0].lba_start);
    TEST_ASSERT_EQUAL_UINT32(100000, mbr->partition_table[0].sector_count);
    free(mbr);
}

TEST_CASE("Test AUTO_ADDRESS partitions are still aligned by default", "[esp_ext_part_table]")
{
    esp_ext_part_list_t list = {0};
    esp_ext_part_list_item_t item = {
        .info = { .size = 1000 * 512ULL, .type = ESP_EXT_PART_TYPE_FAT12, .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&list, &item));
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ESP_OK(esp_mbr_generate(mbr, &list, NULL));
    TEST_ASSERT_EQUAL_UINT32(2048, mbr->partition_table[0].lba_start);
    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));
}

// ---------------------------------------------------------------------------
// A boot sector without a partition table (e.g. a FAT volume boot record on a
// "superfloppy" SD card) also ends in 0x55AA and must not be parsed as an MBR.
// ---------------------------------------------------------------------------

TEST_CASE("Test esp_mbr_parse rejects a sector with invalid status bytes", "[esp_ext_part_table]")
{
    uint8_t *sector = (uint8_t *) calloc(1, ESP_MBR_SIZE);
    TEST_ASSERT_NOT_NULL(sector);
    memcpy(sector, mbr_bin, ESP_MBR_SIZE);
    esp_mbr_t *mbr = (esp_mbr_t *) sector;

    // Valid statuses parse
    mbr->partition_table[0].status = ESP_MBR_PARTITION_STATUS_ACTIVE;
    esp_ext_part_list_t list = {0};
    TEST_ESP_OK(esp_mbr_parse(sector, &list, NULL));
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));

    // Boot code bytes in the "status" position => not an MBR
    mbr->partition_table[1].status = 0x29;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_mbr_parse(sector, &list, NULL));
    TEST_ASSERT_NULL(esp_ext_part_list_item_head(&list));

    // An unused entry with a non-zero status is rejected as well
    mbr->partition_table[1].status = 0x00;
    mbr->partition_table[3].status = 0x01;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_mbr_parse(sector, &list, NULL));

    free(sector);
}

// ---------------------------------------------------------------------------
// A list type that the generator cannot map must not become an empty (0x00) slot
// ---------------------------------------------------------------------------

static uint8_t gen_type_map_nothing(uint8_t type)
{
    (void) type;
    return 0x00;
}

TEST_CASE("Test esp_mbr_generate rejects a type that maps to 0x00", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    esp_mbr_generate_extra_args_t args = { .esp_mbr_generate_custom_supported_partition_types = gen_type_map_nothing };
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED,
                      gen_single_partition(mbr, 2048 * 512ULL, 1000 * 512ULL, ESP_EXT_PART_TYPE_FAT12, &args));

    // Default mapper: an out-of-range type value maps to 0x00 too
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, gen_single_partition(mbr, 2048 * 512ULL, 1000 * 512ULL, 0x7F, NULL));

    // The buffer was left untouched
    for (size_t i = 0; i < sizeof(esp_mbr_t); i++) {
        TEST_ASSERT_EQUAL_HEX8(0, ((uint8_t *) mbr)[i]);
    }
    free(mbr);
}

// ---------------------------------------------------------------------------
// CHS for LBAs beyond the CHS range must be the conventional maximum (1023/254/63)
// ---------------------------------------------------------------------------

TEST_CASE("Test esp_mbr_lba_to_chs_arr saturates to FE FF FF", "[esp_ext_part_table]")
{
    uint8_t chs[3];

    // In range: LBA 2048 => C=0, H=32, S=33
    esp_mbr_lba_to_chs_arr(chs, 2048);
    TEST_ASSERT_EQUAL_HEX8(0x20, chs[0]);
    TEST_ASSERT_EQUAL_HEX8(0x21, chs[1]);
    TEST_ASSERT_EQUAL_HEX8(0x00, chs[2]);

    // Last addressable CHS: C=1023 H=254 S=63
    esp_mbr_lba_to_chs_arr(chs, 1024UL * 255 * 63 - 1);
    TEST_ASSERT_EQUAL_HEX8(0xFE, chs[0]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, chs[1]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, chs[2]);

    // Beyond the range: saturate
    const uint32_t beyond[] = { 1024UL * 255 * 63, 1024UL * 255 * 63 + 100, UINT32_MAX };
    for (size_t i = 0; i < sizeof(beyond) / sizeof(beyond[0]); i++) {
        esp_mbr_lba_to_chs_arr(chs, beyond[i]);
        TEST_ASSERT_EQUAL_HEX8(0xFE, chs[0]);
        TEST_ASSERT_EQUAL_HEX8(0xFF, chs[1]);
        TEST_ASSERT_EQUAL_HEX8(0xFF, chs[2]);
    }
}

// ---------------------------------------------------------------------------
// Zero-size partitions
// ---------------------------------------------------------------------------

TEST_CASE("Test zero-size explicit partition is rejected", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);

    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, gen_single_partition(mbr, 2048 * 512ULL, 0, ESP_EXT_PART_TYPE_FAT12, NULL));

    esp_ext_part_list_item_t item = {
        .info = { .address = 0, .size = 0, .type = ESP_EXT_PART_TYPE_FAT12 }
    };
    esp_mbr_generate_extra_args_t args = { .sector_size = ESP_EXT_PART_SECTOR_SIZE_512B };
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, esp_mbr_partition_set(mbr, 0, &item, &args));
    TEST_ASSERT_EQUAL_HEX8(0, mbr->partition_table[0].type);
    free(mbr);
}

#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))
// ---------------------------------------------------------------------------
// Block devices with an I/O unit larger than 512 B and erase-before-write
// ---------------------------------------------------------------------------

#define FLASH_SIM_UNIT 4096

typedef struct {
    uint8_t *data;
    int erase_count;
} flash_sim_t;

static esp_err_t flash_sim_read(esp_blockdev_handle_t h, uint8_t *dst, size_t dst_size, uint64_t addr, size_t len)
{
    flash_sim_t *f = (flash_sim_t *) h->ctx;
    if (addr % FLASH_SIM_UNIT || len % FLASH_SIM_UNIT || len > dst_size || addr + len > h->geometry.disk_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    memcpy(dst, f->data + addr, len);
    return ESP_OK;
}

static esp_err_t flash_sim_write(esp_blockdev_handle_t h, const uint8_t *src, uint64_t addr, size_t len)
{
    flash_sim_t *f = (flash_sim_t *) h->ctx;
    if (addr % FLASH_SIM_UNIT || len % FLASH_SIM_UNIT || addr + len > h->geometry.disk_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    for (size_t i = 0; i < len; i++) {
        f->data[addr + i] &= src[i]; // NOR flash: a write can only clear bits
    }
    return ESP_OK;
}

static esp_err_t flash_sim_erase(esp_blockdev_handle_t h, uint64_t addr, size_t len)
{
    flash_sim_t *f = (flash_sim_t *) h->ctx;
    if (addr % FLASH_SIM_UNIT || len % FLASH_SIM_UNIT || addr + len > h->geometry.disk_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    memset(f->data + addr, 0xFF, len);
    f->erase_count++;
    return ESP_OK;
}

static esp_err_t flash_sim_release(esp_blockdev_handle_t h)
{
    free(h);
    return ESP_OK;
}

static const esp_blockdev_ops_t flash_sim_ops = {
    .read = flash_sim_read,
    .write = flash_sim_write,
    .erase = flash_sim_erase,
    .release = flash_sim_release,
};

static esp_blockdev_handle_t flash_sim_get(flash_sim_t *f, size_t size)
{
    esp_blockdev_handle_t h = (esp_blockdev_handle_t) calloc(1, sizeof(esp_blockdev_t));
    TEST_ASSERT_NOT_NULL(h);
    h->ctx = f;
    h->device_flags.erase_before_write = 1;
    h->device_flags.and_type_write = 1;
    h->device_flags.default_val_after_erase = 1;
    h->geometry.disk_size = size;
    h->geometry.read_size = FLASH_SIM_UNIT;
    h->geometry.write_size = FLASH_SIM_UNIT;
    h->geometry.erase_size = FLASH_SIM_UNIT;
    h->ops = &flash_sim_ops;
    return h;
}

// Small enough to fit in target RAM: 64 KiB device, 4 KiB alignment, one 16 KiB partition
#define SMALL_DISK_SIZE (64 * 1024)
static const esp_mbr_generate_extra_args_t small_disk_args = { .alignment = ESP_EXT_PART_ALIGN_4KiB };

static void one_fat_partition_list(esp_ext_part_list_t *list)
{
    esp_ext_part_list_item_t item = {
        .info = { .size = 16 * 1024, .type = ESP_EXT_PART_TYPE_FAT12, .flags = ESP_EXT_PART_FLAG_AUTO_ADDRESS }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(list, &item));
}

TEST_CASE("Test BDL helpers on a 4 KiB-unit erase-before-write device", "[esp_ext_part_table]")
{
    const size_t size = SMALL_DISK_SIZE;
    flash_sim_t f = { .data = malloc(size) };
    TEST_ASSERT_NOT_NULL(f.data);
    memset(f.data, 0xFF, size);
    // Pre-existing content in the first unit: bootstrap code and data past the MBR
    for (int i = 0; i < 440; i++) {
        f.data[i] = (uint8_t) i;
    }
    for (int i = ESP_MBR_SIZE; i < FLASH_SIM_UNIT; i++) {
        f.data[i] = (uint8_t)(i * 7);
    }
    esp_blockdev_handle_t h = flash_sim_get(&f, size);

    esp_ext_part_list_t list = {0};
    one_fat_partition_list(&list);
    TEST_ESP_OK(esp_mbr_bdl_write(h, &list, &small_disk_args));
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));
    TEST_ASSERT_EQUAL(1, f.erase_count);

    // Bootstrap code and the rest of the unit survived
    for (int i = 0; i < 440; i++) {
        TEST_ASSERT_EQUAL_HEX8((uint8_t) i, f.data[i]);
    }
    for (int i = ESP_MBR_SIZE; i < FLASH_SIM_UNIT; i++) {
        TEST_ASSERT_EQUAL_HEX8((uint8_t)(i * 7), f.data[i]);
    }

    esp_ext_part_signature_type_t type;
    TEST_ESP_OK(esp_ext_part_probe(h, &type));
    TEST_ASSERT_EQUAL(ESP_EXT_PART_LIST_SIGNATURE_MBR, type);

    TEST_ESP_OK(esp_mbr_bdl_read(h, &list, NULL));
    esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(&list);
    TEST_ASSERT_NOT_NULL(it);
    TEST_ASSERT_EQUAL_UINT64(4096, it->info.address);
    TEST_ASSERT_EQUAL_UINT64(16 * 1024, it->info.size);
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));

    h->ops->release(h);
    free(f.data);
}

TEST_CASE("Test esp_mbr_bdl_write preserves the bootstrap code", "[esp_ext_part_table]")
{
    const size_t size = SMALL_DISK_SIZE;
    uint8_t *buffer = calloc(1, size);
    TEST_ASSERT_NOT_NULL(buffer);
    for (int i = 0; i < 440; i++) {
        buffer[i] = (uint8_t)(0xA0 ^ i);
    }
    esp_blockdev_handle_t h = NULL;
    TEST_ESP_OK(bdl_simulated_get_blockdev(buffer, size, &h));

    esp_ext_part_list_t list = {0};
    one_fat_partition_list(&list);
    TEST_ESP_OK(esp_mbr_bdl_write(h, &list, &small_disk_args));
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));

    for (int i = 0; i < 440; i++) {
        TEST_ASSERT_EQUAL_HEX8((uint8_t)(0xA0 ^ i), buffer[i]);
    }
    TEST_ASSERT_EQUAL_HEX16(ESP_MBR_SIGNATURE, ((esp_mbr_t *) buffer)->boot_signature);

    h->ops->release(h);
    free(buffer);
}

TEST_CASE("Test esp_ext_part_probe rejects a volume boot record", "[esp_ext_part_table]")
{
    const size_t size = 4096;
    uint8_t *buffer = calloc(1, size);
    TEST_ASSERT_NOT_NULL(buffer);
    memcpy(buffer, mbr_bin, ESP_MBR_SIZE);
    ((esp_mbr_t *) buffer)->partition_table[2].status = 0x4E; // boot code byte
    esp_blockdev_handle_t h = NULL;
    TEST_ESP_OK(bdl_simulated_get_blockdev(buffer, size, &h));

    esp_ext_part_signature_type_t type;
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, esp_ext_part_probe(h, &type));

    h->ops->release(h);
    free(buffer);
}
#endif // (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(6, 0, 0))

// ---------------------------------------------------------------------------
// LittleFS block size stored in the CHS-start bytes of a 0xC3 entry
// ---------------------------------------------------------------------------

static void parse_single_c3(const uint8_t chs[3], esp_ext_part_list_t *list)
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    mbr->boot_signature = ESP_MBR_SIGNATURE;
    set_raw_entry(mbr, 0, 0xC3, 2048, 4096);
    memcpy(mbr->partition_table[0].chs_start, chs, 3);
    TEST_ESP_OK(esp_mbr_parse(mbr, list, NULL));
    free(mbr);
    TEST_ASSERT_NOT_NULL(esp_ext_part_list_item_head(list));
}

TEST_CASE("Test LittleFS block size is parsed only when plausible", "[esp_ext_part_table]")
{
    esp_ext_part_list_t list = {0};
    esp_ext_part_list_item_t *it;

    // 4096 (0x001000) written by this library
    parse_single_c3((const uint8_t[3]) {
        0x00, 0x10, 0x00
    }, &list);
    it = esp_ext_part_list_item_head(&list);
    TEST_ASSERT_EQUAL_UINT64(4096, it->info.extra);
    TEST_ASSERT_TRUE(it->info.flags & ESP_EXT_PART_FLAG_EXTRA);
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));

    // Zero: no block size
    parse_single_c3((const uint8_t[3]) {
        0, 0, 0
    }, &list);
    it = esp_ext_part_list_item_head(&list);
    TEST_ASSERT_EQUAL_UINT64(0, it->info.extra);
    TEST_ASSERT_FALSE(it->info.flags & ESP_EXT_PART_FLAG_EXTRA);
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));

    // Real CHS of LBA 2048 (20 21 00 => 0x2120) from another tool: not a block size
    parse_single_c3((const uint8_t[3]) {
        0x20, 0x21, 0x00
    }, &list);
    it = esp_ext_part_list_item_head(&list);
    TEST_ASSERT_EQUAL_UINT64(0, it->info.extra);
    TEST_ASSERT_FALSE(it->info.flags & ESP_EXT_PART_FLAG_EXTRA);
    esp_ext_part_match_t m = esp_ext_part_match_mountable();
    TEST_ASSERT_FALSE(m.fn(&it->info, m.ctx));
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));

    // Power of two but out of range (64 B and 2 MiB)
    parse_single_c3((const uint8_t[3]) {
        0x40, 0x00, 0x00
    }, &list);
    TEST_ASSERT_FALSE(esp_ext_part_list_item_head(&list)->info.flags & ESP_EXT_PART_FLAG_EXTRA);
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));
    parse_single_c3((const uint8_t[3]) {
        0x00, 0x00, 0x20
    }, &list);
    TEST_ASSERT_FALSE(esp_ext_part_list_item_head(&list)->info.flags & ESP_EXT_PART_FLAG_EXTRA);
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));
}

TEST_CASE("Test LittleFS block size is validated on generate", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    esp_ext_part_list_t list = {0};
    esp_ext_part_list_item_t item = {
        .info = { .address = 2048 * 512ULL, .size = 4096 * 512ULL, .type = ESP_EXT_PART_TYPE_LITTLEFS, .flags = ESP_EXT_PART_FLAG_EXTRA }
    };

    const uint64_t bad[] = { 8480, 64, 2 * 1024 * 1024, 0x1000000, 0x1001000 };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        item.info.extra = bad[i];
        TEST_ESP_OK(esp_ext_part_list_insert(&list, &item));
        TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, esp_mbr_generate(mbr, &list, NULL));
        TEST_ESP_OK(esp_ext_part_list_deinit(&list));
    }

    const uint64_t good[] = { 128, 4096, 1024 * 1024 };
    for (size_t i = 0; i < sizeof(good) / sizeof(good[0]); i++) {
        item.info.extra = good[i];
        TEST_ESP_OK(esp_ext_part_list_insert(&list, &item));
        TEST_ESP_OK(esp_mbr_generate(mbr, &list, NULL));
        TEST_ASSERT_EQUAL_UINT32((uint32_t) good[i], esp_mbr_chs_arr_val_get(mbr->partition_table[0].chs_start));
        TEST_ESP_OK(esp_ext_part_list_deinit(&list));
    }

    // No block size: allowed, CHS-start written as zeros
    memset(mbr->partition_table[0].chs_start, 0xAA, 3);
    item.info.extra = 0;
    item.info.flags = ESP_EXT_PART_FLAG_NONE;
    TEST_ESP_OK(esp_ext_part_list_insert(&list, &item));
    TEST_ESP_OK(esp_mbr_generate(mbr, &list, NULL));
    TEST_ASSERT_EQUAL_UINT32(0, esp_mbr_chs_arr_val_get(mbr->partition_table[0].chs_start));
    TEST_ASSERT_EQUAL_HEX8(0xC3, mbr->partition_table[0].type);
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));

    free(mbr);
}

// A disk with a 0xC3 entry without a block size must not block editing the table
TEST_CASE("Test 0xC3 entry without block size round-trips", "[esp_ext_part_table]")
{
    esp_ext_part_list_t list = {0};
    parse_single_c3((const uint8_t[3]) {
        0x20, 0x21, 0x00
    }, &list);
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    TEST_ESP_OK(esp_mbr_generate(mbr, &list, NULL));
    TEST_ASSERT_EQUAL_HEX8(0xC3, mbr->partition_table[0].type);
    TEST_ASSERT_EQUAL_UINT32(2048, mbr->partition_table[0].lba_start);
    free(mbr);
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));
}

TEST_CASE("Test generate rejects a partition at sector 0", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    memset(mbr, 0xAB, sizeof(esp_mbr_t));
    esp_mbr_t before = *mbr;
    esp_ext_part_list_t list = {0};
    esp_ext_part_list_item_t item = {
        .info = { .address = 0, .size = 100 * 512ULL, .type = ESP_EXT_PART_TYPE_FAT12 }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&list, &item));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mbr_generate(mbr, &list, NULL));
    TEST_ASSERT_EQUAL_MEMORY(&before, mbr, sizeof(esp_mbr_t));
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));
    free(mbr);
}

TEST_CASE("Test generate rejects a start address that is not sector aligned", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    esp_ext_part_list_t list = {0};
    esp_ext_part_list_item_t item = {
        .info = { .address = 2048 * 512ULL + 1, .size = 100 * 512ULL, .type = ESP_EXT_PART_TYPE_FAT12 }
    };
    TEST_ESP_OK(esp_ext_part_list_insert(&list, &item));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, esp_mbr_generate(mbr, &list, NULL));
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));

    // A size that is not a whole number of sectors is still rounded up
    item.info.address = 2048 * 512ULL;
    item.info.size = 100 * 512ULL + 1;
    TEST_ESP_OK(esp_ext_part_list_insert(&list, &item));
    TEST_ESP_OK(esp_mbr_generate(mbr, &list, NULL));
    TEST_ASSERT_EQUAL_UINT32(2048, mbr->partition_table[0].lba_start);
    TEST_ASSERT_EQUAL_UINT32(101, mbr->partition_table[0].sector_count);
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));
    free(mbr);
}

TEST_CASE("Test parse skips entries that cannot be regenerated", "[esp_ext_part_table]")
{
    esp_mbr_t *mbr = (esp_mbr_t *) calloc(1, sizeof(esp_mbr_t));
    TEST_ASSERT_NOT_NULL(mbr);
    mbr->boot_signature = ESP_MBR_SIGNATURE;
    mbr->partition_table[0].type = 0x0C; // zero sectors
    mbr->partition_table[0].lba_start = 2048;
    mbr->partition_table[1].type = 0x0C; // starts at the MBR sector
    mbr->partition_table[1].lba_start = 0;
    mbr->partition_table[1].sector_count = 100;
    mbr->partition_table[2].type = 0x0C; // valid
    mbr->partition_table[2].lba_start = 4096;
    mbr->partition_table[2].sector_count = 100;

    esp_ext_part_list_t list = {0};
    TEST_ESP_OK(esp_mbr_parse(mbr, &list, NULL));
    TEST_ASSERT_TRUE(list.flags & ESP_EXT_PART_LIST_FLAG_LOSSY);
    esp_ext_part_list_item_t *it = esp_ext_part_list_item_head(&list);
    TEST_ASSERT_NOT_NULL(it);
    TEST_ASSERT_EQUAL_UINT8(3, it->info.slot);
    TEST_ASSERT_NULL(esp_ext_part_list_item_next(it));

    // The parsed list can be written back
    TEST_ESP_OK(esp_mbr_generate(mbr, &list, NULL));
    TEST_ASSERT_EQUAL_UINT32(4096, mbr->partition_table[0].lba_start);
    TEST_ESP_OK(esp_ext_part_list_deinit(&list));
    free(mbr);
}

void app_main(void)
{
    printf("Running esp_ext_part_tables component tests\n");
    unity_run_menu();
}
