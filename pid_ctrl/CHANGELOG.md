# Changelog

## 0.3.0

- Added IQmath fixed-point backend (`pid_*_iq()`) alongside the existing float backend
- Added a unified, type-dispatched interface for the unsuffixed API names
  - In C, `_Generic` selects between the float and IQ backends based on the handle / config pointer type
  - In C++, the unsuffixed names are provided as overloaded `static inline` wrappers
- Added `CONFIG_PID_CTRL_IQ_FORMAT` (Kconfig) to control the IQmath Q-format used by the `_iq` backend (default Q24)
- Renamed the float-backend opaque struct tags to disambiguate from the IQ backend (`struct pid_ctrl_block_t` → `struct pid_ctrl_block_f_t`, etc.).

## 0.2.0

- Added `pid_reset_ctrl_block()` to clear the accumulated error of a control block

## 0.1.0

- Initial version
