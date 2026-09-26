# cbor: Concise Binary Object Representation (CBOR) for ESP-IDF

[![Component Registry](https://components.espressif.com/components/espressif/cbor/badge.svg)](https://components.espressif.com/components/espressif/cbor)

**CBOR** is a binary data serialization format that is similar to JSON but with a smaller footprint.

This component packages the upstream [TinyCBOR](https://github.com/intel/tinycbor) library for ESP-IDF and exposes its API as is: include `cbor.h` for the encoder and the parser, and `cborjson.h` for the conversion to JSON.

## Usage

Add the component to your project:

```bash
idf.py add-dependency "espressif/cbor^7.0.0"
```

Then include the header and call the upstream API:

```c
#include "cbor.h"
#include "cborjson.h"
```

See the [`examples/cbor`](examples/cbor) project for a complete ESP-IDF example.

## Learn more

For the API, refer to the upstream documentation:

- [TinyCBOR API documentation](https://intel.github.io/tinycbor/current/)
- [TinyCBOR repository](https://github.com/intel/tinycbor)
- [RFC 7049, Concise Binary Object Representation](https://tools.ietf.org/html/rfc7049)
