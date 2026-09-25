# blemesh_manager

State-oriented BLE Mesh provisioner facade for ESP-IDF.

Wraps the raw ESP-BLE-MESH stack so the application can talk in terms of nodes and states ("set OnOff on 0x0011 to true") instead of opcodes, AppKeys, model IDs, and publication setup. Intended as a reusable BLE-mesh provisioner backend for any consumer that wants a clean async API — a protocol bridge, a custom REST gateway, a local controller, etc.

## Scope

Owned by this component:
- ESP-BLE-MESH stack init (`esp_ble_mesh_init`)
- Provisioner role bring-up (key materialization, AppKey add)
- Unprovisioned-beacon scanning gated by a commissioning window
- Per-node Config Client sequence: Composition Data Get → AppKey Add → Model App Bind → Model Pub Set → Heartbeat Pub Set → Default TTL Set
- Outbound Set/Get translation for OnOff / Level / Lightness / CTL / HSL and Sensor Get
- Pending-confirmation tracker that escalates Set→Get on timeout, then fires `on_set_failed`
- Heartbeat-driven reachability tracking
- Restoration of provisioned nodes from NVS on reboot
- Config Node Reset on remove / abort so children don't end up orphaned

Owned by the caller:
- NVS init, BT controller init, Bluedroid init (typically shared with other components)
- Declaration of the provisioner composition (`esp_ble_mesh_comp_t`), including Config Server, Config Client, and any Client models the consumer wants the manager to drive
- Lifetimes of `esp_ble_mesh_prov_t` and `esp_ble_mesh_comp_t` (manager keeps the pointers)
- App-level event handling via the callback struct

## Layout

```
blemesh_manager/
├── CMakeLists.txt           # component build wiring
├── Kconfig.projbuild        # tunables + auto-selected ESP-BLE-MESH flags
├── idf_component.yml        # component manifest (metadata + IDF dep)
├── include/blemesh_manager.h        # public API
├── src/
│   ├── blemesh_manager.c            # public API + lifecycle glue
│   ├── blemesh_prov_task.c          # single task loop, queue dispatch
│   ├── blemesh_provisioner.c        # stack callbacks, prov state machine
│   ├── blemesh_configurator.c       # Config Client sequence per node
│   ├── blemesh_node_directory.c     # volatile in-memory node table
│   ├── blemesh_pending_confirm.c    # Set→Get escalation scheduler
│   ├── blemesh_heartbeat.c          # reachability tracker
│   ├── blemesh_initial_sync.c       # post-provisioning paced state Gets
│   ├── blemesh_publication_rx.c     # Status decode → on_state_changed
│   ├── blemesh_dispatcher.c         # outbound Set/Get encoders
│   └── blemesh_internal.h           # shared types, queue messages
├── examples/repl_demo/              # runnable demo (see examples/repl_demo/README.md)
└── test_apps/                       # Unity tests for the Composition Data parsers
```

## Threading

One internal task (`blemesh_mgr`) drains a single FreeRTOS queue. Everything routes through that queue: public API calls, ESP-BLE-MESH stack callbacks, FreeRTOS-timer ticks. Consumer callbacks fire only from that task — callbacks **must not block**.

## Required ESP-BLE-MESH Kconfig

`Kconfig.projbuild` declares an umbrella option `BLEMESH_MGR_ENABLE` (default `y`) that `select`s every upstream flag the component needs:

- `BT_ENABLED`, `BLE_MESH`, `BLE_MESH_PROVISIONER`
- `BLE_MESH_PB_ADV`, `BLE_MESH_PB_GATT`
- `BLE_MESH_CFG_CLI`, `BLE_MESH_GENERIC_ONOFF_CLI`, `BLE_MESH_GENERIC_LEVEL_CLI`, `BLE_MESH_LIGHT_*_CLI`, `BLE_MESH_SENSOR_CLI`
- `BLE_MESH_SETTINGS` (so the stack persists net keys / IV index / node table / composition data)
- `BLE_MESH_PROVISIONER_RECV_HB` (so heartbeats reach the tracker)

Only host-agnostic, value-less (bool) flags are `select`ed. **The BT host is not chosen for you** — the component works over either NimBLE or Bluedroid (it only calls `esp_ble_mesh_*`), so the application picks `CONFIG_BT_NIMBLE_ENABLED` or `CONFIG_BT_BLUEDROID_ENABLED`. Likewise these must live in your `sdkconfig.defaults`: `BLE_MESH_USE_DUPLICATE_SCAN` + the controller-level mesh scan-dedup (`BT_CTRL_BLE_MESH_SCAN_DUPL_EN` + cache size) to tame the adv flood, and the numeric tunables (`BLE_MESH_TX_SEG_MSG_COUNT` / `BLE_MESH_RX_SEG_MSG_COUNT` for multi-segment Composition Data / Pub Set, `BLE_MESH_MSG_CACHE_SIZE`, `BLE_MESH_MAX_PROV_NODES`) which can't be `select`ed. See the example `sdkconfig.defaults` for sane values.

