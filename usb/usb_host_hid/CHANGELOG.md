## 1.0.1

- Fixed a bug where configuring the driver with `create_background_task = false` did not properly initialize the driver. This lead to `the hid_host_uninstall()` hang-up.
- Fixed bug where `hid_host_uninstall()` would cause a crash during the call while USB device has not been removed.
- Added `hid_host_get_device_info()` to get the basic information of a connected USB HID device.

## 1.0.0

- Initial version



