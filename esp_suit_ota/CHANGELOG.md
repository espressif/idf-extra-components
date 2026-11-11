## 0.1.0

### Features:
- First release: a SUIT manifest processor (draft-ietf-suit-manifest-38, updater role) for releases that update the app and data partitions together
- Native write path through esp_ota_begin, esp_ota_write and esp_ota_end for every flash target
- esp_ota_slots module: APP, AB, COPY and SINGLE pairs, groups that flip together, and a slots record in NVS
- Transport struct with one getter per link (HTTPS, BLE, MQTT, custom), selected by the URI scheme
- HTTPS getter with Range resume, Range chunking and ETag polling
- Stream mode for envelopes that carry their payloads, from any transport
- esp-suit tool: keys, identifiers, signed envelopes, verification and inspection
- Compliance tests for the draft (test_apps), on ESP32-C5 and ESP32-P4
