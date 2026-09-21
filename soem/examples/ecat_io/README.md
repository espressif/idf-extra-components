# EtherCAT digital IO example

This example runs a SOEM EtherCAT master on ESP32-P4 and exchanges digital process data with one LAN9252 slave.

Pressing SW1 turns LED3 on. Releasing SW1 turns LED3 off.

Component usage, Kconfig options, and the list of unvalidated SOEM features are in the [soem component README](../../README.md).

## Experimental setup

### Hardware

Master: [ESP32-P4-Function-EV-Board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32p4/esp32-p4-function-ev-board/index.html). The board uses the ESP32-P4 internal EMAC with an external IP101 Ethernet PHY. The example uses PHY address 1 and GPIO 51 as the PHY reset pin.

Slave: a LAN9252 EtherCAT digital-IO board (Digital I/O mode, no backend microcontroller). Connect the P4 Ethernet port to the slave **IN** port.

The demo uses two field signals on that board:

- **SW1** is a digital input, **active low** (pressed = 0 in process data).
- **LED3** is a digital output, **active high** (write 1 to turn the LED on).

The slave SII EEPROM is incomplete. It carries the identity fields needed for discovery (vendor ID, product code, revision) but does not contain a usable SM/PDO description. Stock SOEM automatic mapping cannot configure this device. The example therefore programs SM and FMMU from the vendor ESI XML by hand. Do not treat this board as a generic CoE slave.

### Peripherals and process-data mapping

The example uses:

- Internal EMAC and the IP101 PHY, through `esp_eth`
- `esp_event` for Ethernet link-up / link-down
- `esp_timer` for the 10 ms cyclic period

From the ESI XML the process data is one output byte (RxPDO) and one input byte (TxPDO). The example programs:

| Direction (slave view) | SM / FMMU | ESC physical address | Logical address | Field used in the demo |
|---|---|---|---|---|
| Output (LED) | SM0, FMMU0 | `0x0F01` | `0x00000000` | bit 0 → LED3, active high |
| Input (switch) | SM1, FMMU1 | `0x1000` | `0x00000001` | bit 0 → SW1, active low |

The application allocates its own `process_image[2]`. Byte 0 is the output to be written this cycle; byte 1 is the input returned by `ecx_LRW()`.

### Application stages

`app_main()` brings the network up in order:

1. Start Ethernet and wait for link up.
2. Bind SOEM to the Ethernet handle (`esp_soem_init()`).
3. Discover slaves (`ecx_config_init()`). That call also requests PRE-OP with the error-acknowledge bit set and does not wait.
4. Check that exactly one slave is present, then request **plain PRE-OP** and wait until AL status is exactly `EC_STATE_PRE_OP`. This extra request is required because the LAN9252 runs ESC device emulation; see the note in the [component README](../../README.md).
5. Program SM/FMMU with `ecx_FPWR()`.
6. Request SAFE-OP and wait.
7. Request OP while keeping LRW running, then wait.
8. Enter the cyclic IO loop.

### Tasks and interrupts

Receive path: the EMAC RX interrupt signals that DMA has finished a frame. The Ethernet RX task copies the frame and calls the SOEM callback `ecx_esp_eth_rx()`, which stores a valid EtherCAT reply in the indexed RX slot and gives `rx_sem`.

Transmit path: cyclic LRW runs in `main_task` (the task that called `app_main()`), not in an ISR. `ecx_LRW()` builds the frame and calls `esp_eth_transmit()`.

Cycle timing: an `esp_timer` with `ESP_TIMER_TASK` dispatch fires every 10 ms and notifies `main_task`. That task blocks on `ulTaskNotifyTake()`, then performs one LRW and updates `process_image`. The 10 ms period is functional, not a hard real-time guarantee.

Link events are delivered on the default `esp_event` loop and recorded in an event group. The cyclic loop checks the link-up bit before each LRW.

## Experiment procedure and results

### Build and run

Set up the ESP-IDF environment, then run:

```bash
cd soem/examples/ecat_io
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port connected to the ESP32-P4 board. Exit the monitor with `Ctrl+]`.

### Expected behavior

After link up the log should show one slave found, PRE-OP, mapping, SAFE-OP, OP, then cyclic IO. Pressing SW1 turns LED3 on; releasing SW1 turns LED3 off. Input typically changes between `0xff` (released) and `0xfe` (pressed).

A successful startup and process-data exchange produces output similar to:

```text
I (2411) ecat_io: Ethernet Link Up
I (2411) ecat_io: MAC XX:XX:XX:XX:XX:XX
I (2411) ecat_io: SOEM initialized
I (2411) ecat_io: 1 EtherCAT slave(s) found
I (2411) ecat_io: Slave: vendor=0xXXXXXXXX, product=0xXXXXXXXX, revision=0xXXXXXXXX
I (2421) ecat_io: Slave reached PRE-OP
I (2421) ecat_io: Process-data mapping configured
I (2421) ecat_io: Slave reached SAFE-OP
I (2431) ecat_io: Slave reached OP
I (2431) ecat_io: EtherCAT cyclic IO started
I (2431) ecat_io: Press SW1: LED3 ON; release SW1: LED3 OFF
I (2441) ecat_io: Cycle 0: input=0xff SW1=RELEASED, next output=0x00 LED3=OFF
I (3891) ecat_io: Cycle 145: input=0xfe SW1=PRESSED, next output=0x01 LED3=ON
I (4181) ecat_io: Cycle 174: input=0xff SW1=RELEASED, next output=0x00 LED3=OFF
I (5071) ecat_io: Cycle 263: input=0xfe SW1=PRESSED, next output=0x01 LED3=ON
I (6011) ecat_io: Cycle 357: input=0xff SW1=RELEASED, next output=0x00 LED3=OFF
I (6731) ecat_io: Cycle 429: input=0xfe SW1=PRESSED, next output=0x01 LED3=ON
I (8171) ecat_io: Cycle 573: input=0xff SW1=RELEASED, next output=0x00 LED3=OFF
```

## Notes and limitations

This example supports only the slave identity and fixed mapping described above. Other EtherCAT slaves need their own identity check, SM/FMMU layout, and expected working counter.

This example treats link-down and unexpected WKC as fatal (`ESP_ERROR_CHECK`), which aborts and typically resets the chip under the default ESP-IDF panic handler. That is demo policy only. A real application must decide for itself how to stop cyclic IO, how to exit, and whether to recover.
