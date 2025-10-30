| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- |

# sysview test

To build and run this test app for SystemView related tests:
```bash
IDF_TARGET=esp32 idf.py @sysview build flash monitor
```

`@sysview` argument apply additional `idf.py` options, from [sysview](sysview) file.
