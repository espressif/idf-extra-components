# `esp_nand_ubi` Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** A UBI-like BDL middleware component for `idf-extra-components` that wraps the raw NAND flash BDL, hides bad blocks, provides logical erase blocks, and enables position-independent factory images on SPI NAND flash.

**Architecture:** `nand_flash_get_blockdev()` (raw physical) → `nand_ubi_attach()` + `nand_ubi_open_volume()` (logical LEBs per volume) → LittleFS / FAT. The UBI layer scans per-PEB on-flash EC+VID headers at attach, rebuilds a `lnum→pnum` EBA table in RAM, and exposes a flat LEB address space per volume to the filesystem above. Position-independence is guaranteed because LEB numbers (not physical PEB numbers) are the only addresses the filesystem ever sees. `esp_partition` is not involved with NAND — UBI volumes are the partitioning scheme for NAND.

**Tech Stack:** C99, ESP-IDF ≥ 6.0, `esp_blockdev` BDL interface, `esp_nand_blockdev.h` NAND ioctl extensions, FreeRTOS (Phase 3 WL task), Python 3 (host tool). Component lives in `~/esp/forks/idf-extra-components/esp_nand_ubi/`.

---

## Resolved Design Decisions

These close the open questions from `esp-nand-ubi-viability.md` §7:

| Question | Decision | Rationale |
|----------|----------|-----------|
| Single vs multi-volume | **UBI is the partitioning scheme for NAND; `esp_partition` not involved** | Phase 1: single volume (whole chip = one LEB space). Phase 3: multi-volume, each volume is its own BDL handle — UBI volumes replace `esp_partition` entries for NAND |
| `vol_id` in VID header | **Yes, always 0x00000000** | Keeps byte-level Linux `ubinize` compatibility |
| Layout volume / vtbl | **Omit in Phase 1** | Only needed for multi-volume; deferred |
| Header placement | **In-band (first 2 pages of each PEB)** | No OOB API complexity; portable across all NAND types |
| `is_bad` ioctl | **Use existing `ESP_BLOCKDEV_CMD_IS_BAD_BLOCK`** | Already implemented in `esp_nand_blockdev.h` — no new ioctl needed |
| Linux `ubinize` compat | **Yes — keep UBI magic numbers** | Reuse `mtd-utils ubinize` on host; also write a standalone `esp_ubinize.py` |
| WL | **Phase 1: passive only (lowest-EC free PEB)** | Phase 3: background FreeRTOS task |
| Fastmap | **Omit in Phase 1** | Full scan ~1–2 s for 2048 blocks; acceptable for a PoC; Phase 3 optional |

---

## RAM Optimization Strategy

> **Key user constraint:** minimize internal SRAM — users need it for their applications.

### The central insight: no permanent PEB copy buffer

The viability doc estimated 128 KB for a `peb_buf`. That was assuming a permanent scratch buffer. We avoid it:

- **Attach scan**: reads only 2 pages per PEB (EC header at page 0, VID header at page 1). A single page-sized allocation (~2 KB typical) is sufficient and is freed immediately after attach.
- **Normal read/write**: translate LEB → PEB address, forward the call directly to `nand_bdl`. No copy buffer needed — the caller's buffer is used in-place.
- **Normal erase**: erase the physical PEB, write 64-byte EC header (update erase counter), add PEB to free pool. No full-block buffer needed.
- **WL moves (Phase 3 only)**: allocate one PEB-sized buffer, copy, free. Triggered only by the background WL task.

### RAM resident while mounted

| Structure | Size (1G NAND, 1024 blocks) | Allocation strategy |
|-----------|---:|---|
| `nand_ubi_device_t` (device control) | ~280 B | `malloc()` → internal SRAM |
| EBA table `eba[leb_count]` | ≤ 4 KB | `heap_caps_malloc_prefer(SPIRAM, …)` |
| PEB state bitmap `peb_state[ceil(peb_count/8)]` | 128 B | same |
| EC table `ec[peb_count]` (Phase 3) | 4 KB | same; Kconfig-gated |
| `nand_ubi_vol_ctx_t` per open volume | ~40 B | `malloc()` → internal SRAM |
| **Total (no PSRAM, no WL)** | **~4.5 KB** | internal SRAM |
| **Total (PSRAM, no WL)** | **~4.5 KB PSRAM + 400 B SRAM** | |

