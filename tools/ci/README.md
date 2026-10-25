# CI tooling

The scripts of this directory are the pieces of the firmware CI that are not
plain `idf.py` calls. They run the same way locally and in the CI, so a
developer can reproduce a red job without waiting for the runner.

| Script | What it does | Fails when |
|---|---|---|
| `report-size.py` | Sizes the application binary against the partition table, publishes the headroom | The image does not fit, or the headroom is below the threshold |
| `check-reproducible.sh` | Builds the firmware twice from scratch and compares the artefacts | The two builds are not identical |
| `run-device-tests.sh` | Builds the CI profile of the test application and runs it under QEMU | A test fails, the application hangs, or the summary is missing |
| `check-no-secrets.sh` | Scans the published artefacts for private keys and local secrets | A secret is found |

Every script takes `--help` (the Python one) or prints its usage (the shell
ones), and none of them needs a network connection.

## The environment

The scripts that build firmware need the ESP-IDF environment:

```bash
source $HOME/esp/esp-idf/export.sh
```

The device tests also need the QEMU build of the ESP32-S3, which is an
optional tool of ESP-IDF:

```bash
python "$IDF_PATH/tools/idf_tools.py" install qemu-xtensa
```

## Running them by hand

```bash
# The size report.
python3 tools/ci/report-size.py --project-dir firmware/gateway

# The reproducible build (two full builds, a few minutes).
tools/ci/check-reproducible.sh firmware/gateway build/reproducible

# The device tests under QEMU.
tools/ci/run-device-tests.sh firmware/gateway/test build/device-tests 300

# The secret scan of a published artefact.
tools/ci/check-no-secrets.sh firmware/gateway/build/gateway.bin
```

## The size report

`report-size.py` reads the **binary** partition table of the build, the one
the bootloader reads, and compares the application binary with the smallest
application partition: that is the limit the build itself enforces. The
report also lists the whole table and the budget of the embedded SPA, so the
free space of the partitions that the application does not use is visible.

The threshold is a gate, not a comment:

```bash
python3 tools/ci/report-size.py --min-free-percent 15
```

fails when less than 15 % of the application partition is free.

## Why the QEMU profile is a different configuration

The QEMU build of ESP-IDF v5.4 does not emulate the Octal PSRAM of the
N16R8, so the chip aborts at boot with `Failed to init external RAM!`. The
CI profile therefore builds without PSRAM, and the tests that need it carry
the `[hw]` tag and are left out.

The SHA accelerator is emulated but broken, so the CI profile uses the
software implementation of mbedTLS. Both decisions are in
`firmware/gateway/test/sdkconfig.qemu.defaults`, and both are documented in
`odd/tasks/issue-14-ci-and-conformance-tests.md`.
