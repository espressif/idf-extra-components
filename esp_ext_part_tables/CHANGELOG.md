# Changelog

All notable changes to this component will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-10-01

First stable release. The public API is frozen from this version on, which required a
number of breaking renames and signature changes that were deliberately made now rather
than after 1.0.

### Changed

- **Breaking:** prefixed all public MBR macros and types to avoid polluting the global
  namespace: `MBR_SIZE`, `MBR_SIGNATURE`, `MBR_COPY_PROTECTED`,
  `MBR_PARTITION_TABLE_OFFSET`, `MBR_PARTITION_STATUS_ACTIVE`,
  `MBR_MAX_PARTITION_COUNT` and `MBR_CHS_*` are now `ESP_MBR_*`; `mbr_t` and
  `mbr_partition_t` are now `esp_mbr_t` and `esp_mbr_partition_t`.
- **Breaking:** fixed a typo in a public function name:
  `esp_mbr_remove_gaps_between_partiton_entries` is now
  `esp_mbr_remove_gaps_between_partition_entries`.
- **Breaking:** `esp_ext_part_list_signature_get`/`_set` now take a typed
  `esp_ext_part_list_signature_t *` instead of a `void *` buffer, so the size and
  alignment of the signature can no longer be mismatched by the caller. The separate
  `type` argument of `_set` is gone; the type travels inside the struct.
  `_set` now also validates the type before modifying the list.
- **Breaking:** input-only parameters are `const` throughout the public API
  (`esp_mbr_parse`, `esp_mbr_generate`, `esp_mbr_partition_set`,
  `esp_ext_part_list_insert`, `esp_ext_part_list_deep_copy`,
  `esp_ext_part_list_signature_get`, `esp_mbr_bdl_read`/`_write` (previously
  `esp_ext_part_list_bdl_read`/`_write`)).
- **Breaking:** `esp_mbr_parse` returns `ESP_ERR_INVALID_STATE` when the target list
  already holds partitions. Previously it appended to the existing list and silently
  overwrote its signature and sector size, merging two unrelated tables. Call
  `esp_ext_part_list_deinit()` before reusing a list.
- **Breaking:** `esp_mbr_partition_set` returns `ESP_ERR_INVALID_ARG` when
  `extra_args->sector_size` is `ESP_EXT_PART_SECTOR_SIZE_UNKNOWN`. Previously the
  byte-to-sector math silently collapsed to zero and produced a zero-length partition
  entry at LBA 0. Clearing an entry (`ESP_EXT_PART_TYPE_NONE`) still needs no sector
  size. Note that this function resolves no defaults at all, unlike `esp_mbr_generate`.
- **Breaking:** `esp_ext_part_list_deep_copy` returns `ESP_ERR_INVALID_STATE` when the
  destination list already holds partitions. Previously it overwrote the destination's
  list head, which dropped the only pointers to its items and their labels.
- **Breaking:** `esp_ext_part_list_bdl_read`/`_write` are replaced by the typed
  `esp_mbr_bdl_read`/`esp_mbr_bdl_write` in `esp_mbr.h`. The old pair took a `void *`
  whose real type was selected by a `type` argument and could not be checked by the
  compiler. Support for another format would be added as new functions rather than a
  new enumerator, which is additive and does not break callers.
- **Breaking:** `esp_mbr_generate` (and so `esp_mbr_bdl_write`) returns
  `ESP_ERR_NOT_SUPPORTED` for a list with more than 4 partitions. Previously it logged a
  warning, wrote only the first 4 and returned `ESP_OK`.
- **Breaking:** partitions with an explicit address are no longer moved by default.
  Previously every partition start was aligned up (to 1 MiB by default), so
  regenerating a parsed table moved any unaligned partition (e.g. the LBA 63 layout of
  older disks) away from its filesystem. The new default policy
  `ESP_EXT_PART_ALIGN_POLICY_KEEP_ADDRESS` (value 0) writes explicit addresses as
  given; only `ESP_EXT_PART_FLAG_AUTO_ADDRESS` partitions are aligned. The old default
  behavior is still available as `ESP_EXT_PART_ALIGN_POLICY_KEEP_SIZE`, which is no
  longer value 0.
- **Breaking:** `esp_mbr_parse`, `esp_mbr_bdl_read` and `esp_ext_part_probe` return
  `ESP_ERR_NOT_FOUND` when a partition entry's status byte is not `0x00` or `0x80`.
  Previously a filesystem boot sector on a medium without a partition table (which
  also ends in `0x55AA`) was parsed as an MBR, producing partitions from boot code.
- **Breaking:** `esp_mbr_generate`/`esp_mbr_partition_set` return
  `ESP_ERR_NOT_SUPPORTED` for a partition whose type maps to MBR type `0x00`, and
  `ESP_ERR_INVALID_SIZE` for a partition with size 0. Previously the first wrote an
  unused slot (the partition silently disappeared) and the second wrote a zero-length
  entry.