For a 256-block NAND: ~1.3 KB total. This is the correct minimal footprint — roughly 30× less than the viability doc's initial estimate.

### Allocation helper

```c
// Try PSRAM first, fall back to internal heap — never fail silently
static void *ubi_alloc(size_t size)
{
#if CONFIG_SPIRAM
    void *p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p) return p;
#endif
    return malloc(size);   // fall back to internal RAM
}
```

---

## Stack Layout After This Component

`esp_partition` is **not involved with NAND at all** in either use case. It was designed for NOR flash (fixed physical offsets, no bad blocks) and cannot be placed below UBI on raw NAND without breaking position-independence at the partition-table level.

**Use case 1 — NAND as external filesystem only (Phase 1 PoC)**

```
SPI NAND hardware
    ↓
nand_flash_get_blockdev(&spi_cfg, &nand_bdl)        [spi_nand_flash]
    ↓
nand_ubi_attach(nand_bdl, &cfg, &ubi_dev)           [NEW: esp_nand_ubi — scan once]
nand_ubi_open_volume(ubi_dev, 0, &vol_bdl)          [whole chip = one volume]
    ↓   geometry: disk_size = leb_count × LEB_SIZE, erase_size = LEB_SIZE
    ↓   bad blocks invisible; no physical addresses above this point
LittleFS / FatFS via vol_bdl
```

**Use case 2 — NAND holds multiple independent regions (Phase 3 multi-volume)**

```
SPI NAND hardware
    ↓
nand_flash_get_blockdev(&spi_cfg, &nand_bdl)
    ↓
nand_ubi_attach(nand_bdl, &cfg, &ubi_dev)           [scan once]
    ├── nand_ubi_open_volume(ubi_dev, 0, &ota0_bdl)  vol "ota_0" — static firmware
    ├── nand_ubi_open_volume(ubi_dev, 1, &ota1_bdl)  vol "ota_1" — OTA slot
    └── nand_ubi_open_volume(ubi_dev, 2, &fs_bdl)    vol "fs"    — LittleFS, autoresize
```

UBI volumes replace `esp_partition` entries for NAND. Each volume is an independent `esp_blockdev_handle_t`. The vtbl (Phase 3) is stored in LEBs with VID headers — fully position-independent, no fixed physical offsets.

**Do not insert `spi_nand_flash_wl_get_blockdev()` (Dhara) in this chain.** The `esp-nand-ubi-vs-dhara.md` analysis covers why: double FTL, incompatible geometry contracts, double write amplification.

---

## Component Layout

```
idf-extra-components/esp_nand_ubi/
├── CMakeLists.txt
├── idf_component.yml
├── Kconfig
├── README.md
├── include/
│   └── esp_nand_ubi.h          ← public API (single header)
├── src/
│   ├── nand_ubi.c              ← attach/detach, open_volume, convenience wrapper, BDL ops vtable
│   ├── nand_ubi_priv.h         ← private: nand_ubi_device_t and nand_ubi_vol_ctx_t definitions
│   ├── nand_ubi_media.h        ← private: EC/VID header structs + magic numbers
│   ├── nand_ubi_eba.c          ← EBA table + PEB state bitmap
│   ├── nand_ubi_eba.h          ← private: EBA interface
│   ├── nand_ubi_io.c           ← header read/write, CRC32
│   └── nand_ubi_io.h           ← private: I/O interface
├── host_test/                  ← Linux-target unit tests (memory BDL backend)
│   ├── CMakeLists.txt
│   └── main/
│       ├── CMakeLists.txt
│       └── nand_ubi_test.c
└── tools/
    └── esp_ubinize.py          ← host tool: FS binary → .ubi flat image
```

---

## Public API (`include/esp_nand_ubi.h`)

The API is split into device-level (attach/detach — happens once per physical chip) and volume-level (open/close — once per logical region). A convenience wrapper covers the single-volume common case.

