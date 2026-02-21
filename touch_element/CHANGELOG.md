## 1.1.2

- Fixed the read stuck issue after deep sleep by resetting the hardware while initializing

## 1.1.1

- Fixed the incorrect interrupt mask

## 1.1.0

- Refactor to remove the dependency on the hal and soc caps in IDF
- Moved the enums in `touch_pad_intr_mask_t` to ll header, but still keep `touch_pad_intr_mask_t` for backward compatibility

## 1.0.0

- Added test app, examples and documentations for the touch element library

## 0.1.0

- Initial touch element version, based on the legacy Touch Sensor driver
