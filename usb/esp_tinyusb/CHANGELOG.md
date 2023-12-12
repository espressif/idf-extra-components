## 1.4.3
- esp_tinyusb: Add AUDIO class driver configuration or tinyUSB

## 1.4.2

- MSC: Fix maximum files open
- Add uninstall function

## 1.4.0

- MSC: Fix integer overflows
- CDC-ACM: Remove intermediate RX ringbuffer
- CDC-ACM: Increase default FIFO size to 512 bytes
- CDC-ACM: Fix Virtual File System binding

## 1.3.0

- Add NCM extension

## 1.2.1 - 1.2.2

- Minor bugfixes

## 1.2.0

- Add MSC extension for accessing SPI Flash on memory card https://github.com/espressif/idf-extra-components/commit/a8c00d7707ba4ceeb0970c023d702c7768dba3dc

## 1.1.0

- Add support for NCM, ECM/RNDIS, DFU and Bluetooth TinyUSB drivers https://github.com/espressif/idf-extra-components/commit/79f35c9b047b583080f93a63310e2ee7d82ef17b

## 1.0.4

- Clean up string descriptors handling https://github.com/espressif/idf-extra-components/commit/046cc4b02f524d5c7e3e56480a473cfe844dc3d6

## 1.0.2 - 1.0.3

- Minor bugfixes

## 1.0.1

- CDC-ACM: Return ESP_OK if there is nothing to flush https://github.com/espressif/idf-extra-components/commit/388ff32eb09aa572d98c54cb355f1912ce42707c

## 1.0.0

- Initial version based on [esp-idf v4.4.3](https://github.com/espressif/esp-idf/tree/v4.4.3/components/tinyusb)
