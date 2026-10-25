#!/usr/bin/env bash
# Generate the key pair that signs the OTA release manifests.
#
# The private half signs the manifests of the releases the unit is allowed to
# install. It never reaches the repository: the file lives in the gitignored
# `certs/` directory and belongs to the release pipeline.
#
# The public half is *not* a secret: the firmware embeds it to verify the
# manifests. It is versioned so every build of the firmware knows which
# releases it accepts, and so a rotation is a reviewable change.
#
# Usage:
#   tools/ota/gen-ota-keys.sh              # generate the pair
#   tools/ota/gen-ota-keys.sh --force      # overwrite an existing pair
#
# After the rotation the firmware has to be rebuilt: the public key is
# embedded at build time.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
CERT_DIR="${REPO_ROOT}/firmware/gateway/components/ota_manager/certs"
PRIVATE_KEY="${CERT_DIR}/ota_signing_private.pem"
PUBLIC_KEY="${CERT_DIR}/ota_signing_key.pem"

FORCE=0
if [[ "${1:-}" == "--force" ]]; then
    FORCE=1
fi

if ! command -v openssl >/dev/null 2>&1; then
    echo "openssl is required" >&2
    exit 1
fi

if [[ -f "${PRIVATE_KEY}" && ${FORCE} -eq 0 ]]; then
    echo "the private key already exists: ${PRIVATE_KEY}" >&2
    echo "run with --force to replace the pair" >&2
    exit 1
fi

mkdir -p "${CERT_DIR}"

# P-256 is the curve the firmware verifies with: mbedTLS, the key format of
# the embedded certificate and the size of the signature all agree with it.
openssl ecparam -name prime256v1 -genkey -noout -out "${PRIVATE_KEY}"
chmod 600 "${PRIVATE_KEY}"
openssl ec -in "${PRIVATE_KEY}" -pubout -out "${PUBLIC_KEY}" 2>/dev/null

echo "the release key pair was written:"
echo "  private (never committed): ${PRIVATE_KEY}"
echo "  public  (committed):       ${PUBLIC_KEY}"
echo
echo "the fingerprint of the public key is:"
openssl ec -in "${PUBLIC_KEY}" -pubin -noout -text 2>/dev/null | sed -n '2p'
echo
echo "the firmware has to be rebuilt: the public key is embedded at build time."
