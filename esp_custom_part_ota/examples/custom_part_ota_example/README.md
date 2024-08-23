# Custom Data Partition OTA

Custom Data Partition OTA aims at enabling Over-the-Air data update for custom data partitions in ESP-IDF.

## ESP Custom Partition OTA Abstraction Layer

This example uses `esp_custom_part_ota` component hosted at [idf-extra-components/esp_custom_part_ota](https://github.com/espressif/idf-extra-components/blob/master/esp_custom_part_ota) and available though the [IDF component manager](https://components.espressif.com/component/espressif/esp_custom_part_ota).

Please refer to its documentation [here](https://github.com/espressif/idf-extra-components/blob/master/esp_custom_part_ota/README.md) for more details.


## How to use the example

### Configure the project
Open the project configuration menu(`idf.py menuconfig`)

In the `Example Connection Configuration` menu:
* Choose the network interface in the `Connect using` option based on your board. Currently both Wi-Fi and Ethernet are supported
* If the Wi-Fi interface is used, provide the Wi-Fi SSID and password of the AP you wish to connect to
* If using the Ethernet interface, set the PHY model under `Ethernet PHY Device` option, e.g. `IP101`

In the `Example Configuration` menu:
* Set the URL of the firmware to download in the `Firmware Upgrade URL` option. The format should be `https://<host-ip-address>:<host-port>/<filename>`, e.g. `https://192.168.2.106:8070/new_data.bin`
* Enable the `Backup the Update Partition` option to backup the contents of the partition to be updated.

### Build and Flash example

```
idf.py build flash
```