```c
/** Configuration passed at attach time */
typedef struct {
    uint32_t reserved_pebs;   /**< spare PEBs held back for bad-block pool (default 4) */
    bool     read_only;       /**< attach without modifying flash (image inspection) */
} nand_ubi_config_t;

#define NAND_UBI_CONFIG_DEFAULT() { .reserved_pebs = 4, .read_only = false }

/** Opaque handle representing an attached UBI device (one physical NAND chip) */
typedef struct nand_ubi_device nand_ubi_device_t;

/* ── device-level ──────────────────────────────────────────────────────── */

/**
 * @brief Scan all PEBs and build the EBA table for a raw NAND BDL.
 *
 * One call per physical device, regardless of volume count.
 * Reads only EC+VID headers (2 pages per PEB); page_buf is freed after scan.
 *
 * @param nand_bdl     Raw flash BDL from nand_flash_get_blockdev().
 *                     Must NOT be the Dhara WL BDL.
 * @param config       Attach config (NULL → defaults).
 * @param out_ubi_dev  Output: opaque device handle.
 * @return ESP_OK or error code.
 */
esp_err_t nand_ubi_attach(esp_blockdev_handle_t   nand_bdl,
                           const nand_ubi_config_t *config,
                           nand_ubi_device_t      **out_ubi_dev);

/**
 * @brief Release all resources held by a UBI device.
 *
 * All volume BDL handles opened from this device must be released first.
 * Does NOT release the underlying nand_bdl.
 */
esp_err_t nand_ubi_detach(nand_ubi_device_t *ubi_dev);

/* ── volume-level ──────────────────────────────────────────────────────── */

/**
 * @brief Open one volume and return a BDL handle scoped to it.
 *
 * Phase 1: only vol_id=0 is valid (whole chip = one volume, no vtbl).
 * Phase 3: any vol_id present in the vtbl.
 *
 * The returned handle must be released via vol_bdl->ops->release(vol_bdl)
 * before nand_ubi_detach() is called.
 *
 * @param ubi_dev      Device handle from nand_ubi_attach().
 * @param vol_id       Volume ID (0 in Phase 1; any vtbl vol_id in Phase 3).
 * @param out_vol_bdl  Output: BDL handle for this volume.
 * @return ESP_OK or ESP_ERR_NOT_FOUND if vol_id does not exist.
 */
esp_err_t nand_ubi_open_volume(nand_ubi_device_t     *ubi_dev,
                                uint32_t               vol_id,
                                esp_blockdev_handle_t *out_vol_bdl);

/* ── convenience wrapper (single-volume common case) ───────────────────── */

/**
 * @brief attach + open_volume(0) in one call.
 *
 * Equivalent to nand_ubi_attach() followed by nand_ubi_open_volume(0).
 * The nand_ubi_device_t lifecycle is hidden inside the BDL; calling
 * vol_bdl->ops->release(vol_bdl) also detaches the device.
 *
 * Use this when the whole NAND is one filesystem and no multi-volume
 * management is needed.
 */
esp_err_t nand_ubi_get_blockdev(esp_blockdev_handle_t   nand_bdl,
                                 const nand_ubi_config_t *config,
                                 esp_blockdev_handle_t  *out_vol_bdl);
```

The BDL ops exposed by a volume handle (`out_vol_bdl`):

| Op | Semantics |
|----|-----------|
| `read(dev, dst, dst_sz, leb_byte_addr, len)` | Translate to `pnum×PEB_SIZE + data_offset + offset`, forward to `nand_bdl` |
| `write(dev, src, leb_byte_addr, len)` | Translate + allocate free PEB if LEB unmapped, write VID header, forward data write |
| `erase(dev, leb_byte_addr, LEB_SIZE)` | Erase physical PEB, increment `ec[]`, write EC header, return PEB to free pool |
| `sync(dev)` | Forward to `nand_bdl` |
| `ioctl(dev, cmd, args)` | Forward to `nand_bdl` (passthrough, inc. NAND-specific ioctls) |
| `release(dev)` | Free volume wrapper; if opened via `nand_ubi_get_blockdev()` also detaches device |

---

## On-Flash Format (`src/nand_ubi_media.h`)

Two 64-byte headers per PEB, both at the start of the block:

```
PEB layout:
┌──────────────────────────────┐ ← PEB offset 0 (page 0, offset 0)
│  EC header  (64 B)           │   magic 0x55424923 "UBI#"
│  image_seq, ec, offsets, CRC │
├──────────────────────────────┤ ← vid_hdr_offset = page_size (e.g. 2048 B)
│  VID header (64 B)           │   magic 0x55424921 "UBI!"
│  vol_id=0, lnum, sqnum, CRC  │
├──────────────────────────────┤ ← data_offset = 2×page_size (e.g. 4096 B)
│  LEB data                    │   filesystem payload; LEB_SIZE = PEB_SIZE − data_offset
│  (e.g. 131072 − 4096 bytes)  │
└──────────────────────────────┘
```

