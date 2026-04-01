| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ----------------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# SPI NAND Flash debug example

This example is designed to gather diagnostic statistics for NAND flash, as outlined below:

1. Bad block statistics.
2. ECC error statistics.
3. Read-write NAND page throughput (both at the lower level and through the Dhara library).
4. Verification of NAND write operations using the Kconfig option `CONFIG_NAND_FLASH_VERIFY_WRITE`.

## How to use example

To run the example, type the following command:

```CMake
# CMake
idf.py -p PORT flash monitor
```

(To exit the serial monitor, type ``Ctrl-]``.)

See the Getting Started Guide for full steps to configure and use ESP-IDF to build projects.

## Example output

Here is the example's console output:
```
...
I (353) debug_app: Get bad block statistics:
I (533) nand_diag:
Total number of Blocks: 1024
Bad Blocks: 1
Valid Blocks: 1023

I (533) debug_app: Read-Write Throughput via Dhara:
I (3423) debug_app: Wrote 2048000 bytes in 2269005 us, avg 902.60 kB/s
I (3423) debug_app: Read 2048000 bytes in 617570 us, avg 3316.22 kB/s

I (3423) debug_app: Read-Write Throughput at lower level (bypassing Dhara):
I (5913) debug_app: Wrote 2048000 bytes in 1883853 us, avg 1087.13 kB/s
I (5913) debug_app: Read 2048000 bytes in 585556 us, avg 3497.53 kB/s

I (5913) debug_app: ECC errors statistics:
I (17173) nand_diag:
Total number of ECC errors: 42
ECC not corrected count: 0
ECC errors exceeding threshold (4): 0
...
```
