# esp_ext_part_tables basic example

This example demonstrates how to use an ESP32 to read and parse the Master Boot Record (MBR) from a (micro)SD card. It shows how to extract and display information about the card's partitions, such as their type, size, and starting sector, using the ESP-IDF framework.

## Requirements

- any ESP32 board which supports SPI with a (micro)SD card slot or breakout board connected
- formatted (micro)SD card using MBR (not GPT) as a partition table

## Build and Flash

The example runs on any ESP development board with SD card connected via SPI (use `idf.py menuconfig` -> `Example config` to set used GPIO pins). To build and run the code on e.g. ESP32-S3, use:

```
idf.py set-target esp32s3
idf.py menuconfig
idf.py build flash monitor
```

*NOTE 1*: This example uses SDSPI instead of SDMMC to connect to SD card due to more ESP32 devices supporting it but it doesn't matter which one one you use in your project to load the first sector containing MBR. 

*NOTE 2*: ESP-IDF 5.1 or newer environment must be set properly before running the example.

## Example output

MicroSD card used in the example showcase has capacity 64GiB and was formatted using Windows Disk Management program to two ~32GiB FAT32 partitions as seen in the picture:

![Screenshot of Disk Management Windows program showing removable Disk 0 (a microSD card) containing 2 FAT32 partitions both roughly 30GB is size](/esp_ext_part_tables/examples/basic/assets/two_fat_partitions.png)

The example code parsed the MBR and printed the loaded partition information (`type 4` corresponds to `ESP_EXT_PART_TYPE_FAT32` in `esp_ext_part_type_known_t` enum, etc.). The second task generated MBR from `esp_ext_part_list_t` definition and then parsed it again and printed the output.

```log
I (275) esp_ext_part_tables_example_basic: Example started
I (275) esp_ext_part_tables_example_basic: Starting MBR parsing example task
I (335) esp_ext_part_tables_example_basic: MBR loaded successfully
I (335) esp_ext_part_tables_example_basic: MBR parsed successfully
Partition 0:
        LBA start sector: 2048, address: 1048576,
        sector count: 59392000, size: 30408704000,
        type: FAT32

Partition 1:
        LBA start sector: 59394048, address: 30409752576,
        sector count: 62746624, size: 32126271488,
        type: FAT32

I (365) esp_ext_part_tables_example_basic: Starting MBR generation example task
I (365) esp_ext_part_tables_example_basic: MBR generated successfully
Partition 0:
        LBA start sector: 2048, address: 1048576,
        sector count: 7953, size: 4071936,
        type: FAT12

Partition 1:
        LBA start sector: 10240, address: 5242880,
        sector count: 10240, size: 5242880,
        type: FAT12

I (395) esp_ext_part_tables_example_basic: Example ended
```

Your output will be different based on (micro)SD card used, partitioning and formatting applied.

## Documentation

See the esp_ext_part_tables component's README.md file.
