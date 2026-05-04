# NAND Flash translation layer for small MCUs

This component provides an ESP-IDF wrapper around the [Dhara library](https://github.com/dlbeer/dhara),
a NAND Flash Translation Layer for small MCUs.

The sources under `dhara/dhara/` are **vendored** from the upstream repository at a recorded baseline
commit. Espressif may apply patches to this tree. See [VENDORED_UPSTREAM.md](VENDORED_UPSTREAM.md)
for the baseline commit SHA and the procedure for refreshing against upstream.

For library background and API documentation, refer to the upstream documentation:
https://github.com/dlbeer/dhara/blob/master/README
