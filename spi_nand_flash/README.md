# SPI NAND Flash Driver

This driver is designed to support SPI nand flash with ESP chipsets.

## About SPI NAND Flash
SPI NAND Flash combines the benefits of NAND Flash technology with the simplicity of the SPI interface, providing an efficient and cost-effective solution for non-volatile data storage in diverse applications. Its versatility, reliability, and affordability make it a popular choice for many embedded systems and electronic devices.

### Key Features:
* Non-Volatile Storage: SPI NAND Flash provides non-volatile storage, retaining data even when power is removed. This characteristic makes it ideal for storing critical system information and application data.

* SPI Interface: The SPI protocol allows for straightforward communication between the microcontroller and the NAND Flash. This simplicity in interface design facilitates easy integration into embedded systems.

* Cost-Effective: SPI NAND Flash offers a cost-effective storage solution, making it attractive for applications with budget constraints. Its competitive pricing makes it a viable option for a wide range of projects.

* High Density: NAND Flash technology inherently supports high-density storage, enabling the storage of large amounts of data in a compact form factor. This is advantageous for applications requiring extensive data storage in constrained spaces.

* Fast Read/Write Operations: The SPI interface enables reasonably fast read and write operations, making it suitable for applications where data access speed is crucial.

## Example

Get started with demo example: [nand_flash](https://github.com/espressif/esp-idf/tree/master/examples/storage/nand_flash)

## Supported SPI NAND Flash chips

At present, ESP-IDF is compatible with the chips produced by following SPI NAND flash manufacturer:

* Winbond
* Gigadevice
* Alliance