## 1.0.0

- Initial version

## 1.1.0

- Update to [CDC-ACM driver](https://components.espressif.com/components/espressif/usb_host_cdc_acm) to v2
- Provide default configurations for tested modems
- Fix USB receive path bug, where received data could be overwritten by new data
- Initial support for modems with two AT ports