Field-level compatibility with Linux UBI EC/VID headers:
- `magic`, `version`, `ec`, `vid_hdr_offset`, `data_offset`, `image_seq` → byte-for-byte identical to Linux `struct ubi_ec_hdr`
- `magic`, `version`, `vol_type`, `copy_flag`, `vol_id` (= 0), `lnum`, `sqnum`, `data_size`, `data_crc`, `hdr_crc` → byte-for-byte identical to Linux `struct ubi_vid_hdr`
- **Result**: images built with `ubinize` from `mtd-utils` work on this layer. `esp_ubinize.py` is provided as a simpler alternative for users without `mtd-utils`.

### Bad-block detection during attach

```c
esp_blockdev_cmd_arg_status_t arg = { .num = pnum, .status = false };
bdl->ops->ioctl(nand_bdl, ESP_BLOCKDEV_CMD_IS_BAD_BLOCK, &arg);
if (arg.status) { /* bad — skip */ }
```

No new ioctl needed — `ESP_BLOCKDEV_CMD_IS_BAD_BLOCK` is already implemented in `nand_flash_blockdev.c`.

---

## Attach Algorithm (low-RAM variant)

```
nand_ubi_attach(nand_bdl, config):
  peb_count  = nand_bdl.geometry.disk_size / nand_bdl.geometry.erase_size
  page_size  = nand_bdl.geometry.read_size      // == write_size for NAND
  peb_size   = nand_bdl.geometry.erase_size

  alloc eba[peb_count]  (int32_t, init to UBI_LEB_UNMAPPED = -1)
  alloc peb_state[ceil(peb_count/8)]  (uint8_t bitmap, init to 0)
  alloc page_buf[page_size]  ← TEMPORARY, on heap, freed after attach

  image_seq = 0;  valid_image_seq = false
  max_lnum  = -1;  sqnum_table[peb_count] = 0  ← or use per-eba sqnum field

  for pnum = 0 .. peb_count-1:
    IS_BAD_BLOCK(pnum) → if bad: mark peb_state[pnum] as BAD; continue

    // Read EC header (page 0)
    nand_bdl.read(pnum × peb_size, page_buf, page_size)
    ec_hdr = (struct ec_hdr *)page_buf
    if all-0xFF:  mark peb_state[pnum] as FREE; continue
    if CRC32(ec_hdr) bad OR magic wrong:  schedule_erase(pnum); continue
    if !valid_image_seq:  image_seq = ec_hdr.image_seq; valid_image_seq = true
    else if ec_hdr.image_seq != image_seq:  log warning, treat as stale
    vid_off = ec_hdr.vid_hdr_offset   // validated: must be page_size or 2×page_size

    // Read VID header (page 1 or 2, at vid_off)
    nand_bdl.read(pnum × peb_size + vid_off, page_buf, page_size)
    vid_hdr = (struct vid_hdr *)page_buf
    if all-0xFF:  mark peb_state[pnum] as FREE; continue
    if CRC32(vid_hdr) bad:  schedule_erase(pnum); continue

    lnum  = vid_hdr.lnum
    sqnum = vid_hdr.sqnum

    if eba[lnum] == UBI_LEB_UNMAPPED:
      eba[lnum] = pnum; sqnum_seen[lnum] = sqnum
      mark peb_state[pnum] as USED
    else:
      old_pnum = eba[lnum]
      if sqnum > sqnum_seen[lnum]:   // keep newer
        if vid_hdr.copy_flag:  verify data_crc before accepting
        mark peb_state[old_pnum] as FREE (schedule erase)
        eba[lnum] = pnum; sqnum_seen[lnum] = sqnum
        mark peb_state[pnum] as USED
      else:
        mark peb_state[pnum] as FREE (schedule erase)

    max_lnum = max(max_lnum, lnum)

  leb_count = max_lnum + 1
  free page_buf
  free sqnum_seen[]  ← only needed during attach, not resident
  // eba[] and peb_state[] remain resident
```

