# Changelog

## 0.2.0

- IQmath backend is now generic over the Q-format: `_iq8` through `_iq28`
  can be used simultaneously, each dispatching to its own backend.
- Removed `CONFIG_CLARKE_PARK_IQ_FORMAT` (Kconfig). The unsuffixed `_iq` API
  is an inline wrapper around the `_iqN` backend named by `GLOBAL_IQ` in the
  including translation unit

## 0.1.0

- Initial version

### Added

- Clarke / inverse-Clarke transform (U/V/W <-> alpha/beta)
- Park / inverse-Park transform (alpha/beta <-> d/q)
- `float` backend (`clarke_park_*_f()`) and IQmath fixed-point backend
  (`clarke_park_*_iq()`), selectable per call through a single unified API
- `CONFIG_CLARKE_PARK_IQ_FORMAT` to select the Q-format of the fixed-point
  backend (default Q15)
