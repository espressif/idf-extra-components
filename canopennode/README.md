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

## Kconfig

`Component config -> CANopenNode Stack Config`

- `Enable SDO server`: enable the CiA 301 SDO server object. This is the normal
  choice for devices that expose local parameters through the Object Dictionary.
- `Enable SDO client`: enable the CiA 301 SDO client object and FIFO support.
  Use this if the device needs to initiate uploads or downloads to other nodes.
- `Enable asynchronous PDO`: enable `CO_PDO.c` with asynchronous `RPDO` / `TPDO`
  support.

## Using This Port

- The application owns TWAI node creation and deletion. This component does not
  configure GPIOs, bitrate, queue depth, or transceiver details.
- The application provides `OD.c` / `OD.h`.
- The application should provide relevant OD definition for enabled modules.
- When PDO support is enabled, the application must also call
  `CO_process_RPDO()` and `CO_process_TPDO()` from its periodic loop.
- Modules marked unsupported above are intentionally forced off by the driver ports in `src/CO_driver_target.h`.

### Notes
- If `CO_MULTIPLE_OD` is disabled, the application should make `OD.h` visible to
  `CANopen.c`using `target_include_directories`, details please refer to example's CMakeLists.txt.

For a minimal ESP-IDF integration example, see `examples/heartbeat`.