> **Note on `sqnum_seen[]`**: this temporary table maps `lnum → last_seen_sqnum` to handle duplicate PEBs. Size: `leb_count × 8 bytes` (uint64_t). For 1024 LEBs: 8 KB, freed after attach. Can be optimized to a flat array of structs if memory is very tight, but 8 KB is acceptable even without PSRAM.

---

## Kconfig Options (`Kconfig`)

```kconfig
menu "NAND UBI configuration"

config ESP_NAND_UBI_ENABLE
    bool "Enable UBI-like layer for SPI NAND flash"
    depends on NAND_FLASH_ENABLE_BDL
    default n
    help
        Enables the esp_nand_ubi component. Requires CONFIG_NAND_FLASH_ENABLE_BDL
        and ESP-IDF >= 6.0. Provides position-independent factory images on SPI NAND.

config ESP_NAND_UBI_RESERVED_PEBS
    int "Default number of reserved PEBs for bad-block pool"
    depends on ESP_NAND_UBI_ENABLE
    range 0 32
    default 4

config ESP_NAND_UBI_WL_ENABLE
    bool "Enable background wear-leveling FreeRTOS task (Phase 3)"
    depends on ESP_NAND_UBI_ENABLE
    default n

config ESP_NAND_UBI_WL_THRESHOLD
    int "Erase-count difference threshold to trigger a WL move"
    depends on ESP_NAND_UBI_WL_ENABLE
    default 4096

endmenu
```

---

## CMakeLists.txt Pattern

```cmake
set(srcs "src/nand_ubi.c"
         "src/nand_ubi_eba.c"
         "src/nand_ubi_io.c")

if(CONFIG_ESP_NAND_UBI_WL_ENABLE)
    list(APPEND srcs "src/nand_ubi_wl.c")
endif()

idf_component_register(
    SRCS            ${srcs}
    INCLUDE_DIRS    include
    PRIV_INCLUDE_DIRS src
    REQUIRES        esp_blockdev spi_nand_flash
    PRIV_REQUIRES   esp_log esp_heap_caps)
```

---

## `esp_ubinize.py` Host Tool

Takes a flat FS image (e.g. LittleFS binary from `mklittlefs`) and NAND geometry parameters, emits a flat `.ubi` binary that a UBI-aware flasher can write to any chip of the same geometry.

```
esp_ubinize.py \
  --peb-size 131072  \     # physical erase block size in bytes
  --page-size 2048   \     # page size (determines vid_hdr_offset and data_offset)
  --image-seq 0xABCD1234 \ # random token; all PEBs share this
  --vol-type dynamic \
  --autoresize \           # leave the autoresize marker (future use)
  input.lfs \              # raw LittleFS binary
  output.ubi               # flat binary of (peb_count × peb_size) bytes
```

Output format: sequential PEB-sized chunks, each containing:
1. EC header (ec=0, same image_seq, vid_hdr_offset, data_offset)
2. VID header (vol_id=0, lnum=N, sqnum=N+1, vol_type=dynamic)
3. LEB data (slice of input FS image, zero-padded at end)

**No physical addresses** appear in the output. A UBI-aware flasher iterates the target chip's non-bad physical PEBs and writes image PEBs in order.

---

## Phased Implementation Plan

### Phase 1 — Minimum Viable Layer (PoC target)

**Deliverable**: A LittleFS image built on Chip A mounts correctly on Chip B with a different factory bad-block distribution.

**Task 1: Component skeleton** ✅ DONE (cb39892)
- Files: `esp_nand_ubi/CMakeLists.txt`, `idf_component.yml`, `Kconfig`, `README.md`, `include/esp_nand_ubi.h`
- `esp_nand_ubi.h` declares `nand_ubi_config_t`, `nand_ubi_device_t` (opaque typedef), `nand_ubi_attach()`, `nand_ubi_detach()`, `nand_ubi_open_volume()`, `nand_ubi_get_blockdev()` (signatures only)
- CMakeLists.txt registers `REQUIRES esp_blockdev spi_nand_flash`
- Kconfig adds `CONFIG_ESP_NAND_UBI_ENABLE` gated on `NAND_FLASH_ENABLE_BDL`
- Verify: `idf.py build` on a minimal app that includes the component (no sources yet = empty lib is fine)

