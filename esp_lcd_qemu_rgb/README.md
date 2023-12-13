# QEMU RGB Panel

This component presents an interface for the virtual QEMU RGB panel, implemented into Espressif's QEMU fork starting from version 8.1.3-20231206.

This virtual RGB panel that can be used to display graphical interfaces. This panel also includes a dedicated frame buffer, absent in real hardware and independent from the internal RAM, that allows user program to populate the pixels in.

**Please note** that the virtual RGB panel currently only supports ARGB8888 (32-bit) color mode.
