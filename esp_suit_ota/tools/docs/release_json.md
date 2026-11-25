# Release description in JSON

A release description tells `esp-suit create` what goes into one release. It is a JSON file, and
JSON is the only format the tool reads.

**Paths inside the file are relative to the file itself**, not to the directory you run the command
from.

A build writes two files. `release.json` gives you `release.suit`, the envelope the device
fetches, and `release.info.json`, a summary of what went into it. The summary is never written
over the description.

JSON has no comments, so everything here is explained in the text rather than beside the lines.
A working sample ships with the example, in
[`examples/suit_ota_example/release/release.json`](../../examples/suit_ota_example/release/release.json).

## The smallest file that works

```json
{
  "sequence_number": 1,
  "vendor": "example.com",
  "class": "my-board-rev2",
  "components": [
    { "id": ["esp32c5", "app"], "file": "build/app.bin", "uri": "app.bin" }
  ],
  "signing": { "keys": ["keys/manifest.pem"] }
}
```

```
esp-suit create release.json
```

- `sequence_number` must be higher than the one already installed, or the device refuses the
  release.
- `vendor` and `class` must be the same two strings you gave to `esp-suit ids`, or the device
  answers `ESP_ERR_SUIT_OTA_CONDITION`.
- The last part of a component identifier, `app` here, names the target on the device.
- `uri` is where the device fetches the payload. A relative one resolves against the URL of the
  manifest.

## Carry the payload inside the envelope

Start the URI with `#` and the payload travels inside `release.suit` instead of being fetched
separately. That costs one network connection instead of two.

```json
"components": [
  { "id": ["esp32c5", "app"], "file": "build/app.bin", "uri": "#app" }
]
```

## Update more than one component

```json
"components": [
  { "id": ["esp32c5", "app"], "file": "build/app.bin", "uri": "app.bin" },
  { "id": ["esp32c5", "f1"],  "file": "f1.bin",        "uri": "f1.bin" },
  { "id": ["esp32c5", "cfg"], "file": "cfg.bin",       "uri": "#cfg" }
]
```

- At most 16 components, and no identifier twice.
- If the firmware puts two targets in one group, a release must update all of them or none, or
  the device answers `ESP_ERR_SUIT_OTA_GROUP`.

## Skip a component that is already installed

```json
"components": [
  { "id": ["esp32c5", "app"], "file": "build/app.bin", "uri": "app.bin",
    "skip_if_installed": true }
]
```

The device hashes what it already has and skips the download when it matches.

## Sign with more than one key

Every key adds a signature block, and a device accepts the release if it can verify any one of
them. This is how you rotate a signing key without leaving devices behind.

```json
"signing": {
  "keys": ["keys/old.pem", "keys/new.pem"],
  "alg": "ESP256",
  "kid": "mfg01"
}
```

## Encrypt the payloads

The device needs `CONFIG_ESP_SUIT_OTA_ENCRYPTED_PAYLOAD` and its own private key. Name the
device's public key here.

```json
"encryption": {
  "recipient": "keys/device_pub.bin",
  "content": "A128GCM"
}
```

- A fresh content key is made for every release and wrapped for that device, so no key is reused.
- `"content": "A128GCM"` is the `suit-sha256-esp256-ecdh-a128gcm` profile, and `"A128CTR"` the
  `suit-sha256-esp256-ecdh-a128ctr` profile.
- Check the result with `esp-suit verify release.suit --recipient-key keys/device.pem`.

## Leave one component unencrypted

```json
"components": [
  { "id": ["esp32c5", "app"], "file": "build/app.bin", "uri": "app.bin" },
  { "id": ["esp32c5", "cfg"], "file": "cfg.bin", "uri": "cfg.bin", "encrypt": false }
],
"encryption": { "recipient": "keys/device_pub.bin" }
```

## Add a description and a reference URI

```json
"reference_uri": "https://ota.example.com/release.suit",
"text": {
  "en": {
    "update-description": "1.2.3: fixes the sensor timeout",
    "manifest-description": "nightly build"
  }
}
```

The text is severed by default, which means it can be stripped before the envelope reaches a
device without breaking the signature. `esp-suit create --inline-text` keeps it inside the
manifest instead.

Non-ASCII text may be written either way. `"Grüße"` and `"Grüße"` in a UTF-8 file are
the same string and give the same envelope.

## Let the sequence number look after itself

```json
"sequence_number": "auto-utc"
```

That uses the time in seconds since 1970, which always increases. It is also what you get when
you leave the key out. Pair it with `esp-suit create --build-dir build` so a release can never
fall below the floor compiled into the firmware.

## A complete file with everything in it

