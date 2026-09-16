# Changelog

## 0.4.0

- IQmath backend is now generic over the Q-format: `_iq1` through `_iq30`
  can be used simultaneously, each dispatching to its own backend
- Removed `CONFIG_PID_CTRL_IQ_FORMAT` (Kconfig) and the `GLOBAL_IQ` requirement.
  The component no longer constrains `GLOBAL_IQ`, so other components are free
  to choose their own format
- An IQmath API per concrete format is added, e.g. `pid_new_control_block_iq24()`
  with `pid_ctrl_config_iq24_t` / `pid_ctrl_block_handle_iq24`
- The unsuffixed `_iq` API is unchanged in name: `pid_ctrl_config_iq_t` /
  `pid_ctrl_block_handle_iq_t` and `pid_*_iq()` keep working. They are inline
  wrappers around the `_iqN` backend named by `GLOBAL_IQ` in the including
  translation unit, and have their own types so they can be mixed with the
  concrete `_iqN` ones in the same translation unit

## 0.3.1

- Renamed PID control block handle typedefs to keep backend suffix naming consistent:
  - `pid_ctrl_block_f_handle_t` -> `pid_ctrl_block_handle_f_t`
  - `pid_ctrl_block_iq_handle_t` -> `pid_ctrl_block_handle_iq_t`

## 0.3.0

- Added IQmath fixed-point backend (`pid_*_iq()`)
- Added `CONFIG_PID_CTRL_IQ_FORMAT` to configure the IQmath Q-format

## 0.2.0

- Added `pid_reset_ctrl_block()` to clear the accumulated error of a control block

## 0.1.0

- Initial version
