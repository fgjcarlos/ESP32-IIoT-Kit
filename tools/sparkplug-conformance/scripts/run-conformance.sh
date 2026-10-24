#!/usr/bin/env bash
#
# Run the Sparkplug B v3.0.0 conformance harness.
#
#   bash tools/sparkplug-conformance/scripts/run-conformance.sh
#
# The script starts a throwaway Mosquitto broker plus the Python Host
# Application, waits for the broker to become healthy, and propagates the
# client's exit code:
#
#   0  every assertion passed
#   2  at least one assertion failed
#   1  harness error (broker unreachable, container did not start, ...)
#
# Environment variables (all optional):
#   MODE                       simulate (default) | observe
#   SPARKPLUG_BROKER_URI       mqtt://broker:1883 (default)
#   SPARKPLUG_GROUP_ID         iiot-kit (default)
#   SPARKPLUG_EDGE_NODE_ID     GW-TEST01 (default)
#   SPARKPLUG_DEVICE_ID        DEV-TEST01 (default)
#   SPARKPLUG_TIMEOUT          seconds to wait for messages (default 30)
#   SPARKPLUG_EXPECT_DEVICE    1 (default) | 0
#
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
HARNESS_DIR="$(cd -- "${SCRIPT_DIR}/.." && pwd)"

MODE="${MODE:-simulate}"
SPARKPLUG_BROKER_URI="${SPARKPLUG_BROKER_URI:-mqtt://broker:1883}"
SPARKPLUG_GROUP_ID="${SPARKPLUG_GROUP_ID:-iiot-kit}"
SPARKPLUG_EDGE_NODE_ID="${SPARKPLUG_EDGE_NODE_ID:-GW-TEST01}"
SPARKPLUG_DEVICE_ID="${SPARKPLUG_DEVICE_ID:-DEV-TEST01}"
SPARKPLUG_TIMEOUT="${SPARKPLUG_TIMEOUT:-30}"

# Exported so that Compose and the client container always agree on the same
# values, even when a value was defaulted here rather than in the environment.
export MODE SPARKPLUG_BROKER_URI SPARKPLUG_GROUP_ID SPARKPLUG_EDGE_NODE_ID
export SPARKPLUG_DEVICE_ID SPARKPLUG_TIMEOUT

CLIENT_CONTAINER="sparkplug-conformance-client"
BROKER_CONTAINER="sparkplug-conformance-broker"

# Host side of the published broker port. Only relevant when the harness runs
# alongside another broker on the same machine.
export SPARKPLUG_BROKER_HOST_PORT="${SPARKPLUG_BROKER_HOST_PORT:-1883}"

# --- compose implementation detection ---------------------------------------
if ! command -v docker >/dev/null 2>&1; then
    echo "ERROR: docker is not installed or not on PATH." >&2
    exit 1
fi

if docker compose version >/dev/null 2>&1; then
    COMPOSE=(docker compose)
elif command -v docker-compose >/dev/null 2>&1; then
    COMPOSE=(docker-compose)
else
    echo "ERROR: neither 'docker compose' nor 'docker-compose' is available." >&2
    exit 1
fi

# --- cleanup ----------------------------------------------------------------
# The stack is disposable; repeated runs must not leave orphans behind.
cleanup() {
    local rc=$?
    trap - EXIT
    "${COMPOSE[@]}" down --volumes --remove-orphans >/dev/null 2>&1 || true
    exit "$rc"
}
trap cleanup EXIT

cd -- "${HARNESS_DIR}"

cat <<BANNER
=== Sparkplug B conformance harness ============================================
  mode            : ${MODE}
  broker          : ${SPARKPLUG_BROKER_URI}
  group id        : ${SPARKPLUG_GROUP_ID}
  edge node id    : ${SPARKPLUG_EDGE_NODE_ID}
  device id       : ${SPARKPLUG_DEVICE_ID}
  timeout         : ${SPARKPLUG_TIMEOUT}s

  'simulate' (default) asserts a synthetic but spec-valid message flow: the
  harness plays the Edge Node that publishes it and the Host Application that
  asserts it, so it needs nothing but Docker. The run ends by dropping the
  connection of the Edge Node without a DISCONNECT, so the broker publishes
  the Will that carries the NDEATH.

  To assert a REAL ESP32-S3 gateway instead, point the gateway at this broker
  (mqtt://<host-ip>:1883, or mqtt://host.docker.internal:1883 for a broker
  published on the host) and run:

      MODE=observe SPARKPLUG_BROKER_URI=mqtt://broker:1883 \\
          bash scripts/run-conformance.sh

  The observe mode also accepts --expect-ndata, --expect-ddeath and
  --expect-rebirth through the client container.
==============================================================================
BANNER

set +e
"${COMPOSE[@]}" up --build --abort-on-container-exit
compose_rc=$?
set -e

# `compose up --abort-on-container-exit` does not reliably propagate a
# container failure: when the broker cannot even start (for instance because
# the host port is taken) Compose prints the error but still exits 0. The
# container states are therefore inspected directly, because they are the
# only trustworthy source of truth here.
broker_state="$(docker inspect -f '{{.State.Status}}' "${BROKER_CONTAINER}" 2>/dev/null || true)"
client_state="$(docker inspect -f '{{.State.Status}}' "${CLIENT_CONTAINER}" 2>/dev/null || true)"
client_rc="$(docker inspect -f '{{.State.ExitCode}}' "${CLIENT_CONTAINER}" 2>/dev/null || true)"

if [[ "${broker_state}" != "running" && "${broker_state}" != "exited" ]]; then
    echo "ERROR: the broker container is '${broker_state:-missing}'." >&2
    echo "       It never started; check the broker log above (a taken host" >&2
    echo "       port is the usual cause -- set SPARKPLUG_BROKER_HOST_PORT to a" >&2
    echo "       free port)." >&2
fi

if [[ "${client_state}" == "exited" && "${client_rc}" =~ ^[0-9]+$ ]]; then
    # The client ran to completion: its exit code is the verdict.
    exit_code="${client_rc}"
elif [[ "${compose_rc}" -ne 0 ]]; then
    exit_code="${compose_rc}"
else
    # The client never ran, so the harness could not produce a verdict.
    exit_code=1
fi

echo
if [[ "${exit_code}" -eq 0 ]]; then
    echo "RESULT: conformance harness PASSED (exit 0)."
elif [[ "${exit_code}" -eq 2 ]]; then
    echo "RESULT: conformance harness FAILED - one or more assertions did not hold (exit 2)."
else
    echo "RESULT: conformance harness ERROR (exit ${exit_code})."
fi

exit "${exit_code}"
