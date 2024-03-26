# API Reference

## Header files

- [include/esp_serial_slave_link/essl.h](#file-includeesp_serial_slave_linkesslh)
- [include/esp_serial_slave_link/essl_sdio.h](#file-includeesp_serial_slave_linkessl_sdioh)
- [include/esp_serial_slave_link/essl_spi.h](#file-includeesp_serial_slave_linkessl_spih)

## File include/esp_serial_slave_link/essl.h





## Structures and Types

| Type | Name |
| ---: | :--- |
| typedef struct essl\_dev\_t \* | [**essl\_handle\_t**](#typedef-essl_handle_t)  <br>_Handle of an ESSL device._ |

## Functions

| Type | Name |
| ---: | :--- |
|  esp\_err\_t | [**essl\_clear\_intr**](#function-essl_clear_intr) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, uint32\_t intr\_mask, uint32\_t wait\_ms) <br>_Clear interrupt bits of ESSL slave. All the bits set in the mask will be cleared, while other bits will stay the same._ |
|  esp\_err\_t | [**essl\_get\_intr**](#function-essl_get_intr) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, uint32\_t \*intr\_raw, uint32\_t \*intr\_st, uint32\_t wait\_ms) <br>_Get interrupt bits of ESSL slave._ |
|  esp\_err\_t | [**essl\_get\_intr\_ena**](#function-essl_get_intr_ena) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, uint32\_t \*ena\_mask\_o, uint32\_t wait\_ms) <br>_Get interrupt enable bits of ESSL slave._ |
|  esp\_err\_t | [**essl\_get\_packet**](#function-essl_get_packet) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, void \*out\_data, size\_t size, size\_t \*out\_length, uint32\_t wait\_ms) <br>_Get a packet from ESSL slave._ |
|  esp\_err\_t | [**essl\_get\_rx\_data\_size**](#function-essl_get_rx_data_size) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, uint32\_t \*out\_rx\_size, uint32\_t wait\_ms) <br>_Get the size, in bytes, of the data that the ESSL slave is ready to send._ |
|  esp\_err\_t | [**essl\_get\_tx\_buffer\_num**](#function-essl_get_tx_buffer_num) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, uint32\_t \*out\_tx\_num, uint32\_t wait\_ms) <br>_Get buffer num for the host to send data to the slave. The buffers are size of_ `buffer_size`_._ |
|  esp\_err\_t | [**essl\_init**](#function-essl_init) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, uint32\_t wait\_ms) <br>_Initialize the slave._ |
|  esp\_err\_t | [**essl\_read\_reg**](#function-essl_read_reg) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, uint8\_t add, uint8\_t \*value\_o, uint32\_t wait\_ms) <br>_Read general purpose R/W registers (8-bit) of ESSL slave._ |
|  esp\_err\_t | [**essl\_reset\_cnt**](#function-essl_reset_cnt) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle) <br>_Reset the counters of this component. Usually you don't need to do this unless you know the slave is reset._ |
|  esp\_err\_t | [**essl\_send\_packet**](#function-essl_send_packet) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, const void \*start, size\_t length, uint32\_t wait\_ms) <br>_Send a packet to the ESSL Slave. The Slave receives the packet into buffers whose size is_ `buffer_size`_ (configured during initialization)._ |
|  esp\_err\_t | [**essl\_send\_slave\_intr**](#function-essl_send_slave_intr) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, uint32\_t intr\_mask, uint32\_t wait\_ms) <br>_Send interrupts to slave. Each bit of the interrupt will be triggered._ |
|  esp\_err\_t | [**essl\_set\_intr\_ena**](#function-essl_set_intr_ena) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, uint32\_t ena\_mask, uint32\_t wait\_ms) <br>_Set interrupt enable bits of ESSL slave. The slave only sends interrupt on the line when there is a bit both the raw status and the enable are set._ |
|  esp\_err\_t | [**essl\_wait\_for\_ready**](#function-essl_wait_for_ready) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, uint32\_t wait\_ms) <br>_Wait for interrupt of an ESSL slave device._ |
|  esp\_err\_t | [**essl\_wait\_int**](#function-essl_wait_int) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, uint32\_t wait\_ms) <br>_wait for an interrupt of the slave_ |
|  esp\_err\_t | [**essl\_write\_reg**](#function-essl_write_reg) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle, uint8\_t addr, uint8\_t value, uint8\_t \*value\_o, uint32\_t wait\_ms) <br>_Write general purpose R/W registers (8-bit) of ESSL slave._ |


## Structures and Types Documentation

### typedef `essl_handle_t`

_Handle of an ESSL device._
```c
typedef struct essl_dev_t* essl_handle_t;
```


## Functions Documentation

### function `essl_clear_intr`

_Clear interrupt bits of ESSL slave. All the bits set in the mask will be cleared, while other bits will stay the same._
```c
esp_err_t essl_clear_intr (
    essl_handle_t handle,
    uint32_t intr_mask,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of an ESSL device. 
* `intr_mask` Mask of interrupt bits to clear. 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9.


**Returns:**



* ESP\_OK: Success
* ESP\_ERR\_NOT\_SUPPORTED: Current device does not support this function.
* One of the error codes from SDMMC host controller
### function `essl_get_intr`

_Get interrupt bits of ESSL slave._
```c
esp_err_t essl_get_intr (
    essl_handle_t handle,
    uint32_t *intr_raw,
    uint32_t *intr_st,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of an ESSL device. 
* `intr_raw` Output of the raw interrupt bits. Set to NULL if only masked bits are read. 
* `intr_st` Output of the masked interrupt bits. set to NULL if only raw bits are read. 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9.


**Returns:**



* ESP\_OK: Success
* ESP\_INVALID\_ARG: If both `intr_raw` and`intr_st` are NULL.
* ESP\_ERR\_NOT\_SUPPORTED: Current device does not support this function.
* One of the error codes from SDMMC host controller
### function `essl_get_intr_ena`

_Get interrupt enable bits of ESSL slave._
```c
esp_err_t essl_get_intr_ena (
    essl_handle_t handle,
    uint32_t *ena_mask_o,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of an ESSL device. 
* `ena_mask_o` Output of interrupt bit enable mask. 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9.


**Returns:**



* ESP\_OK Success
* One of the error codes from SDMMC host controller
### function `essl_get_packet`

_Get a packet from ESSL slave._
```c
esp_err_t essl_get_packet (
    essl_handle_t handle,
    void *out_data,
    size_t size,
    size_t *out_length,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of an ESSL device. 
* `out_data` Data output address 
* `size` The size of the output buffer, if the buffer is smaller than the size of data to receive from slave, the driver returns `ESP_ERR_NOT_FINISHED`
* `out_length` Output of length the data actually received from slave. 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9.


**Returns:**



* ESP\_OK Success: All the data has been read from the slave.
* ESP\_ERR\_INVALID\_ARG: Invalid argument, The handle is not initialized or the other arguments are invalid.
* ESP\_ERR\_NOT\_FINISHED: Read was successful, but there is still data remaining.
* ESP\_ERR\_NOT\_FOUND: Slave is not ready to send data.
* ESP\_ERR\_NOT\_SUPPORTED: This API is not supported in this mode
* One of the error codes from SDMMC/SPI host controller.
### function `essl_get_rx_data_size`

_Get the size, in bytes, of the data that the ESSL slave is ready to send._
```c
esp_err_t essl_get_rx_data_size (
    essl_handle_t handle,
    uint32_t *out_rx_size,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of an ESSL device. 
* `out_rx_size` Output of data size to read from slave, in bytes 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9.


**Returns:**



* ESP\_OK: Success
* ESP\_ERR\_NOT\_SUPPORTED: This API is not supported in this mode
* One of the error codes from SDMMC/SPI host controller
### function `essl_get_tx_buffer_num`

_Get buffer num for the host to send data to the slave. The buffers are size of_ `buffer_size`_._
```c
esp_err_t essl_get_tx_buffer_num (
    essl_handle_t handle,
    uint32_t *out_tx_num,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of a ESSL device. 
* `out_tx_num` Output of buffer num that host can send data to ESSL slave. 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9.


**Returns:**



* ESP\_OK: Success
* ESP\_ERR\_NOT\_SUPPORTED: This API is not supported in this mode
* One of the error codes from SDMMC/SPI host controller
### function `essl_init`

_Initialize the slave._
```c
esp_err_t essl_init (
    essl_handle_t handle,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of an ESSL device. 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9. 


**Returns:**



* ESP\_OK: If success
* ESP\_ERR\_NOT\_SUPPORTED: Current device does not support this function.
* Other value returned from lower layer `init`.
### function `essl_read_reg`

_Read general purpose R/W registers (8-bit) of ESSL slave._
```c
esp_err_t essl_read_reg (
    essl_handle_t handle,
    uint8_t add,
    uint8_t *value_o,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of a `essl` device.
* `add` Address of register to read. For SDIO, Valid address: 0-27, 32-63 (28-31 reserved, return interrupt bits on read). For SPI, see `essl_spi.h`
* `value_o` Output value read from the register. 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9.


**Returns:**



* ESP\_OK Success
* One of the error codes from SDMMC/SPI host controller
### function `essl_reset_cnt`

_Reset the counters of this component. Usually you don't need to do this unless you know the slave is reset._
```c
esp_err_t essl_reset_cnt (
    essl_handle_t handle
) 
```


**Parameters:**


* `handle` Handle of an ESSL device.


**Returns:**



* ESP\_OK: Success
* ESP\_ERR\_NOT\_SUPPORTED: This API is not supported in this mode
* ESP\_ERR\_INVALID\_ARG: Invalid argument, handle is not init.
### function `essl_send_packet`

_Send a packet to the ESSL Slave. The Slave receives the packet into buffers whose size is_ `buffer_size`_ (configured during initialization)._
```c
esp_err_t essl_send_packet (
    essl_handle_t handle,
    const void *start,
    size_t length,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of an ESSL device. 
* `start` Start address of the packet to send 
* `length` Length of data to send, if the packet is over-size, the it will be divided into blocks and hold into different buffers automatically. 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9.


**Returns:**



* ESP\_OK Success
* ESP\_ERR\_INVALID\_ARG: Invalid argument, handle is not init or other argument is not valid.
* ESP\_ERR\_TIMEOUT: No buffer to use, or error ftrom SDMMC host controller.
* ESP\_ERR\_NOT\_FOUND: Slave is not ready for receiving.
* ESP\_ERR\_NOT\_SUPPORTED: This API is not supported in this mode
* One of the error codes from SDMMC/SPI host controller.
### function `essl_send_slave_intr`

_Send interrupts to slave. Each bit of the interrupt will be triggered._
```c
esp_err_t essl_send_slave_intr (
    essl_handle_t handle,
    uint32_t intr_mask,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of an ESSL device. 
* `intr_mask` Mask of interrupt bits to send to slave. 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9.


**Returns:**



* ESP\_OK: Success
* ESP\_ERR\_NOT\_SUPPORTED: Current device does not support this function.
* One of the error codes from SDMMC host controller
### function `essl_set_intr_ena`

_Set interrupt enable bits of ESSL slave. The slave only sends interrupt on the line when there is a bit both the raw status and the enable are set._
```c
esp_err_t essl_set_intr_ena (
    essl_handle_t handle,
    uint32_t ena_mask,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of an ESSL device. 
* `ena_mask` Mask of the interrupt bits to enable. 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9.


**Returns:**



* ESP\_OK: Success
* ESP\_ERR\_NOT\_SUPPORTED: Current device does not support this function.
* One of the error codes from SDMMC host controller
### function `essl_wait_for_ready`

_Wait for interrupt of an ESSL slave device._
```c
esp_err_t essl_wait_for_ready (
    essl_handle_t handle,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of an ESSL device. 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9.


**Returns:**



* ESP\_OK: If success
* ESP\_ERR\_NOT\_SUPPORTED: Current device does not support this function.
* One of the error codes from SDMMC host controller
### function `essl_wait_int`

_wait for an interrupt of the slave_
```c
esp_err_t essl_wait_int (
    essl_handle_t handle,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of an ESSL device. 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9.


**Returns:**



* ESP\_OK: If interrupt is triggered.
* ESP\_ERR\_NOT\_SUPPORTED: Current device does not support this function.
* ESP\_ERR\_TIMEOUT: No interrupts before timeout.
### function `essl_write_reg`

_Write general purpose R/W registers (8-bit) of ESSL slave._
```c
esp_err_t essl_write_reg (
    essl_handle_t handle,
    uint8_t addr,
    uint8_t value,
    uint8_t *value_o,
    uint32_t wait_ms
) 
```


**Parameters:**


* `handle` Handle of an ESSL device. 
* `addr` Address of register to write. For SDIO, valid address: 0-59. For SPI, see `essl_spi.h`
* `value` Value to write to the register. 
* `value_o` Output of the returned written value. 
* `wait_ms` Millisecond to wait before timeout, will not wait at all if set to 0-9.


**Note:**

sdio 28-31 are reserved, the lower API helps to skip.



**Returns:**



* ESP\_OK Success
* One of the error codes from SDMMC/SPI host controller


## File include/esp_serial_slave_link/essl_sdio.h





## Structures and Types

| Type | Name |
| ---: | :--- |
| struct | [**essl\_sdio\_config\_t**](#struct-essl_sdio_config_t) <br>_Configuration for the ESSL SDIO device._ |

## Functions

| Type | Name |
| ---: | :--- |
|  esp\_err\_t | [**essl\_sdio\_deinit\_dev**](#function-essl_sdio_deinit_dev) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle) <br>_Deinitialize and free the space used by the ESSL SDIO device._ |
|  esp\_err\_t | [**essl\_sdio\_init\_dev**](#function-essl_sdio_init_dev) ([**essl\_handle\_t**](#typedef-essl_handle_t) \*out\_handle, const [**essl\_sdio\_config\_t**](#struct-essl_sdio_config_t) \*config) <br>_Initialize the ESSL SDIO device and get its handle._ |


## Structures and Types Documentation

### struct `essl_sdio_config_t`

_Configuration for the ESSL SDIO device._

Variables:

-  sdmmc\_card\_t \* card  <br>_The initialized sdmmc card pointer of the slave._

-  int recv_buffer_size  <br>_The pre-negotiated recv buffer size used by both the host and the slave._


## Functions Documentation

### function `essl_sdio_deinit_dev`

_Deinitialize and free the space used by the ESSL SDIO device._
```c
esp_err_t essl_sdio_deinit_dev (
    essl_handle_t handle
) 
```


**Parameters:**


* `handle` Handle of the ESSL SDIO device to deinit. 


**Returns:**



* ESP\_OK: on success
* ESP\_ERR\_INVALID\_ARG: wrong handle passed
### function `essl_sdio_init_dev`

_Initialize the ESSL SDIO device and get its handle._
```c
esp_err_t essl_sdio_init_dev (
    essl_handle_t *out_handle,
    const essl_sdio_config_t *config
) 
```


**Parameters:**


* `out_handle` Output of the handle. 
* `config` Configuration for the ESSL SDIO device. 


**Returns:**



* ESP\_OK: on success
* ESP\_ERR\_NO\_MEM: memory exhausted.


## File include/esp_serial_slave_link/essl_spi.h





## Structures and Types

| Type | Name |
| ---: | :--- |
| struct | [**essl\_spi\_config\_t**](#struct-essl_spi_config_t) <br>_Configuration of ESSL SPI device._ |

## Functions

| Type | Name |
| ---: | :--- |
|  esp\_err\_t | [**essl\_spi\_deinit\_dev**](#function-essl_spi_deinit_dev) ([**essl\_handle\_t**](#typedef-essl_handle_t) handle) <br>_Deinitialize the ESSL SPI device and free the memory used by the device._ |
|  esp\_err\_t | [**essl\_spi\_get\_packet**](#function-essl_spi_get_packet) (void \*arg, void \*out\_data, size\_t size, uint32\_t wait\_ms) <br>_Get a packet from Slave._ |
|  esp\_err\_t | [**essl\_spi\_init\_dev**](#function-essl_spi_init_dev) ([**essl\_handle\_t**](#typedef-essl_handle_t) \*out\_handle, const [**essl\_spi\_config\_t**](#struct-essl_spi_config_t) \*init\_config) <br>_Initialize the ESSL SPI device function list and get its handle._ |
|  esp\_err\_t | [**essl\_spi\_rdbuf**](#function-essl_spi_rdbuf) (spi\_device\_handle\_t spi, uint8\_t \*out\_data, int addr, int len, uint32\_t flags) <br>_Read the shared buffer from the slave in ISR way._ |
|  esp\_err\_t | [**essl\_spi\_rdbuf\_polling**](#function-essl_spi_rdbuf_polling) (spi\_device\_handle\_t spi, uint8\_t \*out\_data, int addr, int len, uint32\_t flags) <br>_Read the shared buffer from the slave in polling way._ |
|  esp\_err\_t | [**essl\_spi\_rddma**](#function-essl_spi_rddma) (spi\_device\_handle\_t spi, uint8\_t \*out\_data, int len, int seg\_len, uint32\_t flags) <br>_Receive long buffer in segments from the slave through its DMA._ |
|  esp\_err\_t | [**essl\_spi\_rddma\_done**](#function-essl_spi_rddma_done) (spi\_device\_handle\_t spi, uint32\_t flags) <br>_Send the_ `rddma_done`_ command to the slave. Upon receiving this command, the slave will stop sending the current buffer even there are data unsent, and maybe prepare the next buffer to send._ |
|  esp\_err\_t | [**essl\_spi\_rddma\_seg**](#function-essl_spi_rddma_seg) (spi\_device\_handle\_t spi, uint8\_t \*out\_data, int seg\_len, uint32\_t flags) <br>_Read one data segment from the slave through its DMA._ |
|  esp\_err\_t | [**essl\_spi\_read\_reg**](#function-essl_spi_read_reg) (void \*arg, uint8\_t addr, uint8\_t \*out\_value, uint32\_t wait\_ms) <br>_Read from the shared registers._ |
|  void | [**essl\_spi\_reset\_cnt**](#function-essl_spi_reset_cnt) (void \*arg) <br>_Reset the counter in Master context._ |
|  esp\_err\_t | [**essl\_spi\_send\_packet**](#function-essl_spi_send_packet) (void \*arg, const void \*data, size\_t size, uint32\_t wait\_ms) <br>_Send a packet to Slave._ |
|  esp\_err\_t | [**essl\_spi\_wrbuf**](#function-essl_spi_wrbuf) (spi\_device\_handle\_t spi, const uint8\_t \*data, int addr, int len, uint32\_t flags) <br>_Write the shared buffer of the slave in ISR way._ |
|  esp\_err\_t | [**essl\_spi\_wrbuf\_polling**](#function-essl_spi_wrbuf_polling) (spi\_device\_handle\_t spi, const uint8\_t \*data, int addr, int len, uint32\_t flags) <br>_Write the shared buffer of the slave in polling way._ |
|  esp\_err\_t | [**essl\_spi\_wrdma**](#function-essl_spi_wrdma) (spi\_device\_handle\_t spi, const uint8\_t \*data, int len, int seg\_len, uint32\_t flags) <br>_Send long buffer in segments to the slave through its DMA._ |
|  esp\_err\_t | [**essl\_spi\_wrdma\_done**](#function-essl_spi_wrdma_done) (spi\_device\_handle\_t spi, uint32\_t flags) <br>_Send the_ `wrdma_done`_ command to the slave. Upon receiving this command, the slave will stop receiving, process the received data, and maybe prepare the next buffer to receive._ |
|  esp\_err\_t | [**essl\_spi\_wrdma\_seg**](#function-essl_spi_wrdma_seg) (spi\_device\_handle\_t spi, const uint8\_t \*data, int seg\_len, uint32\_t flags) <br>_Send one data segment to the slave through its DMA._ |
|  esp\_err\_t | [**essl\_spi\_write\_reg**](#function-essl_spi_write_reg) (void \*arg, uint8\_t addr, uint8\_t value, uint8\_t \*out\_value, uint32\_t wait\_ms) <br>_Write to the shared registers._ |


## Structures and Types Documentation

### struct `essl_spi_config_t`

_Configuration of ESSL SPI device._

Variables:

-  uint8\_t rx_sync_reg  <br>_The pre-negotiated register ID for Master-RX-Slave-TX synchronization. 1 word (4 Bytes) will be reserved for the synchronization._

-  spi\_device\_handle\_t \* spi  <br>_Pointer to SPI device handle._

-  uint32\_t tx_buf_size  <br>_The pre-negotiated Master TX buffer size used by both the host and the slave._

-  uint8\_t tx_sync_reg  <br>_The pre-negotiated register ID for Master-TX-SLAVE-RX synchronization. 1 word (4 Bytes) will be reserved for the synchronization._


## Functions Documentation

### function `essl_spi_deinit_dev`

_Deinitialize the ESSL SPI device and free the memory used by the device._
```c
esp_err_t essl_spi_deinit_dev (
    essl_handle_t handle
) 
```


**Parameters:**


* `handle` Handle of the ESSL SPI device 


**Returns:**



* ESP\_OK: On success
* ESP\_ERR\_INVALID\_STATE: ESSL SPI is not in use
### function `essl_spi_get_packet`

_Get a packet from Slave._
```c
esp_err_t essl_spi_get_packet (
    void *arg,
    void *out_data,
    size_t size,
    uint32_t wait_ms
) 
```


**Parameters:**


* `arg` Context of the component. (Member `arg` from`essl_handle_t`)
* `out_data` Output data address 
* `size` The size of the output data. 
* `wait_ms` Time to wait before timeout (reserved for future use, user should set this to 0). 


**Returns:**



* ESP\_OK: On Success
* ESP\_ERR\_INVALID\_STATE: ESSL SPI has not been initialized.
* ESP\_ERR\_INVALID\_ARG: The output data address is neither DMA capable nor 4 byte-aligned
* ESP\_ERR\_INVALID\_SIZE: Master requires `size` bytes of data but Slave did not load enough bytes.
### function `essl_spi_init_dev`

_Initialize the ESSL SPI device function list and get its handle._
```c
esp_err_t essl_spi_init_dev (
    essl_handle_t *out_handle,
    const essl_spi_config_t *init_config
) 
```


**Parameters:**


* `out_handle` Output of the handle 
* `init_config` Configuration for the ESSL SPI device 


**Returns:**



* ESP\_OK: On success
* ESP\_ERR\_NO\_MEM: Memory exhausted
* ESP\_ERR\_INVALID\_STATE: SPI driver is not initialized
* ESP\_ERR\_INVALID\_ARG: Wrong register ID
### function `essl_spi_rdbuf`

_Read the shared buffer from the slave in ISR way._
```c
esp_err_t essl_spi_rdbuf (
    spi_device_handle_t spi,
    uint8_t *out_data,
    int addr,
    int len,
    uint32_t flags
) 
```


**Note:**

The slave's HW doesn't guarantee the data in one SPI transaction is consistent. It sends data in unit of byte. In other words, if the slave SW attempts to update the shared register when a rdbuf SPI transaction is in-flight, the data got by the master will be the combination of bytes of different writes of slave SW.



**Note:**

`out_data` should be prepared in words and in the DRAM. The buffer may be written in words by the DMA. When a byte is written, the remaining bytes in the same word will also be overwritten, even the`len` is shorter than a word.



**Parameters:**


* `spi` SPI device handle representing the slave 
* `out_data` Buffer for read data, strongly suggested to be in the DRAM and aligned to 4 
* `addr` Address of the slave shared buffer 
* `len` Length to read 
* `flags` `SPI_TRANS_*` flags to control the transaction mode of the transaction to send.


**Returns:**



* ESP\_OK: on success
* or other return value from :cpp:func:`spi_device_transmit`.
### function `essl_spi_rdbuf_polling`

_Read the shared buffer from the slave in polling way._
```c
esp_err_t essl_spi_rdbuf_polling (
    spi_device_handle_t spi,
    uint8_t *out_data,
    int addr,
    int len,
    uint32_t flags
) 
```


**Note:**

`out_data` should be prepared in words and in the DRAM. The buffer may be written in words by the DMA. When a byte is written, the remaining bytes in the same word will also be overwritten, even the`len` is shorter than a word.



**Parameters:**


* `spi` SPI device handle representing the slave 
* `out_data` Buffer for read data, strongly suggested to be in the DRAM and aligned to 4 
* `addr` Address of the slave shared buffer 
* `len` Length to read 
* `flags` `SPI_TRANS_*` flags to control the transaction mode of the transaction to send.


**Returns:**



* ESP\_OK: on success
* or other return value from :cpp:func:`spi_device_transmit`.
### function `essl_spi_rddma`

_Receive long buffer in segments from the slave through its DMA._
```c
esp_err_t essl_spi_rddma (
    spi_device_handle_t spi,
    uint8_t *out_data,
    int len,
    int seg_len,
    uint32_t flags
) 
```


**Note:**

This function combines several :cpp:func:`essl_spi_rddma_seg` and one :cpp:func:`essl_spi_rddma_done` at the end. Used when the slave is working in segment mode.



**Parameters:**


* `spi` SPI device handle representing the slave 
* `out_data` Buffer to hold the received data, strongly suggested to be in the DRAM and aligned to 4 
* `len` Total length of data to receive. 
* `seg_len` Length of each segment, which is not larger than the maximum transaction length allowed for the spi device. Suggested to be multiples of 4. When set &lt; 0, means send all data in one segment (the `rddma_done` will still be sent.)
* `flags` `SPI_TRANS_*` flags to control the transaction mode of the transaction to send.


**Returns:**



* ESP\_OK: success
* or other return value from :cpp:func:`spi_device_transmit`.
### function `essl_spi_rddma_done`

_Send the_ `rddma_done`_ command to the slave. Upon receiving this command, the slave will stop sending the current buffer even there are data unsent, and maybe prepare the next buffer to send._
```c
esp_err_t essl_spi_rddma_done (
    spi_device_handle_t spi,
    uint32_t flags
) 
```


**Note:**

This is required only when the slave is working in segment mode.



**Parameters:**


* `spi` SPI device handle representing the slave 
* `flags` `SPI_TRANS_*` flags to control the transaction mode of the transaction to send.


**Returns:**



* ESP\_OK: success
* or other return value from :cpp:func:`spi_device_transmit`.
### function `essl_spi_rddma_seg`

_Read one data segment from the slave through its DMA._
```c
esp_err_t essl_spi_rddma_seg (
    spi_device_handle_t spi,
    uint8_t *out_data,
    int seg_len,
    uint32_t flags
) 
```


**Note:**

To read long buffer, call :cpp:func:`essl_spi_rddma` instead.



**Parameters:**


* `spi` SPI device handle representing the slave 
* `out_data` Buffer to hold the received data. strongly suggested to be in the DRAM and aligned to 4 
* `seg_len` Length of this segment 
* `flags` `SPI_TRANS_*` flags to control the transaction mode of the transaction to send.


**Returns:**



* ESP\_OK: success
* or other return value from :cpp:func:`spi_device_transmit`.
### function `essl_spi_read_reg`

_Read from the shared registers._
```c
esp_err_t essl_spi_read_reg (
    void *arg,
    uint8_t addr,
    uint8_t *out_value,
    uint32_t wait_ms
) 
```


**Note:**

The registers for Master/Slave synchronization are reserved. Do not use them. (see `rx_sync_reg` in`essl_spi_config_t`)



**Parameters:**


* `arg` Context of the component. (Member `arg` from`essl_handle_t`)
* `addr` Address of the shared registers. (Valid: 0 ~ SOC\_SPI\_MAXIMUM\_BUFFER\_SIZE, registers for M/S sync are reserved, see note1). 
* `out_value` Read buffer for the shared registers. 
* `wait_ms` Time to wait before timeout (reserved for future use, user should set this to 0). 


**Returns:**



* ESP\_OK: success
* ESP\_ERR\_INVALID\_STATE: ESSL SPI has not been initialized.
* ESP\_ERR\_INVALID\_ARG: The address argument is not valid. See note 1.
* or other return value from :cpp:func:`spi_device_transmit`.
### function `essl_spi_reset_cnt`

_Reset the counter in Master context._
```c
void essl_spi_reset_cnt (
    void *arg
) 
```


**Note:**

Shall only be called if the slave has reset its counter. Else, Slave and Master would be desynchronized



**Parameters:**


* `arg` Context of the component. (Member `arg` from`essl_handle_t`)
### function `essl_spi_send_packet`

_Send a packet to Slave._
```c
esp_err_t essl_spi_send_packet (
    void *arg,
    const void *data,
    size_t size,
    uint32_t wait_ms
) 
```


**Parameters:**


* `arg` Context of the component. (Member `arg` from`essl_handle_t`)
* `data` Address of the data to send 
* `size` Size of the data to send. 
* `wait_ms` Time to wait before timeout (reserved for future use, user should set this to 0). 


**Returns:**



* ESP\_OK: On success
* ESP\_ERR\_INVALID\_STATE: ESSL SPI has not been initialized.
* ESP\_ERR\_INVALID\_ARG: The data address is not DMA capable
* ESP\_ERR\_INVALID\_SIZE: Master will send `size` bytes of data but Slave did not load enough RX buffer
### function `essl_spi_wrbuf`

_Write the shared buffer of the slave in ISR way._
```c
esp_err_t essl_spi_wrbuf (
    spi_device_handle_t spi,
    const uint8_t *data,
    int addr,
    int len,
    uint32_t flags
) 
```


**Note:**

`out_data` should be prepared in words and in the DRAM. The buffer may be written in words by the DMA. When a byte is written, the remaining bytes in the same word will also be overwritten, even the`len` is shorter than a word.



**Parameters:**


* `spi` SPI device handle representing the slave 
* `data` Buffer for data to send, strongly suggested to be in the DRAM 
* `addr` Address of the slave shared buffer, 
* `len` Length to write 
* `flags` `SPI_TRANS_*` flags to control the transaction mode of the transaction to send.


**Returns:**



* ESP\_OK: success
* or other return value from :cpp:func:`spi_device_transmit`.
### function `essl_spi_wrbuf_polling`

_Write the shared buffer of the slave in polling way._
```c
esp_err_t essl_spi_wrbuf_polling (
    spi_device_handle_t spi,
    const uint8_t *data,
    int addr,
    int len,
    uint32_t flags
) 
```


**Note:**

`out_data` should be prepared in words and in the DRAM. The buffer may be written in words by the DMA. When a byte is written, the remaining bytes in the same word will also be overwritten, even the`len` is shorter than a word.



**Parameters:**


* `spi` SPI device handle representing the slave 
* `data` Buffer for data to send, strongly suggested to be in the DRAM 
* `addr` Address of the slave shared buffer, 
* `len` Length to write 
* `flags` `SPI_TRANS_*` flags to control the transaction mode of the transaction to send.


**Returns:**



* ESP\_OK: success
* or other return value from :cpp:func:`spi_device_polling_transmit`.
### function `essl_spi_wrdma`

_Send long buffer in segments to the slave through its DMA._
```c
esp_err_t essl_spi_wrdma (
    spi_device_handle_t spi,
    const uint8_t *data,
    int len,
    int seg_len,
    uint32_t flags
) 
```


**Note:**

This function combines several :cpp:func:`essl_spi_wrdma_seg` and one :cpp:func:`essl_spi_wrdma_done` at the end. Used when the slave is working in segment mode.



**Parameters:**


* `spi` SPI device handle representing the slave 
* `data` Buffer for data to send, strongly suggested to be in the DRAM 
* `len` Total length of data to send. 
* `seg_len` Length of each segment, which is not larger than the maximum transaction length allowed for the spi device. Suggested to be multiples of 4. When set &lt; 0, means send all data in one segment (the `wrdma_done` will still be sent.)
* `flags` `SPI_TRANS_*` flags to control the transaction mode of the transaction to send.


**Returns:**



* ESP\_OK: success
* or other return value from :cpp:func:`spi_device_transmit`.
### function `essl_spi_wrdma_done`

_Send the_ `wrdma_done`_ command to the slave. Upon receiving this command, the slave will stop receiving, process the received data, and maybe prepare the next buffer to receive._
```c
esp_err_t essl_spi_wrdma_done (
    spi_device_handle_t spi,
    uint32_t flags
) 
```


**Note:**

This is required only when the slave is working in segment mode.



**Parameters:**


* `spi` SPI device handle representing the slave 
* `flags` `SPI_TRANS_*` flags to control the transaction mode of the transaction to send.


**Returns:**



* ESP\_OK: success
* or other return value from :cpp:func:`spi_device_transmit`.
### function `essl_spi_wrdma_seg`

_Send one data segment to the slave through its DMA._
```c
esp_err_t essl_spi_wrdma_seg (
    spi_device_handle_t spi,
    const uint8_t *data,
    int seg_len,
    uint32_t flags
) 
```


**Note:**

To send long buffer, call :cpp:func:`essl_spi_wrdma` instead.



**Parameters:**


* `spi` SPI device handle representing the slave 
* `data` Buffer for data to send, strongly suggested to be in the DRAM 
* `seg_len` Length of this segment 
* `flags` `SPI_TRANS_*` flags to control the transaction mode of the transaction to send.


**Returns:**



* ESP\_OK: success
* or other return value from :cpp:func:`spi_device_transmit`.
### function `essl_spi_write_reg`

_Write to the shared registers._
```c
esp_err_t essl_spi_write_reg (
    void *arg,
    uint8_t addr,
    uint8_t value,
    uint8_t *out_value,
    uint32_t wait_ms
) 
```


**Note:**

The registers for Master/Slave synchronization are reserved. Do not use them. (see `tx_sync_reg` in`essl_spi_config_t`)



**Note:**

Feature of checking the actual written value (`out_value`) is not supported.



**Parameters:**


* `arg` Context of the component. (Member `arg` from`essl_handle_t`)
* `addr` Address of the shared registers. (Valid: 0 ~ SOC\_SPI\_MAXIMUM\_BUFFER\_SIZE, registers for M/S sync are reserved, see note1) 
* `value` Buffer for data to send, should be align to 4. 
* `out_value` Not supported, should be set to NULL. 
* `wait_ms` Time to wait before timeout (reserved for future use, user should set this to 0). 


**Returns:**



* ESP\_OK: success
* ESP\_ERR\_INVALID\_STATE: ESSL SPI has not been initialized.
* ESP\_ERR\_INVALID\_ARG: The address argument is not valid. See note 1.
* ESP\_ERR\_NOT\_SUPPORTED: Should set `out_value` to NULL. See note 2.
* or other return value from :cpp:func:`spi_device_transmit`.


