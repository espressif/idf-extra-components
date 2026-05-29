# Changelog

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

