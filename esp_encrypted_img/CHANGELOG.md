## 2.5.0

### Enhancements:
- Added an API to export the public key from the private key: `esp_encrypted_img_export_public_key`

## 2.4.0

### Enhancements:
- Added support for ECIES based OTA encryption scheme

## 2.3.0

### Enhancements:
- Added pre_encrypted_ota example, which demonstrates the OTA using encrypted image file.

## 2.2.1

- Build system: fix the dependency for generating pre encrypted image

## 2.2.0

### Enhancements:
- Added an API to get the size of pre encrypted binary image header, this could be useful while computing entire decrypted image length: `esp_encrypted_img_get_header_size`

## 2.1.0

### Enhancements:
- Added an API to abort the decryption process: `esp_encrypted_img_decrypt_abort`
- Added an API to check if the complete data has been received: `esp_encrypted_img_is_complete_data_received`

## 2.0.4

- `rsa_pub_key` member of `esp_decrypt_cfg_t` structure is now deprecated. Please use `rsa_priv_key` instead. 
- `rsa_pub_key_len` member of `esp_decrypt_cfg_t` structure is now deprecated. Please use `rsa_priv_key_len` instead. 
