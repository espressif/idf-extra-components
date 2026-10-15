# Changelog

## 0.3.0

- Added `clarke_park_enable_cordic()` and `clarke_park_disable_cordic()`: the application decides at run time
  whether the Q15 Park transform takes its sin/cos from the CORDIC hardware
  or from the IQmath software tables. The CORDIC engine is acquired as a shared
  reference while the CORDIC source is selected, and released when the software
  source is selected.
- Added `examples/benchmark/`: cost benchmark comparing the float backend and
  the Q15 backend on the software tables against the same Q15 code on the
  CORDIC hardware, both from a single binary.

## 0.2.0

- IQmath backend is now generic over the Q-format: `_iq8` through `_iq28`
  can be used simultaneously, each dispatching to its own backend.
- Removed `CONFIG_CLARKE_PARK_IQ_FORMAT` (Kconfig). The unsuffixed `_iq` API
  is an inline wrapper around the `_iqN` backend named by `GLOBAL_IQ` in the
  including translation unit.

## 0.1.0

- Initial version

### Added

- Clarke / inverse-Clarke transform (U/V/W <-> alpha/beta)
- Park / inverse-Park transform (alpha/beta <-> d/q)
- `float` backend (`clarke_park_*_f()`) and IQmath fixed-point backend
  (`clarke_park_*_iq()`), selectable per call through a single unified API
- `CONFIG_CLARKE_PARK_IQ_FORMAT` to select the Q-format of the fixed-point
  backend (default Q15)