**Task 2: On-flash media structs and CRC** ✅ DONE (cb39892)
- File: `src/nand_ubi_media.h`
- Define `nand_ubi_ec_hdr_t` and `nand_ubi_vid_hdr_t` as `__attribute__((packed))` structs
- Magic constants: `UBI_EC_HDR_MAGIC 0x55424923`, `UBI_VID_HDR_MAGIC 0x55424921`
- `UBI_LEB_UNMAPPED (int32_t)(-1)`, `UBI_FREE_LEB (uint64_t)(~0ULL)`
- File: `src/nand_ubi_io.c` / `nand_ubi_io.h`
- Implement `uint32_t nand_ubi_crc32(const void *buf, size_t len)` — use `esp_rom_crc32_le()` on device, stdlib fallback for host
- Implement `bool nand_ubi_ec_hdr_valid(const nand_ubi_ec_hdr_t *h)`
- Implement `bool nand_ubi_vid_hdr_valid(const nand_ubi_vid_hdr_t *h)`
- Tests: unit-test CRC, magic, and invalid-header detection

**Task 3: EBA table and PEB state bitmap** ✅ DONE
- File: `src/nand_ubi_eba.c` / `nand_ubi_eba.h`
- `nand_ubi_eba_t` struct: `int32_t *eba`, `uint8_t *peb_state`, counts
- PEB state bits: `UBI_PEB_FREE`, `UBI_PEB_USED`, `UBI_PEB_BAD`, `UBI_PEB_ERASE_PENDING`
- `nand_ubi_eba_alloc(uint32_t peb_count, uint32_t leb_count)` → allocates using `ubi_alloc()`
- `nand_ubi_eba_free(nand_ubi_eba_t *eba)`
- `nand_ubi_eba_get_pnum(eba, lnum)` → `int32_t`
- `nand_ubi_eba_set(eba, lnum, pnum)`
- `nand_ubi_eba_peb_is_free(eba, pnum)` / `_set_free()` / `_set_used()` / `_set_bad()`
- `nand_ubi_eba_find_free_peb(eba, peb_count)` → linear scan, returns first free pnum (Phase 1: no EC-based selection needed)
- Tests: alloc/free, set/get, find_free_peb with injected bad blocks

**Task 4: Attach — scan + EBA build**
- File: `src/nand_ubi.c`
- Implement `nand_ubi_attach(nand_bdl, config, out_ubi_dev)`:
  - Allocate `page_buf` (size = `nand_bdl.geometry.read_size`)
  - Loop over all PEBs: `IS_BAD_BLOCK` ioctl → EC header read → VID header read → EBA update
  - Duplicate resolution by `sqnum`
  - `copy_flag` → verify `data_crc` before accepting (read LEB data into temp buf, compute CRC)
  - Free `page_buf` and `sqnum_seen[]` after scan
  - Return populated `nand_ubi_device_t *` — does NOT return a BDL handle
- Internal `nand_ubi_device_t` struct (private, defined in `src/nand_ubi_priv.h`, not in public header):
  ```c
  /* Device-level: one per physical NAND chip, owns the scan results */
  struct nand_ubi_device {
      esp_blockdev_handle_t nand_bdl;
      uint32_t  peb_count;
      uint32_t  peb_size, page_size;
      uint32_t  vid_hdr_offset, data_offset;
      uint32_t  leb_size;          // = peb_size - data_offset
      uint32_t  image_seq;
      uint64_t  global_sqnum;      // monotonically increasing; max(sqnum seen) at attach
      nand_ubi_eba_t eba;          // owns eba[] and peb_state[] arrays
      SemaphoreHandle_t lock;
      /* Phase 3: uint32_t vol_count; nand_ubi_vol_t *volumes[]; */
  };

  /* Volume-level: one per open volume, wraps device + vol_id into a BDL */
  typedef struct {
      nand_ubi_device_t *dev;
      uint32_t           vol_id;
      uint32_t           leb_count;   // number of LEBs in this volume
      bool               owns_device; // true when opened via nand_ubi_get_blockdev()
  } nand_ubi_vol_ctx_t;
  ```

**Task 5: Volume open and ops vtable**
- Implement `nand_ubi_open_volume(ubi_dev, vol_id, out_vol_bdl)`:
  - Phase 1: assert `vol_id == 0`; `leb_count = max_lnum + 1` from the attach scan
  - Allocate `nand_ubi_vol_ctx_t`, populate `dev`, `vol_id`, `leb_count`, `owns_device = false`
  - Allocate `esp_blockdev_t`, fill geometry and flags, set ops pointer to `s_nand_ubi_vol_ops`
