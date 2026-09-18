# Changelog

## 0.2.0

- IQmath backend is now generic over the Q-format: `_iq1` through `_iq29`
  can be used simultaneously, each dispatching to its own backend
- `_iq30` is not supported: IQmath provides radian sin/cos only through Q29
- Removed `CONFIG_CLARKE_PARK_IQ_FORMAT` (Kconfig). The unsuffixed `_iq` API
  is an inline wrapper around the `_iqN` backend named by `GLOBAL_IQ` in the
  including translation unit
- IQmath API is now per format, e.g. `clarke_park_clarke_iq15()` on
  `clarke_park_uvw_iq15_t`
- The `_iq` API still works and follows the including translation unit's
  `GLOBAL_IQ`

## 0.1.0

- Initial version

### Added

- Clarke / inverse-Clarke transform (U/V/W <-> alpha/beta)
- Park / inverse-Park transform (alpha/beta <-> d/q)
- `float` backend (`clarke_park_*_f()`) and IQmath fixed-point backend
  (`clarke_park_*_iq()`), selectable per call through a single unified API
- `CONFIG_CLARKE_PARK_IQ_FORMAT` to select the Q-format of the fixed-point
  backend (default Q15)
