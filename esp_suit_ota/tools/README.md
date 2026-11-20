# esp-suit

`esp-suit` makes the keys, the identifiers and the signed SUIT envelopes that the `esp_suit_ota` component reads. It also checks an envelope the same way that the device checks it, before you publish the envelope.

## Install

```
pip install -e esp_suit_ota/tools
```

This installs the `esp-suit` command. It needs Python 3.9 or later. You can also run the tool without installing it:

```
python esp_suit_ota/tools/esp_suit.py <command> ...
```

## Commands

| Command | What it does |
|---|---|
| `esp-suit keygen KEY.pem [--anchor ANCHOR.bin]` | Makes a P-256 signing key, and optionally the 65-byte trust anchor for the device. The key file gets mode 0600. |
| `esp-suit anchor KEY.pem [-o ANCHOR.bin] [--c-array NAME]` | Writes the trust anchor of a private or public key, as a file or as a C array. |
| `esp-suit ids --vendor DOMAIN --class NAME [-o FILE.h]` | Writes the vendor and class UUIDs as a C header. Use `--pen NUMBER` instead of `--vendor` for a private enterprise number. |
| `esp-suit create RELEASE.yml [options]` | Makes a signed envelope (`.suit`) and a JSON summary (`.json`). |
| `esp-suit verify ENVELOPE.suit [--key FILE] [--payload URI=FILE] [--vendor DOMAIN --class NAME]` | Makes sure that an envelope is valid, the way the device does. The exit status is 0 if the envelope passes. |
| `esp-suit inspect ENVELOPE.suit [--json]` | Prints an envelope with every field named. |

Options of `create`:

- `-o FILE`: the envelope path. The default is the YAML name with `.suit`.
- `--key PEM`: a signing key. You can give it more than once, and it replaces `signing.keys`.
- `--alg ESP256|ES256`: the signature algorithm. The default is ESP256.
- `--sever-install`: carry the install sequence outside the manifest.
- `--inline-text`: keep the text inside the manifest. By default, the text is severed.
- `--min-seq N`, `--build-dir DIR`: refuse a sequence number below `N`, or below `CONFIG_ESP_SUIT_OTA_INITIAL_SEQ` of that build.

## The release file

```yaml
sequence_number: 42              # or auto-utc: the time in seconds since 1970
vendor: example.com              # vendor UUID = UUIDv5(DNS, domain)
# vendor_pen: 41381              # or a private enterprise number, sent as cbor-pen (tag 112)
class: my-product-hw2            # class UUID, draft section 8.4.8.2
reference_uri: https://ota.example.com/release.suit   # optional
text: { en: { update-description: "1.2.3: fixes" } }  # optional
components:
  - { id: [board, app], file: build/app.bin, uri: app.bin, skip_if_installed: true }
  - { id: [board, f1],  file: f1.bin,        uri: f1.bin }
  - { id: [board, cfg], file: cfg.bin,       uri: "#cfg" }
signing:
  keys: [keys/manifest_key.pem]  # one COSE_Sign1 block per key
  alg: ESP256                    # or ES256
  kid: mfg01                     # optional key identifier
```

Paths are relative to the YAML file. The last part of a component identifier names the pair on the device. A URI that starts with `#` puts the payload inside the envelope. Any other URI is fetched, and a relative URI resolves against the URL of the manifest. `skip_if_installed` makes the device skip a component whose installed image already matches.

The tool signs with deterministic ECDSA (RFC 6979), so the same inputs give the same envelope byte for byte. It encodes every item in RFC 8949 core deterministic CBOR.

## What verify looks at

- The envelope is `#6.107` and its CBOR is deterministic, including every nested item.
- The authentication wrapper and the manifest are present, and the manifest digest is correct.
- At least one signature block verifies with a key that you give with `--key`.
- The manifest version is 1.
- Every severed member is declared in the manifest, is present in the envelope, and has the right digest.
- Every integrated payload belongs to a component and matches its digest and size, and every `#` URI has its payload.
- Every file that you give with `--payload` matches its component. Give the URI exactly as the manifest writes it.
- With `--vendor` and `--class`, the identifiers match.

## Library

The package `esp_suit_tool` has the same functions for Python code: `esp_suit_tool.load()` and `esp_suit_tool.build()` for a release, `esp_suit_tool.verify()` for an envelope, and lower-level functions in `esp_suit_tool.envelope` and `esp_suit_tool.cbor`.

## Tests

```
pip install -e 'esp_suit_ota/tools[test]'
cd esp_suit_ota/tools && python -m pytest
```

If the ESP-IDF Python environment runs the tests, set `PYTEST_DISABLE_PLUGIN_AUTOLOAD=1`. Otherwise the ESP-IDF pytest plugin skips every test that has no chip target.
