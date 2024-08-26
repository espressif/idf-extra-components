# ESP Non-App OTA Updates

This component provides an API interface to update the Non-App type partitions using ESP-IDF.

Currently ESP-IDF only supports the OTA updates for partitions of subtype `app`. This component can be used to facilitate the OTA updates of partitions of subtypes other than `app`.

## Workflow

`esp_custom_part_ota` component supports OTA update using two schemes custom_part_ota_backup and custom_part_ota:

### custom_part_ota_backup

Consider that the device has the partition table as shown below: 

```
# Name,     Type,  SubType, Offset,   Size
  nvs,      data,  nvs,     0x9000,   0x6000, 
  otadata,  data,  ota,           ,   0x2000, 
  phy_init, data,  phy,     0xf000,   0x1000, 
  ota_0,    app,   ota_0,         ,   0x100000, 
  ota_1,    app,   ota_1,         ,   0x100000, 
  storage,  data,  spiffs,        ,   0x100000, 
```

Here the non-app partition which we are trying to update is spiffs. When the user wants to update the spiffs partition, the workflow will be as follows: 

* Consider that ota_0 is the current running partition. 
* Copy the data from the spiffs partition to the backup partition (backup partition will be always ota passive partition).
* Once the data is copied, download the new data, and write it into the spiffs partition. 
* This could result in two cases: 
  1. Data is downloaded and written successfully: End the OTA update process. 
  2. Error occurred during writing the data or power is lost during the OTA process: In such cases, we will have a copy of the spiffs partition data and we can use that as recovery data and write it into the spiffs partition. 

**Note:**
Backup partition can be specified using `backup_partition` field in `esp_custom_part_ota_cfg_t` structure. If no backup partition is specified, the passive app partition will be set as the backup partition by default.

### custom_part_ota

Under this scheme, the data is written directly into the specified partition. No copy of data is stored for recovery purposes.

## which scheme should be used

* If there is limited flash available, and only one OTA partiton is there. Then no backup partition can be formed, hence go for
scheme 2 i.e. custom_part_ota.
  1. But this can be unsafe, as if there is power interrupt during ota processes. There can be data loss as no backup is present.
* If there is enough flash available, and there are two OTA partitons (active and passive). Then one should for custom_part_ota_backup scheme. In case of power interrupt there can be power loss.

## API Reference

To learn more about how to use this component, please check API Documentation from header file [esp_custom_part_ota.h](https://github.com/espressif/idf-extra-components/blob/master/esp_custom_part_ota/include/esp_custom_part_ota.h)
