# EtherCAT digital IO example

This example runs a SOEM EtherCAT master on ESP32-P4 and exchanges digital process data with one LAN9252 slave.

Pressing SW1 turns LED3 on. Releasing SW1 turns LED3 off.

## Hardware

The example requires:

- An ESP32-P4 using the internal EMAC
- An external IP101 Ethernet PHY
- One LAN9252 EtherCAT digital IO slave

The example uses PHY address 1 and GPIO 51 as the PHY reset pin.

Connect the ESP32-P4 Ethernet port to the EtherCAT input port of the slave.

The expected slave identity is:

- Vendor ID: `0x000004D8`
- Product code: `0x0000000D`
- Revision: `0x00000001`

## Process-data mapping

The example configures a fixed one-byte output and one-byte input mapping.

Output:

- Logical address: `0x00000000`
- ESC physical address: `0x0F01`
- SM0 and FMMU0
- LED3 on bit 0, active high

Input:

- Logical address: `0x00000001`
- ESC physical address: `0x1000`
- SM1 and FMMU1
- SW1 on bit 0, active low

The mapping is configured directly because the tested slave does not support CoE PDO mapping.

## Build and run

Set up the ESP-IDF environment, then run:

```bash
cd soem/examples/ecat_io
idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port connected to the ESP32-P4 board.

## Expected behavior

The example discovers and validates the slave, configures its process-data mapping and requests PRE-OP, SAFE-OP and OP states.

In OP, LRW process-data communication runs every 10 ms. The input state of SW1 determines the output state of LED3.

Communication stops if the Ethernet link is disconnected or an unexpected working counter is received.

## Example output

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

## Limitations

This example supports only the slave identity and fixed mapping described above. Other EtherCAT slaves require their own identity checks, process-data mapping and expected working counter.
