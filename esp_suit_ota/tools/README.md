# esp-suit

[`esp_suit_ota`](../README.md) is an ESP-IDF component that installs firmware and data from a
signed [SUIT](https://datatracker.ietf.org/doc/draft-ietf-suit-manifest/) manifest. One manifest
can update the app and several data partitions together. Nothing is switched until every payload
has been downloaded and verified, and with A/B slots the whole set switches at once.

`esp-suit` is the host side of that component. It makes the keys and identifiers the firmware is
built with, turns a short json description into a signed envelope the device can install, and
checks that envelope the way the device will, before you publish it.

> **WARNING:** a signing key lets anyone who holds it install firmware on your devices. Keep it
> off shared machines and out of the repository.

## Installation

```
pip install -e esp_suit_ota/tools
```

The tool needs Python 3.9 or later. You can also run it without installing:

```
python esp_suit_ota/tools/esp_suit.py <command> ...
```

## Make a signing key

This writes a P-256 private key with mode 0600 (Unix file permission), and the 65-byte trust
anchor that the firmware embeds and checks every manifest against.

> **NOTE:** at the current state, the tool and the component support two profiles only,
> `suit-sha256-esp256-ecdh-a128ctr` and `suit-sha256-esp256-ecdh-a128gcm`. Other profiles may be
> added in future.

```
esp-suit keygen keys/manifest.pem --anchor main/keys/anchor_p256.bin
```

- The private key stays on your build machine. The anchor is public and goes into the firmware.
- `--force` replaces an existing key file. Without it an existing file is an error.
- A device that decrypts payloads needs a second key pair of its own. Add `--scalar` to write the
  32-byte private scalar the firmware embeds:

  `esp-suit keygen keys/device.pem --anchor keys/device_pub.bin --scalar keys/device_key.bin`

The two pairs are independent, and the release description names one file from each: the signing
private key, and the device public key.

```json
{
  "components": [
    { "id": ["esp32c5", "app"], "file": "build/app.bin", "uri": "app.bin" }
  ],
  "encryption": {
    "recipient": "keys/device_pub.bin",
    "content": "A128GCM"
  },
  "signing": {
    "keys": ["keys/manifest.pem"]
  }
}
```

`encryption.recipient` is the public half of the device pair, and `signing.keys` the private half
of the signing pair.

The other two files go into the firmware: `main/keys/anchor_p256.bin` verifies the signature, and
`keys/device_key.bin` decrypts the payload. `keys/device.pem` stays on your machine as the copy
you keep.

**NOTE: Running the `keygen` command generate one pair of P256 key, user need to generate two pair (hence run the command for two times), one for singing and verifying and other for encrypted paylaods**

## Get the trust anchor of an existing key

Use this when the key already exists, or when you want the anchor as source rather than a file.

```
esp-suit anchor keys/manifest.pem -o main/keys/anchor_p256.bin
esp-suit anchor keys/manifest.pem --c-array suit_anchor
```

- It accepts a private PEM, a public PEM or an anchor file, and always gives the same 65 bytes.
- With neither option it prints the anchor as hex.

## Make the vendor and class identifiers

The device checks these before it writes anything, so the firmware and every release must agree
on them. This writes them as a C header.

```
esp-suit ids --vendor example.com --class my-board-rev2 -o main/suit_identity.h
```

- The vendor identifier is UUIDv5 of the domain, and the class identifier is UUIDv5 of the class
  name under it. The same two strings always give the same identifiers.
- `--pen 41381` replaces `--vendor` for a private enterprise number.
- Choose the class name per product and hardware revision, not per firmware version.

## Make a release

This reads the release description, signs the manifest, and writes the envelope the device
fetches together with a JSON summary of what went into it.

```
esp-suit create release.json
```

- It writes `release.suit` and `release.info.json` beside the description. `-o` and `--info`
  change those.
- It verifies the envelope it has just written, so a broken file is never handed to you.
- `--key PEM` overrides `signing.keys`, and may be given more than once.
- `--alg ES256` selects the other signature algorithm. The default is ESP256.
- `--min-seq N` and `--build-dir DIR` (esp-idf build directory) refuse a sequence number below `N`, or below
  `CONFIG_ESP_SUIT_OTA_INITIAL_SEQ` of that build (reads from `sdkconfig`). Prefer `--build-dir` over `--min-seq`.
- `--sever-install` and `--inline-text` move the install sequence and the text out of or into
  the manifest. (This is `optional`.)

## Check a release before publishing it

This runs the device's own checks on your machine, and exits non-zero if any of them fails.

```
esp-suit verify release.suit --key main/keys/anchor_p256.bin \
    --vendor example.com --class my-board-rev2 --payload app.bin=build/app.bin
```

- Give `--key` the anchor file, so you check with exactly the bytes the device holds. Without a
  key the signature is not checked, and the report says so.
- `--payload URI=FILE` checks a payload that is fetched rather than carried in the envelope. The
  URI must match the component's `uri` field exactly.
- `--recipient-key PEM` is the device's private key, which lets verify decrypt an encrypted
  payload and check its digest.
- Every line is marked `ok`, `FAIL` or `skip`, with the reason beside it.

## Look inside an envelope

```
esp-suit inspect release.suit
esp-suit inspect release.suit --json
```

- The first form prints the envelope in CBOR diagnostic notation with every field named, which is
  the quickest way to see why a device rejected something.
- `--json` prints the size, the sequence number and the component list, for scripts.

# Syntax for release.json

A release description is JSON. The guide starts with the smallest file that works, then adds one
section per thing you may want, and ends with every key listed.

- [Release description in JSON](docs/release_json.md)

A working sample ships with the example, in
[`examples/suit_ota_example/release/release.json`](../examples/suit_ota_example/release/release.json).

# References

- [draft-ietf-suit-manifest](https://datatracker.ietf.org/doc/draft-ietf-suit-manifest/), the
  manifest format.
- [draft-ietf-suit-mti](https://datatracker.ietf.org/doc/draft-ietf-suit-mti/), the algorithm
  profiles.
- [draft-ietf-suit-firmware-encryption](https://datatracker.ietf.org/doc/draft-ietf-suit-firmware-encryption/),
  encrypted payloads.
- [RFC 8949](https://www.rfc-editor.org/rfc/rfc8949), the CBOR encoding the tool emits.
