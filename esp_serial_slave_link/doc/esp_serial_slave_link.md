## ESP Serial Slave Link

[\[中文\]](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/protocols/esp_serial_slave_link.html)

## Overview

Espressif provides several chips that can work as slaves. These slave devices rely on some common buses, and have their own communication protocols over those buses. The `esp_serial_slave_link` component is designed for the master to communicate with ESP slave devices through those protocols over the bus drivers.

After an `esp_serial_slave_link` device is initialized properly, the application can use it to communicate with the ESP slave devices conveniently.

Note

The ESP-IDF component `esp_serial_slave_link` has been moved from ESP-IDF since version v5.0 to a separate repository:

+   [ESSL component on GitHub](https://github.com/espressif/idf-extra-components/tree/master/esp_serial_slave_link)


To add ESSL component in your project, please run `idf.py add-dependency espressif/esp_serial_slave_link`.

## Espressif Device Protocols

For more details about Espressif device protocols, see the following documents.

+   [Communication with ESP SDIO Slave](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_sdio_slave_protocol.html)
+   [ESP SPI Slave HD (Half Duplex) Mode Protocol](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_spi_slave_protocol.html)

## Terminology

+   ESSL: Abbreviation for ESP Serial Slave Link, the component described by this document.

+   Master: The device running the `esp_serial_slave_link` component.

+   ESSL device: a virtual device on the master associated with an ESP slave device. The device context has the knowledge of the slave protocol above the bus, relying on some bus drivers to communicate with the slave.

+   ESSL device handle: a handle to ESSL device context containing the configuration, status and data required by the ESSL component. The context stores the driver configurations, communication state, data shared by master and slave, etc.

    > +   The context should be initialized before it is used, and get deinitialized if not used any more. The master application operates on the ESSL device through this handle.
    >

+   ESP slave: the slave device connected to the bus, which ESSL component is designed to communicate with.

+   Bus: The bus over which the master and the slave communicate with each other.

+   Slave protocol: The special communication protocol specified by Espressif HW/SW over the bus.

+   TX buffer num: a counter, which is on the slave and can be read by the master, indicates the accumulated buffer numbers that the slave has loaded to the hardware to receive data from the master.

+   RX data size: a counter, which is on the slave and can be read by the master, indicates the accumulated data size that the slave has loaded to the hardware to send to the master.


## Services Provided by ESP Slave

There are some common services provided by the Espressif slaves:

1.  Tohost Interrupts: The slave can inform the master about certain events by the interrupt line. (optional)

2.  Frhost Interrupts: The master can inform the slave about certain events.

3.  TX FIFO (master to slave): The slave can receive data from the master in units of receiving buffers.

    The slave updates the TX buffer num to inform the master how much data it can receive, and the master then read the TX buffer num, and take off the used buffer number to know how many buffers are remaining.

4.  RX FIFO (slave to master): The slave can send data in stream to the master. The SDIO slave can also indicate it has new data to send to master by the interrupt line.

    The slave updates the RX data size to inform the master how much data it has prepared to send, and then the master read the data size, and take off the data length it has already received to know how many data is remaining.

5.  Shared registers: The master can read some part of the registers on the slave, and also write these registers to let the slave read.


The services provided by the slave depends on the slave’s model. See [SDIO Slave Capabilities of Espressif Chips](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_sdio_slave_protocol.html#esp-sdio-slave-caps) and [SPI Slave Capabilities of Espressif Chips](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_spi_slave_protocol.html#esp-spi-slave-caps) for more details.

## Initialization of ESP Serial Slave Linkheadline")

### ESP SDIO Slave

The ESP SDIO slave link (ESSL SDIO) devices relies on the SDMMC component. It includes the usage of communicating with ESP SDIO Slave device via the SDMMC Host or SDSPI Host feature. The ESSL device should be initialized as below:

1.  Initialize a SDMMC card (see :doc:\` Document of SDMMC driver </api-reference/storage/sdmmc>\`) structure.

2.  Call [`sdmmc_card_init()`](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html#_CPPv415sdmmc_card_initPK12sdmmc_host_tP12sdmmc_card_t "sdmmc_card_init") to initialize the card.

3.  Initialize the ESSL device with [`essl_sdio_config_t`](#_CPPv418essl_sdio_config_t "essl_sdio_config_t"). The `card` member should be the [`sdmmc_card_t`](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/sdmmc.html#_CPPv412sdmmc_card_t "sdmmc_card_t") got in step 2, and the `recv_buffer_size` member should be filled correctly according to pre-negotiated value.

4.  Call [`essl_init()`](#_CPPv49essl_init13essl_handle_t8uint32_t "essl_init") to do initialization of the SDIO part.

5.  Call [`essl_wait_for_ready()`](#_CPPv419essl_wait_for_ready13essl_handle_t8uint32_t "essl_wait_for_ready") to wait for the slave to be ready.


### ESP SPI Slave

Note

If you are communicating with the ESP SDIO Slave device through SPI interface, you should use the [SDIO interface](#essl-sdio-slave-init) instead.

Has not been supported yet.

## APIs

After the initialization process above is performed, you can call the APIs below to make use of the services provided by the slave:

### Tohost Interrupts (Optional)

1.  Call [`essl_get_intr_ena()`](#_CPPv417essl_get_intr_ena13essl_handle_tP8uint32_t8uint32_t "essl_get_intr_ena") to know which events trigger the interrupts to the master.

2.  Call [`essl_set_intr_ena()`](#_CPPv417essl_set_intr_ena13essl_handle_t8uint32_t8uint32_t "essl_set_intr_ena") to set the events that trigger interrupts to the master.

3.  Call [`essl_wait_int()`](#_CPPv413essl_wait_int13essl_handle_t8uint32_t "essl_wait_int") to wait until interrupt from the slave, or timeout.

4.  When interrupt is triggered, call [`essl_get_intr()`](#_CPPv413essl_get_intr13essl_handle_tP8uint32_tP8uint32_t8uint32_t "essl_get_intr") to know which events are active, and call [`essl_clear_intr()`](#_CPPv415essl_clear_intr13essl_handle_t8uint32_t8uint32_t "essl_clear_intr") to clear them.


### Frhost Interrupts

1.  Call [`essl_send_slave_intr()`](#_CPPv420essl_send_slave_intr13essl_handle_t8uint32_t8uint32_t "essl_send_slave_intr") to trigger general purpose interrupt of the slave.


### TX FIFO

1.  Call [`essl_get_tx_buffer_num()`](#_CPPv422essl_get_tx_buffer_num13essl_handle_tP8uint32_t8uint32_t "essl_get_tx_buffer_num") to know how many buffers the slave has prepared to receive data from the master. This is optional. The master will poll `tx_buffer_num` when it tries to send packets to the slave, until the slave has enough buffer or timeout.

2.  Call [`essl_send_packet()`](#_CPPv416essl_send_packet13essl_handle_tPKv6size_t8uint32_t "essl_send_packet") to send data to the slave.


### RX FIFO

1.  Call [`essl_get_rx_data_size()`](#_CPPv421essl_get_rx_data_size13essl_handle_tP8uint32_t8uint32_t "essl_get_rx_data_size") to know how many data the slave has prepared to send to the master. This is optional. When the master tries to receive data from the slave, it updates the `rx_data_size` for once, if the current `rx_data_size` is shorter than the buffer size the master prepared to receive. And it may poll the `rx_data_size` if the `rx_data_size` keeps 0, until timeout.

2.  Call [`essl_get_packet()`](#_CPPv415essl_get_packet13essl_handle_tPv6size_tP6size_t8uint32_t "essl_get_packet") to receive data from the slave.


### Reset Counters (Optional)

Call [`essl_reset_cnt()`](#_CPPv414essl_reset_cnt13essl_handle_t "essl_reset_cnt") to reset the internal counter if you find the slave has reset its counter.

## Application Example

The example below shows how ESP32 SDIO host and slave communicate with each other. The host uses the ESSL SDIO:

[peripherals/sdio](https://github.com/espressif/esp-idf/tree/8fc8f3f4799/examples/peripherals/sdio)

Please refer to the specific example README.md for details.