# design — issue #16 gateway product roll-up

## The defect

### What happens

The whole Unity suite runs inside `app_main()`, which ESP-IDF runs in the
**main task**. The stack of that task is `CONFIG_ESP_MAIN_TASK_STACK_SIZE`.

That value was raised from the IDF default of 3584 to 8192 in commit
`bac1d96`, because `nvs_flash_init()` overflowed it. The value was never
revisited after the suite grew: today it runs 114 tests, and the deepest of
them needs more than 8192 bytes of stack below the current frame.

The symptom is a green suite followed by a crash:

```
114 Tests 0 Failures 0 Ignored
OK
I (8943) gateway_test: Main task stack headroom after the run: 0 bytes of 8192
I (8943) main_task: Returned from app_main()
***ERROR*** A stack overflow in task main has been detected.
```

`UNITY_END()` prints the summary and the verdict; the FreeRTOS overflow
detector only speaks on the next context switch, which happens after
`app_main()` returns. The gate in `tools/ci/run-device-tests.sh` looked at
the summary and the verdict and nothing else, so it reported success.

The consequence is not a flaky test. It is a **silent hole in the CI**: the
whole on-target suite has been green over a crashing application since the
suite outgrew the stack.

### Why the suite grew past it

The suite is not one function. Each `TEST_CASE` is a function that Unity
calls from the runner, and the runner calls them all on the same stack.
`unity_run_all_tests()` does not reset the stack between cases: the high
water mark of the run is the deepest single case, and 114 cases means 114
opportunities to be the deepest one. The mbedtls ECDSA verification of
`test_ota_manifest.c` and the Sparkplug payload codecs are the heavy ones.

### Decision

**Raise the main task stack of the test application to a measured value, and
make the gate fail on the class of failure.**

| Option | Why it was rejected |
|---|---|
| Raise the stack and stop there | The same defect returns the next time the suite grows. The gate would still be blind to it. |
| Run the suite in a dedicated task with a big stack | Correct in principle, but it moves the overflow from the main task to a task whose stack nobody measures, and it changes the application the CI runs. The measurement below is what makes the value maintainable. |
| **Raise the stack and gate on the headroom** | **Chosen.** The measurement is the artefact: it says how much room is left, the gate says how much room is enough, and a suite that grows past the floor fails before it becomes a crash. |

### The measured value

| Stack | Headroom after the 114-test suite | Overflow |
|---|---|---|
| 8192 | 0 bytes | yes, right after the summary |
| 16384 | 7504 bytes | no |

16384 is chosen with a 7504-byte margin, not as the smallest value that
works. The floor of the gate is 1024 bytes, which leaves room for the suite
to grow by roughly 6.5 KiB of stack depth before the gate objects.

The product keeps its own 8192-byte stack. `app_main()` of the gateway
brings up the NVS driver, the brand layer, the display contracts, the
Wi-Fi driver and the OTA manager; that chain is measured too, and the
measurement is reported at boot.

## Memory budget

| Item | Where | Size | Note |
|---|---|---|---|
| Main task stack (product) | internal DRAM, at startup | 8192 B | measured at boot; the headroom is logged |
| Main task stack (test application) | internal DRAM, at startup | 16384 B | was 8192; measured headroom 7504 B |
| Octal PSRAM | external | 8 MiB | `Found 8MB PSRAM device`, `SPI SRAM memory test OK` |
| Internal heap at boot | internal DRAM | 331 KiB | after the reservation of 32 KiB for DMA |
| Application image | flash | 799,504 B | 25.42 % of the 3 MiB `ota_0` partition |
| SPIFFS | flash | 4 MiB at `0x620000` | the Preact SPA is not built yet; the 200 KiB gzipped budget is not checked |
| RGB565 framebuffer | PSRAM, when the display lands | 108,800 B | contract constant only; no allocation yet |

Raising the test stack by 8 KiB out of a 331 KiB heap is 2.4 %. It is the
price of a suite that runs 114 cases on one stack.

## The OTA rollback, as demonstrated

The rollback is not new in this change; it is the evidence of issue #13 and
the evidence of AC-05 of issue #16. It was reproduced on the board for this
roll-up.

```
   host                     bootloader                ota_1 (defective)      ota_0 (good)
     |                           |                           |                      |
     | esptool write_flash      |                           |                      |
     | 0x320000  --------------->|                           |                      |
     | make-otadata.py --slot ota_1 --state new              |                      |
     | esptool write_flash      |                           |                      |
     | 0xf000 ----------------->|                           |                      |
     |                           |                           |                      |
     | reset                    |                           |                      |
     |-------------------------->| "Loaded app from          |                      |
     |                           |  partition at 0x320000"  |                      |
     |                           |-------------------------->| abort()               |
     |                           |                           |                      |
     |                           | otadata: ota_1 = aborted  |                      |
     | reset                    |                           |                      |
     |-------------------------->| "No factory image,        |                      |
     |                           |  trying OTA 0"            |                      |
     |                           | "Loaded app from partition at 0x20000" -------->|
```

The `otadata` partition after the rollback reads:

```
record 0: seq=2 label='ota_1' state=4 (aborted) crc=0x55f63774 crc_ok=True
```

`state=4` is `ESP_OTA_IMG_ABORTED`. The bootloader started the defective
image, the image never confirmed itself, and the bootloader discarded it and
went back to `ota_0`, which is the image the unit is running now.

## Hardware-affecting decisions

**None.** This change assigns no GPIO, writes no register of the display
module and changes no partition layout.

The hardware hold is unchanged and is restated here so it is not weakened
by a document that talks about the hardware:

- GPIO35, GPIO36 and GPIO37 are consumed by the Octal PSRAM of the N16R8 and
  are unavailable for external use.
- GPIO48 drives the onboard WS2812 RGB LED.
- GPIO19 and GPIO20 are the native USB D− and D+.
- The 8-pin display contact map (1 GND, 2 VCC, 3 SCL, 4 SDA, 5 RES, 6 DC,
  7 CS, 8 BLK) is **display-side**, not a GPIO assignment. GPIO22 as the
  backlight stays a candidate, pending physical verification.
- The withdrawn `0.1155 × 0.1155 mm` pixel pitch is not used, and no
  geometry is derived from it.

## What was found in the active change folders

`openspec/changes/issue-9-gateway-firmware-foundation/apply-progress.md`
carries the three markers of an unresolved merge conflict committed to
`main`: the opening marker, the separator and the closing marker. They are
the residue of a `git stash` that was applied with a conflict and committed
as it was. The authoritative copy of that change is
`openspec/changes/archive/2026-09-29-issue-9-gateway-firmware-foundation/`,
which the archive rule preserves verbatim.

The markers are a defect of the repository record, not of the firmware. They
are removed in this change, and the resolution is taken from the archived
copy, which is the one the workflow declares final.

## Power budget

Not affected. This change touches the gateway only, and the gateway is
mains-powered. The node firmware path (wake → read → ESP-NOW → deep sleep)
is not on disk and is not touched.

## Sequence diagrams that do not apply here

The design rule asks for sequence diagrams of the ESP-NOW, OTA, WebSocket
and SPIFFS-served Preact flows. Only the OTA flow exists in this change's
scope, and it is above. The other three belong to subsystems that are
contracts (ESP-NOW) or not on disk (the SPA), and drawing them here would
describe behaviour that does not exist.
