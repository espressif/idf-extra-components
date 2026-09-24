# SPI NAND FatFs Throughput Example

Testing-only example that measures FatFs throughput on SPI NAND flash using
the legacy `spi_nand_flash_init_device()` + `esp_vfs_fat_nand_*` path.

**It is destructive:** the whole chip is erased on every boot.

For raw page-level numbers without FatFs/Dhara, see
[`nand_flash_debug_app`](../nand_flash_debug_app).

## What it measures

After erase and format, four phases run on the same volume, each with chunk
sizes of 512 B, 4 KiB, 16 KiB and 32 KiB:

1. **stdio/buffered** — `fopen`/`fwrite`/`fread` with default libc buffering.
2. **POSIX sequential** — `open`/`write`/`read`.
3. **POSIX random** — `pwrite`/`pread` at shuffled chunk offsets.
4. **POSIX multi-file** — the same pattern split across several files.

Each `(phase, chunk)` cell is repeated `CONFIG_EXAMPLE_TEST_TRIES` times and
reports mean/min/max kB/s, physical operation counts, and write/read
amplification. Phases run back to back, so later phases see the wear and
garbage-collection state left by earlier ones.

If a cell runs out of FTL capacity (`ENOSPC`, or `EIO` caused by a Dhara
capacity error), the cell is logged as skipped and the run continues. Any
other error aborts the run.

## Comparing configurations

Besides the human-readable output, the example prints one `RUN {...}` JSON
line with the configuration and one `RESULT {...}` JSON line per cell.
`tools/compare_throughput_logs.py` reads these from captured logs.

1. Flash one configuration and capture the serial output. Leave the monitor
   with `Ctrl+]` once the run has finished:

   ```bash
   idf.py -p PORT flash monitor | tee baseline.log
   ```

   Use one file per run. Logs from firmware without the JSON lines are not
   supported.

2. Change the configuration (e.g. enable a cache in `idf.py menuconfig`) and
   capture again into another file, e.g. `path_cache.log`.

3. Compare, baseline first:

   ```bash
   python3 tools/compare_throughput_logs.py baseline.log path_cache.log [more.log ...] --output comparison.md
   ```

The report labels the logs `A` (baseline), `B`, `C`, ... It contains the
differing configuration, the settings shared by all logs, a summary
(geometric-mean throughput change, worst/best cell, write-pass physical
reads, median amplification), and per-cell tables for throughput,
amplification, and physical operation counts (reads, programs, copies,
erases, metadata cache hit rate, summed over all tries). Skipped and missing cells are shown as such and left out of the
summary. Log colour codes are ignored.

## Configuration

Example Kconfig:

| Option | Default | Effect |
|---|---|---|
| `EXAMPLE_TEST_TRIES` | 3 | Repeats per cell (1-5). |
| `EXAMPLE_PRECONDITION_ENABLE` | y | Write a fill file and keep it during the timed phases, so they run on a partly full FTL instead of a fresh chip. |
| `EXAMPLE_PRECONDITION_FILL_PERCENT` | 20 | Fill file size as percent of free space (10-80). |

`sdkconfig.defaults` enables `CONFIG_NAND_FLASH_PERF_STATS`, which the example
requires for the physical operation counts.

Driver caches (`spi_nand_flash` Kconfig), printed at boot and recorded in `RUN`:

- `CONFIG_DHARA_MAP_PATH_CACHE`
- `CONFIG_NAND_FLASH_PAGE_REGISTER_CACHE`
- `CONFIG_NAND_FLASH_DHARA_META_CACHE` (+ `CONFIG_NAND_FLASH_DHARA_META_CACHE_SLOTS`)

Compile-time `#define`s in `main/`: `EXAMPLE_FLASH_FREQ_KHZ`,
`EXAMPLE_IO_MODE`, `EXAMPLE_TEST_FILE_SIZE` (default 2 MB),
`EXAMPLE_ALLOC_UNIT_SIZE`, `EXAMPLE_MULTIFILE_COUNT`,
`EXAMPLE_MULTIFILE_FREE_PERCENT`.
