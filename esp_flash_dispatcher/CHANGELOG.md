## 1.0.1

- feat: Added `spi_flash_mmap` / `spi_flash_munmap` to the set of Flash APIs intercepted by the dispatcher, so memory-mapping can also be safely invoked from tasks whose stacks live in PSRAM.
- fix: Serialize dispatcher requests with an internal mutex so the shared request/result slot is always exclusively owned by a single caller, even when wrappers are called concurrently from different tasks.
- deprecate: `esp_flash_dispatcher_config_t::queue_size` is kept for backward compatibility only and no longer affects behavior; the dispatcher now uses a single shared slot regardless of the configured value.

## 1.0.0

- Initial release of `esp_flash_dispatcher`. Intercepts `esp_flash_read`, `esp_flash_write`, `esp_flash_write_encrypted`, `esp_flash_erase_region`, and `esp_flash_erase_chip`, and executes them on a dedicated background task whose stack lives in internal RAM, so application tasks can keep their stacks in PSRAM without breaking Flash operations.
