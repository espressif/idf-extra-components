# Changelog

## 1.0.0

- First release.
- usrsctp wrapped as an ESP-IDF component. Submodule pins canonical
  [sctplab/usrsctp](https://github.com/sctplab/usrsctp) at commit
  `2e1ab10`; five ESP-IDF-specific patches are applied at build configure
  time (see [`patches/README.md`](patches/README.md)).
- Supports the full ESP32 family plus the IDF Linux target for
  host-side testing.
- Bundled patches add lwIP routing for the userspace conn layer,
  thread-safe netif APIs, SPIRAM-backed worker-thread stacks, and
  buildability on the IDF Linux target.
- Embedded smoke test (`test_apps/`) and host-side socket-lifecycle
  test (`host_test/`) included.
