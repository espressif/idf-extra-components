# SEGGER SystemView for ESP-IDF

[![Component Registry](https://components.espressif.com/components/espressif/esp_sysview/badge.svg)](https://components.espressif.com/components/espressif/esp_sysview)

This component integrates SEGGER SystemView with ESP-IDF and is distributed as a managed component.

## Install (managed component)

Add a dependency in your project's `idf_component.yml`:

```yaml
dependencies:
  espressif/esp_sysview: ^1
```

Configure SystemView tracing in `idf.py menuconfig`:
- Enable tracing: `CONFIG_ESP_TRACING_ENABLE`
- Select trace library: Component config > ESP Trace Configuration > Trace library > External library from component registry `CONFIG_ESP_TRACE_LIB_EXTERNAL`
- Select timestamp source: Component config > ESP Trace Configuration > Trace timestamp source
- Configure event filters: Component config > SEGGER SystemView Configuration

## Documentation and examples

- ESP-IDF examples:
  - `examples/system/sysview_tracing/` (basic SystemView tracing)
  - `examples/system/sysview_tracing_heap_log/README.md` (heap tracing)
- SEGGER SystemView documentation for host-side tooling.

## License

Apache 2.0. See `LICENSE` file.