- Implement `nand_ubi_get_blockdev(nand_bdl, config, out_vol_bdl)`:
  - `nand_ubi_attach(nand_bdl, config, &dev)`
  - `nand_ubi_open_volume(dev, 0, out_vol_bdl)`
  - Set `vol_ctx->owns_device = true` so `release()` also calls `nand_ubi_detach()`
- Geometry filled in volume BDL:
  - `disk_size   = leb_count × leb_size`
  - `read_size   = nand_bdl.geometry.read_size`
  - `write_size  = nand_bdl.geometry.write_size`
  - `erase_size  = leb_size  // = peb_size - data_offset`
- Flags: copy `erase_before_write=1`, `and_type_write=1` from `nand_bdl`
- Static ops: `s_nand_ubi_vol_ops = { .read = ..., .write = ..., .erase = ..., .sync = ..., .ioctl = ..., .release = ... }`

**Task 6: BDL read / write / erase**
- `nand_ubi_read(dev, dst, dst_sz, leb_byte_addr, len)`:
  - `lnum = leb_byte_addr / dev->leb_size`; `offset = leb_byte_addr % dev->leb_size`
  - `pnum = eba_get_pnum(&ctx->dev->eba, lnum)`; return `ESP_ERR_NOT_FOUND` if unmapped
  - Forward: `nand_bdl.read(pnum × peb_size + data_offset + offset, dst, dst_sz, len)`
- `nand_ubi_write(dev, src, leb_byte_addr, len)`:
  - `lnum`, `offset` from address (using `ctx->dev->leb_size`)
  - If `eba[lnum] == unmapped`: allocate free PEB, write EC header (ec=0 for new allocs), write VID header (vol_id=0, lnum, ++global_sqnum)
  - Forward data write to correct physical offset
- `nand_ubi_erase(dev, leb_byte_addr, LEB_SIZE)`:
  - `lnum` from address; get `pnum` from `ctx->dev->eba`
  - `nand_bdl.erase(pnum × peb_size, peb_size)`
  - Increment `ec[pnum]` if EC table present; write updated EC header
  - `eba[lnum] = unmapped`; `peb_state[pnum] = FREE`