## Public API tour

```c
#include "blemesh_manager.h"

esp_err_t blemesh_manager_init(const blemesh_manager_config_t *cfg);
esp_err_t blemesh_manager_register_callbacks(const blemesh_manager_callbacks_t *cb);
esp_err_t blemesh_manager_register_models(const blemesh_manager_models_t *models);
esp_err_t blemesh_manager_start(void);

esp_err_t blemesh_manager_enable_commissioning(bool enable, uint32_t window_seconds);
esp_err_t blemesh_manager_set_state(blemesh_addr_t addr, const blemesh_state_value_t *val, uint32_t transition_ms);
esp_err_t blemesh_manager_get_state(blemesh_addr_t addr, blemesh_state_id_t state);

esp_err_t blemesh_manager_remove_node(blemesh_addr_t addr);
esp_err_t blemesh_manager_list_nodes(blemesh_node_info_t *out, size_t *count);
esp_err_t blemesh_manager_reset(void);   /* wipes mesh NVS; caller must esp_restart() */
```

Callbacks (every event is delivered on the manager task):

```c
typedef struct {
    void (*on_node_provisioned)(const blemesh_node_info_t *info, bool is_fresh);
    void (*on_node_removed)    (blemesh_addr_t addr);
    void (*on_reachability)    (blemesh_addr_t addr, bool reachable);
    void (*on_state_changed)   (blemesh_addr_t addr, const blemesh_state_value_t *val);
    void (*on_set_failed)      (blemesh_addr_t addr, blemesh_state_id_t state);
    void (*on_vendor_message)  (blemesh_addr_t addr, uint16_t cid, uint32_t opcode, const uint8_t *data, size_t len);
} blemesh_manager_callbacks_t;
```

State catalogue (v1):

```
BLEMESH_STATE_ONOFF        bool
BLEMESH_STATE_LEVEL        int16_t
BLEMESH_STATE_LIGHTNESS    uint16_t
BLEMESH_STATE_CTL          { uint16_t lightness, temperature; }
BLEMESH_STATE_HSL          { uint16_t l, h, s; }
BLEMESH_STATE_SENSOR_VALUE { uint16_t property_id; uint8_t data[16]; uint8_t len; }
```

Vendor opcodes are exposed through `blemesh_manager_send_vendor` + `on_vendor_message`.

## What `blemesh_manager_init` does

1. Registers all ESP-BLE-MESH stack callbacks (prov, cfg cli, generic cli, lighting cli, sensor cli, plus heartbeat-recv).
2. Calls `esp_ble_mesh_init(cfg->prov, cfg->comp)`.
3. Calls `esp_ble_mesh_provisioner_prov_enable(...)` once so the stack materializes the primary NetKey. The role stays enabled for the lifetime of the manager — `enable_commissioning(false)` does **not** call `prov_disable` because that would clear `VALID_PROV` and break unicast sends to provisioned nodes.
4. Adds the local AppKey.
5. Walks `esp_ble_mesh_provisioner_get_node_table_entry()` and rebuilds the volatile directory from NVS-persisted nodes (so devices provisioned in a previous boot are still addressable).
6. Spawns the manager task + queue + FreeRTOS timers (commissioning auto-close, pending-confirm tick, heartbeat tick).

You then `register_callbacks` and `register_models`. The latter also binds the local AppKey to every supplied Client model and subscribes the uplink Clients to `cfg->uplink_group_addr` (so Status publications from all nodes fan in). Then `start()` replays restored nodes to the consumer via `on_node_provisioned` and arms the heartbeat tracker.

## Initial-state synchronization

Every time `on_node_provisioned` fires — either for a fresh leaf finishing configuration or for a node replayed from NVS at boot — the manager schedules paced Get requests so the consumer learns the leaf's actual current state instead of seeing whatever default / stale value its own NVS may have persisted. Without this layer the consumer's first user-driven Set after reboot is often a no-op (the leaf is already at the requested value).

Per profile:

