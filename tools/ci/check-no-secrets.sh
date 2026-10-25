#!/usr/bin/env bash
#
# Check that an artefact carries no secret.
#
# The development artefacts of the firmware are published by the CI, so they
# must not carry the manufacturing secrets of a brand, the Wi-Fi credentials of
# a site, nor a private key. The CI build never sees those files (they are
# gitignored and generated locally), so this check is the proof that the
# artefact really is free of them.
#
# The check looks for:
#   - the PEM markers of a private key;
#   - the names of the local secret headers, which would mean a secret was
#     compiled into the image;
#   - the private half of the OTA release key.
#
# Usage:
#   tools/ci/check-no-secrets.sh <file-or-directory> [...]

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

cd "${REPO_ROOT}"

if [[ $# -eq 0 ]]; then
    echo "check-no-secrets: nothing to check" >&2
    exit 1
fi

# Markers that must never appear in a published artefact.
#
# A private key is a COMPLETE PEM block: the BEGIN marker, the base64 body
# and the END marker. Looking for the marker alone is not enough, because
# mbedTLS carries the marker strings in its code and they end up in the image
# without any key material next to them.
PEM_BLOCK='-----BEGIN [A-Z ]*PRIVATE KEY-----[A-Za-z0-9+/=\r\n]{32,}-----END'

# Headers that hold the local secrets of a site or of a brand. Their names
# appear in the image when the secret was compiled into it.
LOCAL_SECRET_MARKERS=(
    "brand_secrets"
    "wifi_credentials"
)

status=0

for target in "$@"; do
    if [[ ! -e "${target}" ]]; then
        echo "check-no-secrets: ${target} does not exist" >&2
        status=1
        continue
    fi

    if grep -Pz --binary-files=text -q -- "${PEM_BLOCK}" "${target}" 2>/dev/null; then
        echo "check-no-secrets: ${target} contains a private key" >&2
        status=1
    fi

    for pattern in "${LOCAL_SECRET_MARKERS[@]}"; do
        if grep -rIl --binary-files=text -e "${pattern}" "${target}" >/dev/null 2>&1; then
            echo "check-no-secrets: ${target} contains '${pattern}'" >&2
            status=1
        fi
    done
done

# The private half of the release key must never be tracked, and the local
# secret headers must never be committed either.
if git rev-parse --git-dir >/dev/null 2>&1; then
    tracked="$(git ls-files | grep -E '\.(pem|key)$|(^|/)\.env' || true)"
    # The public half of the OTA key is deliberately tracked.
    tracked="$(echo "${tracked}" | grep -v 'ota_manager/certs/ota_signing_key.pem' || true)"

    if [[ -n "${tracked}" ]]; then
        echo "check-no-secrets: these key or environment files are tracked:" >&2
        echo "${tracked}" >&2
        status=1
    fi

    for local_secret in \
        "firmware/gateway/components/wifi_manager/include/wifi_credentials.local.h" \
        "firmware/gateway/components/brand_profile/include/brand_secrets.local.h"; do
        if git ls-files --error-unmatch "${local_secret}" >/dev/null 2>&1; then
            echo "check-no-secrets: ${local_secret} is tracked" >&2
            status=1
        fi
    done
fi

if [[ ${status} -eq 0 ]]; then
    echo "check-no-secrets: no secret found in: $*"
fi

exit ${status}