**Task 7: Host tests**
- Location: `host_test/main/nand_ubi_test.c`
- Use `esp_blockdev_util/memory` BDL for a fake NAND (or roll a minimal stub if that component isn't available in host test)
- Test cases:
  1. Format (ubinize) + attach: EBA table correct
  2. Read/write/erase round-trip through UBI layer
  3. Injected factory bad blocks: attach skips them, LEB numbering contiguous
  4. Duplicate PEB (simulated power-cut mid-write): attach resolves by sqnum
  5. `copy_flag` set on the newer PEB, data_crc mismatch: attach falls back to older PEB
  6. Release: all memory freed (asan check)

---

### Phase 2 — Tooling

**Task 8: `esp_ubinize.py`**
- Location: `esp_nand_ubi/tools/esp_ubinize.py`
- Arguments: `--peb-size`, `--page-size`, `--image-seq` (random default), `--vol-type`, `--autoresize`, `input_image`, `output_ubi`
- Algorithm:
  1. Slice `input_image` into LEB-sized chunks
  2. For each chunk: construct EC header (ec=0, vid_hdr_offset=page_size, data_offset=2×page_size, image_seq), compute CRC; construct VID header (vol_id=0, lnum=N, sqnum=N+1), compute CRC
  3. Write: EC header (64B) + padding to page_size + VID header (64B) + padding to data_offset + LEB data
- Output size: `ceil(len(input) / leb_size) × peb_size` bytes
- Validation: read back output, parse headers, verify all CRCs

**Task 9: UBI-aware flasher extension**
- Standalone Python script `tools/ubi_flash.py` (or `esptool` extension if it gains NAND support):
  - Accepts `.ubi` file and target NAND geometry
  - Iterates non-bad physical PEBs on target (via `esptool` or direct SPI NAND flash commands)
  - Writes image PEBs sequentially, skipping target bad blocks
- For the PoC: can be a manual procedure documented in README (flash PEB-by-PEB via JTAG + `idf.py flash` for the UBI region)

---

### Phase 3 — Optional Production Polish (post-PoC validation)

**Task 10: Background WL FreeRTOS task** (gated on `CONFIG_ESP_NAND_UBI_WL_ENABLE`)
- Add `ec_table[peb_count]` (uint32_t array, PSRAM-preferred) to `nand_ubi_ctx_t`
- EC values loaded during attach from EC headers
- WL task: every N erases, find `pnum_hot = max(ec[used])`, `pnum_cold = min(ec[free])`
- If `ec[hot] - ec[cold] >= CONFIG_ESP_NAND_UBI_WL_THRESHOLD`: move LEB from hot to cold
  - Allocate PEB-sized buffer (PSRAM first), read LEB data, write to cold PEB with `copy_flag=1` and `data_crc`, update EBA, schedule hot for erase
- Task priority: `tskIDLE_PRIORITY + 1`; yields after each move
- Stack size: 4 KB

**Task 11: Simplified fastmap** (Kconfig-gated)
- Reserve PEB 0 for a fastmap: flat array of `{uint32_t lnum, uint32_t pnum}` pairs + CRC32 + generation counter
- Written on clean detach; invalidated (erased) on unexpected reset detected at next attach
- If fastmap CRC valid at attach: skip full scan, use fastmap directly
- Falls back to full scan on CRC failure or absence

**Task 12: Multi-volume support**
- The API (`nand_ubi_attach` / `nand_ubi_open_volume`) already supports this at the call site — no API changes needed
- Add `vol_id` filtering to the attach scan loop (Phase 1 accepts all `vol_id` values from VID headers but bins them all into the Phase 1 single-volume EBA; Phase 3 separates them)
- Add per-volume EBA lookup: `nand_ubi_device_t` grows a `vol_count` field and a small array of `{vol_id, leb_count, eba_offset}` entries
- Implement vtbl: layout volume `vol_id = 0x7FFFEFFF`, LEBs 0+1 (mirrored), contains `ubi_vtbl_record[128]`; load on attach, validate CRC per record
- `nand_ubi_open_volume(vol_id=N)` looks up vtbl entry, returns BDL scoped to that volume's LEB range
- `esp_ubinize.py` Phase 3 extension: `--vol-id`, `--vol-name`, multi-volume ini config (matches Linux `ubinize.ini` format)

---

## Open Questions for Review

1. **PoC target**: is it sufficient for Phase 1 to demonstrate host-test attach + read/write, or do you want a real ESP32-S3 + physical NAND chip as the PoC success criterion? (Affects whether we need Task 9 before calling Phase 1 done.)

2. **`data_offset` choice**: `data_offset = 2 × page_size` means 2 pages are consumed per PEB for headers, leaving `peb_size - 2×page_size` for LEB data. For a 128 KB PEB / 2 KB page: LEB_SIZE = 124 KB. This is 3% overhead. Is that acceptable, or should we try to pack EC+VID into a single page (both headers fit in 128 bytes, well within 2 KB)?  
   - Option A: 2 pages (safe, separate pages for each header, matches Linux UBI)  
   - Option B: 1 page (both headers in the same page, saves 2 KB/block = 2 MB on 1G NAND) — diverges slightly from Linux layout but still compatible if offsets are stored in EC header

3. **`esp_ubinize.py` vs reusing Linux `ubinize`**: since we keep byte-level compatibility with Linux UBI magic numbers, standard `ubinize` from `mtd-utils` works directly on a Linux host. Should we provide our own Python tool (simpler for users) **and** document `ubinize` compatibility? Or Python tool only?

4. **Branch strategy in `idf-extra-components`**: should I create the branch now (e.g. `feature/esp_nand_ubi`) so the PoC work has a clean home, or wait until the plan is approved?

5. **RAM allowance on no-PSRAM targets**: the ~4.5 KB EBA overhead for a 1 G NAND is the floor. Is that acceptable for ESP32 targets without PSRAM? If the target is always ESP32-S3 + PSRAM, we can be less strict about the allocation strategy.

---

*Sources: `esp-nand-ubi-viability.md`, `esp-nand-ubi-vs-dhara.md`, `esp_blockdev.h`, `esp_nand_blockdev.h`, `wl_blockdev.cpp`, `spi_nand_flash/CMakeLists.txt`*