| Profile                    | Gets issued                                     |
| ---                        | ---                                              |
| `BLEMESH_DEV_ONOFF`        | OnOff                                            |
| `BLEMESH_DEV_DIMMABLE`     | OnOff + Lightness                                |
| `BLEMESH_DEV_COLOR_TEMP`   | OnOff + CTL  (lightness + temperature in one Status reply) |
| `BLEMESH_DEV_COLOR_HSL`    | OnOff + HSL  (L + H + S in one Status reply)     |
| `BLEMESH_DEV_OCCUPANCY` / `BLEMESH_DEV_TEMPERATURE` | Sensor                  |

The Gets are queued into a small FIFO (`CONFIG_BLEMESH_MGR_INITIAL_SYNC_QUEUE_DEPTH`, default 64) and dispatched one at a time on a `CONFIG_BLEMESH_MGR_INITIAL_SYNC_INTERVAL_MS` tick (default 250 ms) so a busy boot — many nodes restored at once — does not overrun the mesh stack's segment buffers. Status replies arrive through the regular RX path and surface to the consumer via `on_state_changed`. If a Status never arrives the consumer stays un-synced until the next leaf-side change reaches it; there is no Get-retry layer.

The same paced Gets also fire whenever the heartbeat tracker flips a node from unreachable back to reachable, **and** on every received heartbeat publication. The unreachable-to-reachable trigger catches leaves that fell below the loss threshold; the every-heartbeat trigger catches the harder case of a leaf that reboots and rejoins within a single heartbeat period (so the node was never marked unreachable in the first place). Without the latter the consumer silently keeps using its stale cache and the next write to that leaf produces a no-op `Set`.

Triggering on every heartbeat does mean the manager keeps re-pulling each leaf's state once per `heartbeat_period_ms` even when nothing changed. To keep that bounded, `blemesh_initial_sync_enqueue` deduplicates by `(addr, state_id)` against the FIFO contents — if a previous round of Gets has not yet drained, redundant heartbeat-driven enqueues are silently skipped. Steady-state air-time cost is therefore at most `n_states_for_profile` Gets plus their Status replies per heartbeat per node, all serialised by the same paced timer; the heartbeat period is the natural rate-limit, so widening `heartbeat_period_ms` directly reduces the steady-state sync traffic.

## Provisioning serialization

Only one device is provisioned + configured at a time:

- A `provisioning_busy` flag in the context is set when the stack cb calls `add_unprov_dev` and cleared when the configurator finishes (success, abort, or the queue drains).
- New unprovisioned-beacon events are silently dropped while busy.
- The configurator keeps an 8-entry FIFO so that if a second `PROV_COMPLETE` arrives mid-configuration (e.g. two nodes were already in the stack's add_unprov queue), it is processed serially after the current one finishes.

This avoids the SeqAuth collisions and segment-buffer exhaustion that parallel configurations produce.

## Persistence

The component is volatile in its own state but relies on the stack's NVS storage (`CONFIG_BLE_MESH_SETTINGS=y`) for net keys, IV index, sequence numbers, the node list, and (when explicitly stored) composition data.

After every successful Composition Data Get the configurator calls `esp_ble_mesh_provisioner_store_node_comp_data` so that on next boot the restore step can re-classify each node's profile correctly.

`blemesh_manager_reset()` calls `esp_ble_mesh_deinit({.erase_flash = true})` — that wipes everything mesh-related from NVS. The call does **not** reboot the device; the caller is responsible for invoking `esp_restart()` (after wiping any sibling NVS namespaces it owns). The standalone REPL example calls `esp_restart()` immediately after this function returns.

## Open design notes

- Heartbeat publish period is derived per-node from `cfg.heartbeat_period_ms` so the tracker's missed-period count actually matches what nodes publish.
- Model publication retransmit count is set to **0** on each configured node — the network layer already retransmits; otherwise the provisioner sees N copies of every Status.
- Foundation models (Cfg Srv/Cli, Health Srv/Cli) are skipped during AppKey-Bind / Pub-Set because they use DevKey.
- Configurator timeout / failure path sends Config Node Reset to the child before deleting locally, so half-configured nodes wipe themselves and re-appear as unprovisioned next time the window opens.

## Example

See [`examples/repl_demo/README.md`](examples/repl_demo/README.md) for a runnable demo with an `esp_console` REPL exposing every API command.

## Tests

`test_apps/` is a Unity target app covering the radio-free Composition Data (Page 0) parsers — logical-element derivation and the CTL/HSL channel-element offsets — which is the deterministic, hardware-independent logic most prone to byte-offset regressions. Build and flash to any BLE-capable target, then run the cases from the Unity menu:

```sh
cd test_apps
idf.py set-target esp32c3        # or esp32 / esp32c6 / esp32s3 / esp32h2
idf.py -p PORT build flash monitor
```
