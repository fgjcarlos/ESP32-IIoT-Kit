#!/usr/bin/env python3
"""Sign an OTA release manifest.

The gateway only installs an image whose manifest carries a valid
signature over the canonical form of the manifest itself. This script is
the release side of that contract: it builds the canonical form, signs it
with the release key and writes the signed manifest.

The canonical form is defined once, in the C header
`firmware/gateway/components/ota_manifest/include/ota_manifest.h`. The two
implementations cannot drift apart because both are checked against the
same golden vector: `GOLDEN_MANIFEST` / `GOLDEN_CANONICAL` below and the
`kManifestJson` / `kCanonicalPayload` literals of
`firmware/gateway/components/ota_manifest/test/test_ota_manifest.c`.

Usage:
    tools/ota/sign-manifest.py --private-key key.pem --in manifest.json \
                               --out manifest.signed.json
    tools/ota/sign-manifest.py --self-test

The private key never reaches the repository: it lives in the release
pipeline (or in the gitignored `ota_signing_private.pem` of a local
installation).
"""

import argparse
import base64
import hashlib
import json
import sys
from pathlib import Path

try:
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.hazmat.primitives.asymmetric.utils import decode_dss_signature
except ImportError:  # pragma: no cover - the dependency is documented
    sys.exit("the 'cryptography' package is required: pip install cryptography")


# The order of the fields is part of the contract. The header of the
# canonical form is fixed too.
CANONICAL_HEADER = "iiot-ota-manifest/v1"

# The fields the canonical form covers, in order. The signature itself is
# not one of them: it is what signs the rest.
CANONICAL_FIELDS = (
    "schema",
    "product",
    "brand",
    "model",
    "revision",
    "target",
    "version",
    "channel",
    "size",
    "sha256",
    "url",
    "key_id",
)

# The vector the firmware test asserts against. If this script and the
# firmware ever disagree about the canonical form, one of the two fails.
GOLDEN_MANIFEST = {
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
    "url": "https://example.org/firmware/gateway.bin",
    "signature": "",
    "key_id": "ota-2026",
}

GOLDEN_CANONICAL = (
    "iiot-ota-manifest/v1\n"
    "schema=1\n"
    "product=iiot-kit-gateway\n"
    "brand=IIoT-Kit\n"
    "model=GW-S3-1\n"
    "revision=rev-a\n"
    "target=esp32s3\n"
    "version=1.1.0\n"
    "channel=production\n"
    "size=1011344\n"
    "sha256=2f2d5b61c3a9e4f0d8b7a6c5e4d3c2b1a09f8e7d6c5b4a39281706f5e4d3c2b1\n"
    "url=https://example.org/firmware/gateway.bin\n"
    "key_id=ota-2026\n"
)


def canonical_payload(manifest: dict) -> bytes:
    """Build the exact bytes that get signed."""
    lines = [CANONICAL_HEADER]
    for field in CANONICAL_FIELDS:
        value = manifest[field]
        if isinstance(value, int):
            value = str(value)
        value = str(value)
        if "\n" in value or "\r" in value:
            raise ValueError(f"the field {field} contains a line break")
        lines.append(f"{field}={value}")
    return ("\n".join(lines) + "\n").encode("utf-8")


def sign(private_key_path: Path, manifest: dict) -> dict:
    """Sign a manifest and return a copy that carries the signature."""
    payload = canonical_payload(manifest)

    key = serialization.load_pem_private_key(private_key_path.read_bytes(), password=None)
    if not isinstance(key, ec.EllipticCurvePrivateKey) or not isinstance(
        key.curve, ec.SECP256R1
    ):
        raise ValueError("the release key must be an ECDSA P-256 private key")

    der = key.sign(payload, ec.ECDSA(hashes.SHA256()))
    r, s = decode_dss_signature(der)
    raw = r.to_bytes(32, "big") + s.to_bytes(32, "big")

    signed = dict(manifest)
    signed["signature"] = base64.b64encode(raw).decode("ascii")
    # The digest of the canonical form travels with the manifest so the
    # unit can pin it as well.
    signed["manifest_sha256"] = hashlib.sha256(payload).hexdigest()
    return signed


def self_test() -> int:
    """Check the canonical form against the golden vector."""
    payload = canonical_payload(GOLDEN_MANIFEST).decode("utf-8")
    if payload != GOLDEN_CANONICAL:
        print("the canonical form does not match the golden vector", file=sys.stderr)
        print(f"expected:\n{GOLDEN_CANONICAL}", file=sys.stderr)
        print(f"obtained:\n{payload}", file=sys.stderr)
        return 1
    print("the canonical form matches the golden vector")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--private-key", type=Path, help="PEM file with the release private key")
    parser.add_argument("--in", dest="input", type=Path, help="unsigned manifest to sign")
    parser.add_argument("--out", type=Path, help="where the signed manifest is written")
    parser.add_argument("--canonical", action="store_true", help="print the canonical form and exit")
    parser.add_argument("--self-test", action="store_true", help="check the canonical form and exit")
    args = parser.parse_args()

    if args.self_test:
        return self_test()

    if args.input is None:
        parser.error("--in is required")
    if not args.canonical and args.private_key is None:
        parser.error("--private-key is required")

    manifest = json.loads(args.input.read_text())
    for field in CANONICAL_FIELDS:
        if field not in manifest:
            sys.exit(f"the manifest has no {field}")

    if args.canonical:
        sys.stdout.write(canonical_payload(manifest).decode("utf-8"))
        return 0

    signed = sign(args.private_key, manifest)
    document = json.dumps(signed, indent=2, sort_keys=True) + "\n"

    if args.out is None:
        sys.stdout.write(document)
    else:
        args.out.write_text(document)
        print(f"the signed manifest was written to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
