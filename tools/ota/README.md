# OTA release tooling

The tools of the secure update path of issue #13. They cover the two halves
of a release: the **release side**, which signs the manifest, and the
**diagnosis side**, which lets anybody reproduce the rollback demonstration
without going through the whole pipeline.

| Tool | What it does |
|---|---|
| `gen-ota-keys.sh` | Generates the ECDSA P-256 key pair that signs the release manifests. |
| `sign-manifest.py` | Builds the canonical form of a manifest, signs it and writes the signed manifest. |
| `make-otadata.py` | Builds the `otadata` record that points the bootloader at a slot. |
| `make-defective-image.sh` | Builds a firmware image that never works, to demonstrate the rollback. |

## The keys

```bash
tools/ota/gen-ota-keys.sh
```

The pair lands in `firmware/gateway/components/ota_manager/certs/`:

- `ota_signing_private.pem` — the private half. It **never** enters the
  repository: the file is covered by the `*.pem` rule of `.gitignore`. It
  belongs to the release pipeline.
- `ota_signing_key.pem` — the public half. A public key is not a secret: the
  firmware embeds it to verify the manifests, so it is versioned. The
  `.gitignore` has an explicit exception for this exact path.

The public key is embedded at build time, so **rotating the key means
rebuilding the firmware**. The rotation is therefore a reviewable change:
the new public key is committed together with the firmware that accepts it.

The key of the repository is a development key. It exists so the tests have
a key that matches a signature; it is not a production key.

## Signing a manifest

The manifest is a flat JSON document. The unsigned form carries every field
of the schema except the signature:

```json
{
  "schema": 1,
  "product": "iiot-kit-gateway",
  "brand": "IIoT-Kit",
  "model": "GW-S3-1",
  "revision": "rev-a",
  "target": "esp32s3",
  "version": "1.1.0",
  "channel": "production",
  "size": 1011344,
  "sha256": "2f2d5b61c3a9e4f0d8b7a6c5e4d3c2b1a09f8e7d6c5b4a39281706f5e4d3c2b1",
  "url": "https://releases.example.org/iiot-kit/1.1.0/gateway.bin",
  "key_id": "ota-2026"
}
```

```bash
tools/ota/sign-manifest.py \
    --private-key firmware/gateway/components/ota_manager/certs/ota_signing_private.pem \
    --in manifest.json \
    --out manifest.signed.json
```

The signed document adds two fields:

- `signature` — the base64 of the raw ECDSA P-256 signature (`r || s`, 32
  bytes each) over the canonical form of the manifest.
- `manifest_sha256` — the SHA-256 of the canonical form, so the unit can
  pin the manifest itself and not only its signature.

The **canonical form** is the byte string the signature covers:

```
iiot-ota-manifest/v1
schema=1
product=iiot-kit-gateway
brand=IIoT-Kit
model=GW-S3-1
revision=rev-a
target=esp32s3
version=1.1.0
channel=production
size=1011344
sha256=<64 hex en minúsculas>
url=https://releases.example.org/iiot-kit/1.1.0/gateway.bin
key_id=ota-2026
```

The definition lives in one place, the header
`firmware/gateway/components/ota_manifest/include/ota_manifest.h`, and the
two implementations cannot drift apart because both are checked against the
same golden vector:

```bash
tools/ota/sign-manifest.py --self-test   # the release side
```

and the `kCanonicalPayload` literal of
`firmware/gateway/components/ota_manifest/test/test_ota_manifest.c` (the
firmware side, run on the host and on the device).

## Demonstrating the rollback

The rollback is the safety net of the whole feature: an image that does not
work must never leave the unit without firmware. The demonstration needs no
release server and no network.

```bash
# 1. The defective image: a copy of the gateway whose app_main panics.
tools/ota/make-defective-image.sh

# 2. Write it into the inactive slot, without resetting the unit yet.
esptool.py --chip esp32s3 --port /dev/ttyACM0 --after no_reset \
           write_flash 0x320000 build/ota-defective/gateway.bin

# 3. Arm that slot. The bootloader starts it and marks it pending verify.
tools/ota/make-otadata.py --slot ota_1 --out /tmp/otadata.bin
esptool.py --chip esp32s3 --port /dev/ttyACM0 --after no_reset \
           write_flash 0xf000 /tmp/otadata.bin

# 4. Reset and watch: the defective image panics, the bootloader discards
#    it and goes back to ota_0.
idf.py -p /dev/ttyACM0 monitor
```

The `otadata` record is 32 bytes:

```
offset  0   uint32  ota_seq      the sequence number of the selection
offset  4   char[20] seq_label   the label of the slot
offset 24   uint32  ota_state    new / pending verify / valid / aborted
offset 28   uint32  crc          CRC-32 of the four bytes of ota_seq
```

The bootloader maps the sequence number to a slot with
`(ota_seq - 1) % number_of_slots`, so an odd sequence number starts `ota_0`
and an even one starts `ota_1`.

**The CRC is not the plain CRC-32 of the data.** The bootloader uses
`esp_rom_crc32_le(0xFFFFFFFF, ...)`, which complements the seed before the
loop and complements the result afterwards. `zlib.crc32(data)` gives a
different value; `zlib.crc32(data, 0xFFFFFFFF)` gives the right one. The
tool computes it the way the ROM does and cross-checks it against that
convention, so a record it writes is always accepted.

`make-otadata.py` also writes the `pending-verify`, `valid` and `aborted`
states, which is what the bootloader leaves behind after a rollback.