- **Breaking:** the LittleFS block size (`extra` of a type `0xC3` partition, stored in
  the CHS-start bytes) must be 0 or a power of two from 128 B to 1 MiB.
  `esp_mbr_generate`/`esp_mbr_partition_set` return `ESP_ERR_INVALID_SIZE` for other
  values; previously values above 24 bits were silently truncated. `esp_mbr_parse`
  sets `extra` and `ESP_EXT_PART_FLAG_EXTRA` only for such a value; previously it set
  the flag for every `0xC3` entry, so real CHS bytes written by other tools were
  reported as a block size. `esp_ext_part_match_mountable()` now requires a block
  size for LittleFS. These functions no longer return `ESP_ERR_INVALID_STATE` for a
  LittleFS partition without a block size.
- Minimum supported ESP-IDF version raised from 5.1 to 5.2, which is the oldest version
  actually covered by CI.

### Fixed

- `esp_mbr_parse` reads all four partition table slots. Previously it stopped at the
  first unused slot, on the wrong assumption that MBR entries must be consecutive, so
  every partition after an empty slot (as left by deleting a partition with `fdisk` or
  Windows) was silently dropped without `ESP_EXT_PART_LIST_FLAG_LOSSY` being set.
- `esp_mbr_bdl_read`, `esp_mbr_bdl_write` and `esp_ext_part_probe` now work on block
  devices whose read, write or erase granularity is larger than 512 B, and
  `esp_mbr_bdl_write` erases before writing on devices that need it. Previously they
  always transferred exactly 512 bytes and never erased.
- `esp_mbr_bdl_write` preserves the bootstrap code and any other data in the first
  I/O unit (read-modify-write). Previously it wrote a zeroed sector.
- A table containing a `0xC3` partition without a usable block size can be
  regenerated. Previously `esp_mbr_generate` failed for the whole table, so no other
  partition on such a disk could be changed.
- CHS values for LBAs beyond the CHS range are now the conventional maximum
  `FE FF FF` (C/H/S 1023/254/63). Previously only the cylinder was clamped, leaving
  arbitrary head and sector values.
- `esp_mbr_generate` no longer leaves stale data behind when generating into a buffer
  that already holds an MBR (the read-modify-write case that `keep_signature` exists
  for). Partition table entries not covered by the list are now zeroed instead of being
  left in place, where they would reappear on the next parse; and each written entry is
  fully rewritten, so a stale `status` (bootable) byte or stale CHS bytes cannot survive
  from a previous partition. The bootstrap code area is still preserved.
- The MBR copy-protection marker now follows `ESP_EXT_PART_LIST_FLAG_READ_ONLY` in both
  directions. It was previously only ever set, never cleared, so a list without the flag
  could still produce a copy-protected MBR.
- `esp_mbr_generate` leaves the caller's buffer unmodified on any error. It now builds
  the MBR in a temporary copy and commits it only after the whole layout is validated;
  previously an error could leave a mix of new and stale entries in the buffer.
- `esp_mbr_parse` leaves the list empty on any error instead of returning a partially
  filled list.
- `esp_mbr_partition_set` no longer leaves a partially written entry behind when it
  returns an error; the entry is only committed once every check has passed.
- `esp_mbr_generate` / `esp_mbr_partition_set` reject a partition starting at sector 0,
  which would overwrite the MBR itself (`ESP_ERR_INVALID_ARG`).
- `esp_mbr_generate` / `esp_mbr_partition_set` reject an explicit `info.address` that is
  not a multiple of the sector size (`ESP_ERR_INVALID_ARG`). Previously it was silently
  rounded up, which moved the partition.
- `esp_mbr_parse` skips used entries with zero sectors or a start at sector 0 and marks
  the list `ESP_EXT_PART_LIST_FLAG_LOSSY`, so a parsed list can always be regenerated.
- Added the missing `esp_hw_support` requirement (for `esp_random()`) and the missing
  `<stdlib.h>` include, both of which previously worked only through transitive
  dependencies. `log` moved to `PRIV_REQUIRES` and a stray `freertos/FreeRTOS.h`
  include was removed.

### Added

- `esp_ext_part_t.slot` records the 1-based table slot a partition was parsed from, and
  `esp_mbr_generate_extra_args_t.preserve_slots` makes `esp_mbr_generate` write
  partitions back to those slots, keeping partition numbering across a
  parse-modify-generate cycle. Without it, partitions are still written to consecutive
  slots (compacted).
- `esp_ext_part_probe()` reports which partition table format a block device carries,
  so a caller that does not know the medium can choose a parser instead of guessing.
  Detection uses the fact that the first sector is always an MBR: a `0xEE` entry means
  the disk is GPT. `ESP_EXT_PART_LIST_SIGNATURE_GPT` was added for that result; GPT is
  detected only, it cannot be parsed or generated by this component.
- Documented that a GPT disk parses as a single `ESP_EXT_PART_TYPE_GPT_PROTECTIVE_MBR`
  partition rather than failing, so callers that may see GPT media know to check for it.
- Documented the `total_size` auto-fill behaviour of `esp_mbr_bdl_write`.
- Component tests now also run on target under QEMU (esp32s3, esp32c3) in addition to
  the linux host build.

## Earlier versions

Releases before 1.0.0 were not tracked in this file.
