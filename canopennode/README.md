# ESP-IDF CANopenNode Component

`canopennode` ports upstream [CANopenNode](https://github.com/CANopenNode/CANopenNode)
to the ESP-IDF `esp_driver_twai` driver.

This component is meant to be used as a library inside an ESP-IDF application.
Application-specific Object Dictionary files and CANopen application behavior
are still provided by the consuming project.

## Support

Current CiA 301 coverage in this port:

- [x] `NMT_Heartbeat` - node state handling and heartbeat producer.
- [x] `Emergency` - emergency producer support and error reporting path.
- [x] `OD interface` - Object Dictionary access layer used by the stack.
- [x] `SDO server` - local OD access from a CANopen master or PC tool.
- [x] `SDO client` - access to remote CANopen nodes from this device.
- [x] asynchronous `RPDO` / `TPDO` - event-driven PDO communication without `SYNC`.
- [ ] `SYNC`
- [ ] synchronous PDO
- [ ] `TIME`
- [ ] `HB consumer`
- [ ] `LSS`
- [ ] `LEDs`

## Kconfig

`Component config -> CANopenNode Stack Config`

- `Enable SDO server`: enable the CiA 301 SDO server object. This is the normal
  choice for devices that expose local parameters through the Object Dictionary.
- `Enable SDO client`: enable the CiA 301 SDO client object and FIFO support.
  Use this if the device needs to initiate uploads or downloads to other nodes.
- `Enable asynchronous PDO`: enable `CO_PDO.c` with asynchronous `RPDO` / `TPDO`
  support. The application OD must provide valid `0x1400/0x1600` and/or
  `0x1800/0x1A00` objects, and the app must call `CO_CANopenInitPDO()` plus
  periodic `CO_process_RPDO()` / `CO_process_TPDO()`.
- `Enable MULTIPLE_OD`: enable `CO_MULTIPLE_OD` so the application can provide
  its own generated `OD.c` / `OD.h`. If disabled, the app-level CMake setup
  must expose a global `OD.h` include path visible to `CANopen.c`.

## Using This Port

- The application owns TWAI node creation and deletion. This component does not
  configure GPIOs, bitrate, queue depth, or transceiver details.
- The application provides `OD.c` / `OD.h`.
- If `CO_MULTIPLE_OD` is disabled, the build must still make `OD.h` visible to
  `CANopen.c`, typically by exporting the application's OD include path.
- The application reset loop calls `CO_CANinit()`, `CO_CANopenInit()`,
  optionally `CO_CANopenInitPDO()`, then `CO_CANsetNormalMode()` and periodic
  `CO_process()`.
- When PDO support is enabled, the application must also call
  `CO_process_RPDO()` and `CO_process_TPDO()` from its periodic loop.
- Modules marked unsupported above are intentionally forced off by the ESP32
  target configuration in `src/CO_driver_target.h`.

For a minimal ESP-IDF integration example, see `examples/heartbeat`.
