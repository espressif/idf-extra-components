| Supported Targets | Linux |
| ----------------- | ----- |

# Host Test for SPI NAND Flash Emulation

Linux host tests use the mmap-backed NAND emulator (`nand_linux_mmap_emul`). Full field semantics (including how OOB is interleaved in the backing file and how that affects usable capacity) are documented on `nand_file_mmap_emul_config_t` in [`nand_linux_mmap_emul.h`](../include/nand_linux_mmap_emul.h).

## NAND Flash Emulation Configuration

The NAND flash emulation can be configured using the `nand_file_mmap_emul_config_t` structure:

```c
#include "nand_linux_mmap_emul.h"

// Configuration structure for NAND emulation
nand_file_mmap_emul_config_t cfg = {
    .flash_file_name = "",                  // Empty string for temporary file, or specify path
    .flash_file_size = EMULATED_NAND_SIZE,  // Default is 128MB
    .keep_dump = true                       // true to keep file after tests
};
```

### Configuration Options:

1. **flash_file_name**:
   - Empty string ("") - Creates temporary file with pattern "/tmp/idf-nand-XXXXXX"
   - Custom path - Creates file at specified location
   - Maximum length: 256 characters

2. **flash_file_size**:
   - Default: `EMULATED_NAND_SIZE` (128MB)
   - Must be a multiple of the chip's **user-visible** erase-block size (`page_size * pages_per_block`). The on-disk image is larger per page because OOB bytes are interleaved after each page; see the struct documentation in `nand_linux_mmap_emul.h`.

3. **keep_dump**:
   - true: Keeps the memory-mapped file on disk after testing (for debugging or data persistence)
   - false: Removes the backing file on cleanup

### Usage Example:

#### Option 1: Direct Device API
```c
#include "nand_linux_mmap_emul.h"
#include "spi_nand_flash.h"

// Initialize with custom settings
nand_file_mmap_emul_config_t cfg = {
    .flash_file_name = "/tmp/my_nand.bin",
    .flash_file_size = 50 * 1024 * 1024,  // 50MB
    .keep_dump = false
};
spi_nand_flash_config_t nand_flash_config = {&cfg, 0, SPI_NAND_IO_MODE_SIO, 0};

// Initialize NAND flash with emulation
spi_nand_flash_device_t *handle;
spi_nand_flash_init_device(&nand_flash_config, &handle);

// Use direct NAND operations (page API preferred; sector API is also supported)
uint32_t page_size;
spi_nand_flash_get_page_size(handle, &page_size);
uint8_t *buffer = malloc(page_size);
uint32_t page_id = 0;
spi_nand_flash_read_page(handle, buffer, page_id);
spi_nand_flash_write_page(handle, buffer, page_id);

// Cleanup
spi_nand_flash_deinit_device(handle);
```

#### Option 2: Block Device API

Requires **ESP-IDF 6.0+** with **`CONFIG_NAND_FLASH_ENABLE_BDL`** enabled (same as on-target BDL tests).

```c
#include "nand_linux_mmap_emul.h"
#include "spi_nand_flash.h"
#include "esp_nand_blockdev.h"

// Initialize with block device interface
nand_file_mmap_emul_config_t cfg = {"", 50 * 1024 * 1024, false};
spi_nand_flash_config_t nand_flash_config = {&cfg, 0, SPI_NAND_IO_MODE_SIO, 0};

esp_blockdev_handle_t nand_bdl;

// Create Flash Block Device Layer
nand_flash_get_blockdev(&nand_flash_config, &nand_bdl);

// Use block device operations
uint32_t page_size = nand_bdl->geometry.read_size;  // BDL geometry uses page size
nand_bdl->ops->read(nand_bdl, buffer, page_size, offset, size);
nand_bdl->ops->write(nand_bdl, buffer, offset, size);

// Cleanup
nand_bdl->ops->release(nand_bdl);
```

## Building and running

From this directory (with ESP-IDF environment loaded):

```bash
idf.py --preview set-target linux
idf.py build monitor
```

Catch2-based suites are selected from `test_app_main.cpp` according to Kconfig (see the **Linux Host Testing** section in [`layered_architecture.md`](../layered_architecture.md)): legacy raw/device tests vs BDL-enabled sources such as `test_nand_flash_bdl.cpp` and `test_nand_flash_ftl.cpp`.
