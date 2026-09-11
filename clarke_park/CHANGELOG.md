# Changelog

## 0.1.0

- Initial version

### Added

- Clarke / inverse-Clarke transform (U/V/W <-> alpha/beta)
- Park / inverse-Park transform (alpha/beta <-> d/q)
- `float` backend (`clarke_park_*_f()`) and IQmath fixed-point backend
  (`clarke_park_*_iq()`), selectable per call through a single unified API
- `CONFIG_CLARKE_PARK_IQ_FORMAT` to select the Q-format of the fixed-point
  backend (default Q15)
- `CONFIG_CLARKE_PARK_USE_CORDIC_HW` to accelerate the fixed-point Park
  transform with the CORDIC hardware on supported chips
- `examples/benchmark`: cycle-count benchmark of every transform, including a
  software reference line to compare the CORDIC hardware path with the IQmath
  software implementation