```json
{
  "sequence_number": "auto-utc",
  "vendor": "example.com",
  "class": "my-board-rev2",
  "reference_uri": "https://ota.example.com/release.suit",
  "text": { "en": { "update-description": "1.2.3: fixes the sensor timeout" } },
  "components": [
    { "id": ["esp32c5", "app"], "file": "build/app.bin", "uri": "app.bin",
      "skip_if_installed": true },
    { "id": ["esp32c5", "cfg"], "file": "cfg.bin", "uri": "#cfg", "encrypt": false }
  ],
  "encryption": {
    "recipient": "keys/device_pub.bin",
    "content": "A128GCM",
    "key_wrap": "A128KW",
    "salt": 16
  },
  "signing": {
    "keys": ["keys/manifest.pem"],
    "alg": "ESP256"
  }
}
```

## Every key

**Top level**

| Key | Type | Required | Meaning |
|---|---|---|---|
| `sequence_number` | number or `"auto-utc"` | no, default `"auto-utc"` | Anti-rollback counter. Must exceed the installed one. |
| `vendor` | string | yes, unless `vendor_pen` | Domain hashed into the vendor identifier. |
| `vendor_pen` | number | yes, unless `vendor` | Private enterprise number, sent as cbor-pen. |
| `class` | string | yes | Class name hashed into the class identifier. |
| `components` | array | yes | What the release updates. At most 16. |
| `signing` | object | yes, unless `--key` is passed | Who signs the manifest. |
| `encryption` | object | no | Turns on payload encryption for every component. |
| `reference_uri` | string | no | Where this manifest lives. Metadata only. |
| `text` | object of language to fields | no | Human readable description. |

**Inside a component**

| Key | Type | Required | Meaning |
|---|---|---|---|
| `id` | array of strings | yes | Component identifier. The last part names the target. |
| `file` | string | yes | The payload, read when the release is built. |
| `uri` | string | yes | Where the device fetches it. A leading `#` carries it in the envelope. |
| `skip_if_installed` | boolean | no, default false | Skip when the installed image already matches. |
| `encrypt` | boolean | no, default true | Set false to leave this one in the clear. |
| `prefetch` | boolean | no, default false | Carry the payload in the envelope under its full URI. |
| `fetch_args` | string | no | Opaque argument handed to the transport. |

**Inside `signing`**

| Key | Type | Required | Meaning |
|---|---|---|---|
| `keys` | array of strings | yes, unless `key` | One signature block per key. |
| `key` | string | yes, unless `keys` | A single signing key. |
| `alg` | `"ESP256"` or `"ES256"` | no, default `"ESP256"` | Signature algorithm. |
| `kid` | string | no | Key identifier written into the signature header. |

**Inside `encryption`**

| Key | Type | Required | Meaning |
|---|---|---|---|
| `recipient` | string | yes | The device's public key, PEM or the 65-byte point. |
| `content` | `"A128GCM"`, `"A128CTR"`, `"A256GCM"`, `"A256CTR"` | no, default `"A128GCM"` | Payload cipher. |
| `key_wrap` | `"A128KW"` or `"A256KW"` | no, default `"A128KW"` | Protects the content key. |
| `kid` | string | no | Identifies the recipient key. |
| `salt` | number, 0 to 64 | no, default 0 | Bytes of random salt for the key derivation. |

## Two things to watch

**A misspelled key inside a component is ignored.** `"skip_if_install"` instead of
`"skip_if_installed"` builds a release that silently does not skip anything. Check the key names
in the tables above against what you wrote.

**A class name is text, even when it looks like something else.** `"class": "no"` and
`"class": "1.20"` are names. Write `"class": 1.20` without the quotes and the tool refuses the
file, because a number would hash to an identifier nobody meant.

## When something goes wrong

| Message | Cause |
|---|---|
| `not valid JSON` | A syntax error. The message gives the line and column. A comment is a syntax error. |
| `YAML is no longer read` | The file is named `.yml` or `.yaml`. Write the description in JSON. |
| `not a release description` | The file parsed, but the top level is not an object. |
| `release description has no "components"` | A missing or misspelled top-level key. |
| `"class" must be text` | A class or vendor given as a number rather than a string. |
| `component N has no "uri"` | A component is missing `id`, `file` or `uri`. |
| `cannot read FILE` | A component names a payload file that is not there. |
| `sequence number N is below the floor M` | `--min-seq` or `--build-dir` refused it. |
| `component [...] is listed twice` | Two components share an identifier. |
| `the encryption block needs "recipient"` | The encryption block has no recipient key. |
| `no signing key` | No `signing.keys`, no `signing.key` and no `--key`. |
