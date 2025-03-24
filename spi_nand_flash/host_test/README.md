| Supported Targets | Linux |
| ----------------- | ----- |

# Host Test for SPI NAND Flash Emulation

## NAND Flash Emulation Configuration

The NAND flash emulation can be configured using the `nand_mmap_emul_config_t` structure:

```c
// Configuration structure for NAND emulation
nand_mmap_emul_config_t cfg = {
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
   - Default: EMULATED_NAND_SIZE (128MB)
   - Can be customized based on test requirements
   - Must be aligned to block size

3. **keep_dump**:
   - true: Removes the memory-mapped file after testing
   - false: Keeps the file for debugging or data persistence

### Usage Example:

```c
// Initialize with custom settings
nand_mmap_emul_config_t cfg = {
    .flash_file_name = "/tmp/my_nand.bin",
    .flash_file_size = 1024 * 1024,  // 1MB
    .keep_dump = false
};
spi_nand_flash_config_t nand_flash_config = {.emul_conf = &cfg};

// Initialize nand_flash with NAND emulation parameter
spi_nand_flash_device_t *handle;
spi_nand_flash_init_device(&nand_flash_config, &handle)

// Use NAND operations...

// Cleanup
ESP_ERROR_CHECK(spi_nand_flash_deinit_device(handle));
```
