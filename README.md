[![pre-commit](https://img.shields.io/badge/pre--commit-enabled-brightgreen?logo=pre-commit&logoColor=white)](https://github.com/pre-commit/pre-commit)
[![Build and Run Apps](https://github.com/espressif/idf-extra-components/actions/workflows/build_and_run_apps.yml/badge.svg?branch=master)](https://github.com/espressif/idf-extra-components/actions/workflows/build_and_run_apps.yml)
[![Clang-Tidy](https://github.com/espressif/idf-extra-components/actions/workflows/clang-tidy.yml/badge.svg?branch=master)](https://github.com/espressif/idf-extra-components/security/code-scanning?query=is%3Aopen+branch%3Amaster)

# ESP-IDF Extra Components

This repository is used to maintain various extra components for [ESP-IDF](https://github.com/espressif/esp-idf). These components can be installed from [ESP Component Registry](https://components.espressif.com/).

Many of the components in this repository are wrappers around third-party libraries, such as zlib, libpng, etc. There are also various components developed by Espressif which don't currently fit into another repository (like [esp-protocols](https://github.com/espressif/esp-protocols), [esp-usb](https://github.com/espressif/esp-usb), [esp-iot-solution](https://github.com/espressif/esp-iot-solution), etc).

## Related Projects

- [ESP-IDF](https://github.com/espressif/esp-idf)
- [ESP Component Registry](https://components.espressif.com/)
- [IDF Component Manager](https://github.com/espressif/idf-component-manager)
- [ESP IOT Solution](https://github.com/espressif/esp-iot-solution)
- [ESP Protocols](https://github.com/espressif/esp-protocols)
- [ESP USB](https://github.com/espressif/esp-usb)

## Contribution

We welcome contributions to idf-extra-components repository!

You can contribute by fixing bugs, adding features, adding documentation or reporting an [issue](https://github.com/espressif/idf-extra-components/issues). We accept contributions via [Github Pull Requests](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/about-pull-requests).

Before reporting an issue, make sure you've searched for a similar one that was already created.

### Adding New Components

Please note that this repository is intended for components maintained by Espressif developers. If you don't work at Espressif and you'd like to publish a component to the ESP Component Registry, please set up a separate repository for your component. You can find more information about this in the [IDF Component Manager documentation](https://docs.espressif.com/projects/idf-component-manager/en/latest/). You can also check out the [talk about developing and publishing components](https://youtu.be/D86gQ4knUnc) from Espressif DevCon 2023. Feel free to [open an issue](https://github.com/espressif/idf-component-manager/issues) if you encounter any problem.
