# Hardware test catalogue — the tests that need a board

Everything in this document needs a physical gateway, a physical node, or
both. None of it runs in the CI, and the CI says so: the tests of this
catalogue carry the `[hw]` tag and the CI profile of the test application
leaves them out.

The separation is mechanical, not a convention:

- `firmware/gateway/test/main/test_main.c` runs `unity_run_all_tests()` in the
  **board** profile and `unity_run_tests_by_tag("[hw]", true)` in the **logic**
  profile;
- the **logic** profile is the one the CI builds (it is also the profile that
  disables the Octal PSRAM, which QEMU does not emulate);
- a test that needs hardware and is not tagged `[hw]` is a bug in the test
  suite, and the CI will find it.

## How to run the board profile

```bash
source $HOME/esp/esp-idf/export.sh

# The profile of the board: every test, including the [hw] ones.
#
# The generated sdkconfig is removed first. It is a generated file, and a
# stale one keeps the old value of a symbol that sdkconfig.defaults has
# changed — the build then succeeds with the previous configuration and the
# change looks like it had no effect. The CI profile of the application does
# the same thing for sdkconfig.qemu.
rm -f firmware/gateway/test/sdkconfig

idf.py -C firmware/gateway/test build
idf.py -C firmware/gateway/test -p /dev/ttyACM0 monitor
```

The console of this carrier is the secondary USB-Serial/JTAG, so the Unity
output appears on `/dev/ttyACM*` and not on the UART0 pins.

The result of the run has to be copied into the run sheet of the change: the
summary line (`N Tests M Failures K Ignored`) and the version of the image.

After the suite the application restarts the unit on purpose, so the end of
the run is an event the environment can observe. Without it the chip would
sit in the idle task and the emulation would only stop at the CI timeout.

## A green summary is not a green run

The Unity summary is printed **before** the FreeRTOS stack overflow detector
speaks. A run whose stack is too small looks like this:

```
114 Tests 0 Failures 0 Ignored
OK
I (8943) gateway_test: Main task stack headroom after the run: 0 bytes of 8192
***ERROR*** A stack overflow in task main has been detected.
```

The suite passed and the application crashed. This is not a theoretical
case: it is what happened on the 8192-byte main task stack, in both
profiles, and the CI reported success over it because the gate only looked
at the summary.

`tools/ci/run-device-tests.sh` therefore fails the run when:

| Condition | Why |
|---|---|
| The log contains a crash after the summary (`***ERROR***`, `stack overflow`, `Guru Meditation`, `abort() was called`, `Rebooting...`) | A crashed application is not a passing run, whatever the summary says. |
| The headroom of the main task was not measured | The measurement is missing, so the run cannot be judged. |
| The headroom is below the floor (1024 bytes, overridable with `HEADROOM_FLOOR_BYTES`) | The next growth of the suite would turn the missing headroom into an overflow. |

The headroom line is the measurement that makes the value of
`CONFIG_ESP_MAIN_TASK_STACK_SIZE` a fact rather than a guess:

```
I (8943) gateway_test: Main task stack headroom after the run: 7504 bytes of 16384
```

The whole suite runs in `app_main()`, in the main task, so the stack has to
hold the deepest single test and not the average one. Adding a test can
therefore break the build of the suite without touching anything that looks
like a stack — the headroom floor is what turns that into a failure that is
visible in the CI.

The product reports its own headroom in the same way, at the end of the
initialisation chain of `app_main()`:

```
I (1451) gateway: Main task stack headroom after the init chain: 5648 bytes of 8192
```

## The catalogue

| Id | What it proves | Needs | How | Evidence |
|---|---|---|---|---|
| `HW-BOARD-01` | The connected board is the N16R8 of the carrier: 16 MB of flash and 8 MB of Octal PSRAM | Gateway | The `[board_profile]` tests of the board profile | The Unity summary of the board profile |
| `HW-BOARD-02` | A RGB565 framebuffer of 170×320 fits in the PSRAM | Gateway | The second `[board_profile]` test | The heap log of the test |
| `HW-RGB-01` | The onboard WS2812 on GPIO48 uses the GRB order and lights up | Gateway | The `[board_rgb]` test | The Unity summary and a photo of the LED |
| `HW-NOW-01` | The ESP-NOW radio receives a node and the Wi-Fi AP stays up | Gateway + 1 node | The radio soak of the [stress plan](stress-plan.md) | The RSSI and the report counter |
| `HW-STRESS-01` | Ten nodes report for 24 hours without losing their identity | Gateway + 10 nodes | The [stress plan](stress-plan.md), half B | The run sheet of the soak |
| `HW-NOW-02` | ESP-NOW and Wi-Fi share the same channel | Gateway + 1 node | Read the channel of the AP and of the node | The channel log of both |
| `HW-OTA-01` | A signed image is installed, confirmed and rolled back when it does not answer | Gateway | The OTA run of `docs/ota/ota-security.md` | The OTA state before and after |
| `HW-OTA-02` | The rollback leaves the gateway on the previous image | Gateway | The OTA run of `docs/ota/ota-security.md` | The boot slot and the rollback state |
| `HW-LCD-01` | The 8-pin ST7789V2 shows the boot splash of the selected brand | Gateway + display | The UI run of the display phase | A photo of the screen |
| `HW-LCD-02` | The backlight is controlled by the GPIO of the carrier | Gateway + display | The UI run of the display phase | The GPIO reading |
| `HW-WIFI-01` | The gateway serves the AP and, when configured, associates with the site network | Gateway | The Wi-Fi run | The association log |

## The rules of this catalogue

1. A test that needs hardware is tagged `[hw]`. There is no second list: the
   tag IS the list.
2. A test that does not need hardware is never tagged `[hw]`, even when it is
   inconvenient for the CI. The CI profile of the application builds without
   the Octal PSRAM, so a test that reads the PSRAM is a hardware test by
   definition.
3. The evidence of a hardware test is the summary line of the Unity run and
   the version of the image. A test without evidence did not run.
4. A hardware test that fails is a defect of the firmware, not a defect of the
   test. The test encodes the behaviour the product promises.

## What the CI covers, and what it does not

| Area | In the CI | On the board |
|---|---|---|
| Protocol codec (ESP-NOW frames) | Yes, on the host | — |
| Sparkplug B payload, sequence and bdSeq | Yes, on the host and under QEMU | — |
| Sparkplug B against a real broker | Yes, with the conformance harness | — |
| Device registry, command parsing | Yes, on the host | — |
| Brand profile and OTA manifest rules | Yes, on the host and under QEMU | — |
| Firmware build, sizes and partitions | Yes | — |
| The Octal PSRAM of the N16R8 | No | `HW-BOARD-01`, `HW-BOARD-02` |
| The RMT of the RGB LED | No | `HW-RGB-01` |
| The radio | No | `HW-NOW-01`, `HW-NOW-02`, `HW-STRESS-01` |
| The OTA on the device | No | `HW-OTA-01`, `HW-OTA-02` |
| The display | No | `HW-LCD-01`, `HW-LCD-02` |
| The Wi-Fi association | No | `HW-WIFI-01` |
