# ESP Serial Slave Link

## Overview

Espressif provides several chips that can work as slaves. These slave devices rely on some common buses, and have their own communication protocols over those buses. The `esp_serial_slave_link` component is designed for the **master** to communicate with ESP slave devices through those protocols over the bus drivers, although the name of the component implies it a `slave link`.

After a slave device is initialized properly, the application can use it to communicate with the slave devices conveniently, as long as the slave complies with the protocols. The component provides a set of APIs to operate the slave device, including sending and receiving data, triggering interrupts, etc.

## Terminology

-   ESSL: Abbreviation for ESP Serial Slave Link, the component described by this document.
-   Master: The device running the `esp_serial_slave_link` component.
-   ESSL Device: A virtual device on the master associated with an ESP slave device. The device context has the knowledge of the slave protocol above the bus, relying on some bus drivers to communicate with the slave.
-   ESSL Device Handle: a handle to ESSL device context containing the configuration, status and data required by the ESSL component. The context stores the driver configurations, communication state, data shared by master and slave, etc.

    -   The context should be initialized before it is used, and get de-initialized if not used any more. The master application operates on the ESSL device through this handle.

-   ESP Slave: the slave device connected to the bus, which ESSL component is designed to communicate with.
-   Bus: The bus over which the master and the slave communicate with each other.
-   Slave Protocol: The special communication protocol specified by Espressif HW/SW over the bus.
-   TX Buffer Num: a counter, which is on the slave and can be read by the master, indicates the accumulated buffer numbers that the slave has loaded to the hardware to receive data from the master.
-   RX Data Size: a counter, which is on the slave and can be read by the master, indicates the accumulated data size that the slave has loaded to the hardware to send to the master.

## About the Slave Communication Protocols

For more details about the device communication protocols, please refer to the following documents:

-   [SDIO Slave Protocol](sdio_slave_protocol.md)
-   [SPI Slave Half Duplex Protocol](spi_slave_hd_protocol.md)

## Services Provided by ESP Slave

There are some common services provided by the Espressif slaves:

1.  Tohost Interrupts: The slave can inform the master about certain events by the interrupt line. (optional)
2.  Frhost Interrupts: The master can inform the slave about certain events.
3.  TX FIFO (master to slave): The slave can receive data from the master in units of receiving buffers. The slave updates the TX buffer num to inform the master how much data it can receive, and the master then read the TX buffer num, and take off the used buffer number to know how many buffers are remaining.
4.  RX FIFO (slave to master): The slave can send data in stream to the master. The SDIO slave can also indicate it has new data to send to master by the interrupt line. The slave updates the RX data size to inform the master how much data it has prepared to send, and then the master read the data size, and take off the data length it has already received to know how many data is remaining.
5.  Shared registers: The master can read some part of the registers on the slave, and also write these registers to let the slave read.

## Initialization of ESP Serial Slave Link

### ESP SDIO Slave

The ESP SDIO slave link (ESSL SDIO) devices relies on the SDMMC component. It includes the usage of communicating with ESP SDIO Slave device via the SDMMC Host or SDSPI Host feature. The ESSL device should be initialized as follows:

1.  Initialize a SDMMC card (see [Document of SDMMC driver](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html)) structure.
2.  Call `sdmmc_card_init` to initialize the card.
3.  Initialize the ESSL device with `essl_sdio_config_t`. The `card` member should be the `sdmmc_card_t` got in step 2, and the `recv_buffer_size` member should be filled correctly according to pre-negotiated value.
4.  Call [essl_init](api.md#function-essl_init) to do initialization of the SDIO part.
5.  Call [essl_wait_for_ready](api.md#function-essl_wait_for_ready) to wait for the slave to be ready.

<div class="warning">

If you are communicating with the ESP SDIO Slave device through SPI interface, you should still choose this **SDIO interface**.

</div>

### ESP SPI Slave

Has not been supported yet.

## Typical Usage of ESP Serial Slave Link

After the initialization process above is performed, you can call the APIs below to make use of the services provided by the slave:

### Tohost Interrupts (Optional)

1.  Call [essl_get_intr_ena](api.md#function-essl_get_intr_ena) to know which events will trigger the interrupts to the master.
2.  Call [essl_set_intr_ena](api.md#function-essl_set_intr_ena) to set the events that should trigger interrupts to the master.
3.  Call [essl_wait_int](api.md#function-essl_wait_int) to wait until interrupt from the slave, or timeout.
4.  When interrupt is triggered, call [essl_get_intr](api.md#function-essl_get_intr) to know which events are active, and call [essl_clear_intr](api.md#function-essl_clear_intr) to clear them.

### Frhost Interrupts

1.  Call [essl_send_slave_intr](api.md#function-essl_send_slave_intr) to trigger general purpose interrupt of the slave.

### TX FIFO

1.  Call [essl_get_tx_buffer_num](api.md#function-essl_get_tx_buffer_num) to know how many buffers the slave has prepared to receive data from the master. This is optional. The master will poll `tx_buffer_num` when it tries to send packets to the slave, until the slave has enough buffer or timeout.
2.  Call [essl_send_packet](api.md#function-essl_send_packet) to send data to the slave.

### RX FIFO

1.  Call [essl_get_rx_data_size](api.md#function-essl_get_rx_data_size) to know how many data the slave has prepared to send to the master. This is optional. When the master tries to receive data from the slave, it updates the `rx_data_size` for once, if the current `rx_data_size` is shorter than the buffer size the master prepared to receive. And it may poll the `rx_data_size` if the `rx_data_size` keeps 0, until timeout.
2.  Call [essl_get_packet](api.md#function-essl_get_packet) to receive data from the slave.

### Reset Counters (Optional)

Call [essl_reset_cnt](api.md#function-essl_reset_cnt) to reset the internal counter if you find the slave has reset its counter.
