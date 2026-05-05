| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

## CI / pytest sdkconfig presets

On-target tests use `pytest_spi_nand_flash.py` with a **config** name that selects `sdkconfig.ci.<name>` (merged at build time by `idf-build-apps` / CI).

| Preset | File | Purpose |
|--------|------|---------|
| `default` | (project defaults) | Legacy path, `CONFIG_NAND_FLASH_EXPERIMENTAL_OOB_LAYOUT` off |
| `oob_layout` | `sdkconfig.ci.oob_layout` | Legacy path + experimental OOB layout **on** |
| `bdl` | `sdkconfig.ci.bdl` | BDL enabled |
| `bdl_oob_layout` | `sdkconfig.ci.bdl_oob_layout` | BDL + experimental OOB layout **on** |

**Local reproduction (after a full ESP-IDF export):** build and flash the app with the same sdkconfig preset the CI job uses, then run pytest with the matching `--config` / embedded-idf options your environment expects. Example build-only check from this directory:

```bash
idf.py set-target esp32
idf.py -DSDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.ci.oob_layout" build
```

(Exact `SDKCONFIG_DEFAULTS` merging may depend on your IDF version; CI uses `idf-build-apps` with `sdkconfig.ci.*` rules from the repo root.)
