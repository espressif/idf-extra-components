# blemesh_manager — Console Example

Runnable demo for the [`blemesh_manager`](../..) component. Brings up the BLE stack, declares a provisioner composition with every Client model the manager knows about, registers logging callbacks for every event, and exposes the manager's API through an `esp_console` REPL on the default UART.

## Layout

```
examples/repl_demo/
├── CMakeLists.txt
├── sdkconfig.defaults       # required ESP-BLE-MESH + BT + console flags
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml    # override_path dep on ../../..
    └── blemesh_example_main.c
```

## Build and Flash

Before project configuration and build, set the correct chip target:

```sh
idf.py set-target esp32s3        # or esp32 / esp32c3 / esp32c6 / esp32s2
```

Then run `idf.py -p PORT build flash monitor` to build, flash and monitor the project.
(To exit the serial monitor, type `Ctrl-]`.)

See the [Getting Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/get-started/index.html) for full steps to configure and use ESP-IDF to build projects.

If you've used the example before and want a clean state, run `idf.py -p PORT erase-flash flash monitor`, or type `reset` at the REPL.

## BLE host: NimBLE vs Bluedroid

The `blemesh_manager` component is **host-agnostic** — it only calls the
`esp_ble_mesh_*` APIs, which work over either NimBLE or Bluedroid. The host is
the application's choice (a Kconfig `choice`), so the component does **not**
select one.

This example defaults to **NimBLE** (`CONFIG_BT_NIMBLE_ENABLED=y` in
`sdkconfig.defaults`) — it is lighter and is what the manager was tuned on. The
example's `bluetooth_init()` has both branches under `#ifdef`:

- **NimBLE:** `nimble_port_init()` → set `ble_hs_cfg` callbacks → `ble_store_config_init()` → start the host task → block on a sync semaphore until the host resolves its address (mesh must init only after sync). The controller is brought up by `nimble_port_init()` itself.
- **Bluedroid:** `esp_bt_controller_init/enable(BLE)` → `esp_bluedroid_init/enable()`.

To switch to Bluedroid, set `CONFIG_BT_BLUEDROID_ENABLED=y` (instead of NimBLE)
in `sdkconfig.defaults` and rebuild clean (`idf.py fullclean set-target …`).
No code change needed — the `#ifdef` picks the right branch.

> If you see `BT_APPL: …` log lines, you're running Bluedroid; NimBLE never
> prints those.

## What the example sets up

- BLE host: **NimBLE** (see above; flip to Bluedroid in `sdkconfig.defaults`).
- Provisioner composition with: Config Server, Config Client, Generic OnOff Cli, Generic Level Cli, Light Lightness Cli, Light CTL Cli, Light HSL Cli, Sensor Cli.
- Provisioner own address `0x0001`, range of 16 reserved addrs, node addresses start at `0x0011`.
- Uplink group `0xC000` for fan-in of all node Status publications.
- Hard-coded net key and app key (do not ship like this — fine for a desktop demo).
- Heartbeat period 30 s, loss threshold 3 → unreachable after ~90 s of silence.
- `set_confirm_timeout_ms` 4000 ms, `set_retries` 1.

The manager's `[EVT] …` log lines come from the example's callbacks — see `blemesh_example_main.c` for the exact format.

## REPL commands

Type `help` at the `blemesh>` prompt for the full list.

| Command | Purpose |
| --- | --- |
| `nodes` | Print all provisioned nodes with profile, element count, reachability, UUID. |
| `commission <on\|off> [-t sec]` | Open / close the commissioning window. Default window 60 s. |
| `set <addr> <state> <value> [-t ms]` | Send a Set. See state syntax below. |
| `get <addr> <state>` | Send a Get. Result arrives via `on_state_changed`. |
| `remove <addr>` | Send Config Node Reset to the child, then forget it locally. |
| `reset` | Wipe mesh NVS and reboot the provisioner. |
| `help [cmd]` | Built-in. |

`<addr>` accepts decimal (`17`) or hex (`0x0011`).

### `set` value formats

| State | `<value>` syntax | Example |
| --- | --- | --- |
| `onoff` | `on` / `off` / `0` / `1` / `true` | `set 0x0011 onoff on` |
| `level` | signed int16 | `set 0x0011 level -1000` |
| `lightness` | uint16 (0–65535) | `set 0x0011 lightness 32768` |
| `ctl` | `<lightness>,<temp>` | `set 0x0011 ctl 32768,5000` |
| `hsl` | `<l>,<h>,<s>` | `set 0x0011 hsl 32768,21845,65535` |

The optional `-t <ms>` is the model-level transition time hint; encoded into the Generic Default Transition Time format.

## Typical session

```
blemesh> commission on -t 120
commissioning ON (window=120s) -> ESP_OK

# Power on an unprovisioned ESP-BLE-MESH node (e.g. the IDF
# bluetooth/esp_ble_mesh/onoff_models/onoff_server example).

I (15232) blemesh_prov: provisioning complete addr=0x0011
I (18902) blemesh_ex:  [EVT] node provisioned addr=0x0011 profile=OnOff elem=3 (fresh) uuid=…

blemesh> nodes
addr    profile      elem  reach  uuid
------- ------------ ----  -----  ------------------------------------
0x0011  OnOff           3  yes    dddd…

blemesh> set 0x0011 onoff on
set -> ESP_OK
I (23582) blemesh_ex:  [EVT] state change: addr=0x0011 onoff=1

blemesh> remove 0x0011
remove -> ESP_OK
I (...) blemesh_ex:  [EVT] node removed addr=0x0011
```

After a reboot of the provisioner, `nodes` will still list previously provisioned children (re-built from NVS), and `set` / `get` work without re-commissioning — the children retained their state too.

## Recovering wedged nodes

If a child is stuck (provisioned from its perspective but unknown to the provisioner, or vice-versa):

- From the provisioner: `remove <addr>` sends Config Node Reset. The child must handle `ESP_BLE_MESH_NODE_PROV_RESET_EVT` and call `esp_restart()` for its advertising to resume — see the upstream IDF example.
- If the provisioner can't reach the child: factory reset the child. Then run `reset` here to wipe the provisioner.

## Notes on the example wiring (vs. what the manager does)

For clarity the example main keeps the things that are inherently the application's responsibility:

- `nvs_flash_init`
- `esp_bt_controller_init + enable(BLE_MODE)`
- `esp_bluedroid_init + enable`
- declaration of the `esp_ble_mesh_prov_t` and `esp_ble_mesh_comp_t` structs and the `esp_ble_mesh_client_t` / publication contexts they reference

The manager itself handles `esp_ble_mesh_init`, provisioner role enable, local AppKey add, callback registration, model AppKey binding, uplink group subscription, NVS-restore of known nodes, and the whole commissioning + Config Client flow.
