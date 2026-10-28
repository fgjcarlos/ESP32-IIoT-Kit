# ESP32-S3-WROOM-1-N16R8 gateway candidate

**Current status: selected candidate; connected SoC and memory are probe-confirmed, and onboard GPIO48 RGB behavior passed a visual smoke check; carrier model/revision and external display remain unvalidated.** This profile replaces the failed ESP32-C6-LCD-1.47 candidate for gateway planning. It distinguishes read-only probe observations and the RGB smoke result from exact module/carrier identity and unknown external-display details.

## Quick path

1. Use `esp32s3` when firmware work begins.
2. Reserve GPIO35, GPIO36, and GPIO37: the N16R8 module uses them for Octal PSRAM.
3. Do not wire the display until physical labels, board-side GPIO mapping, supply and logic levels, and backlight requirements are validated. The candidate map below is the target of that validation, not an approval to wire.
4. Keep autonomous dashboard and OTA as requirements; use the ODD-3 custom 16 MB partition layout and validate its runtime budgets on hardware.

## Confirmed module facts

| Area | Confirmed fact | Planning consequence |
|---|---|---|
| Module | ESP32-S3-WROOM-1-N16R8 | The gateway candidate uses the ESP-IDF `esp32s3` target. |
| Flash | 16 MB Quad-SPI flash | ODD-3 configures custom partitions: two 3 MiB OTA slots and a 4 MiB SPIFFS partition. |
| PSRAM | 8 MB Octal-SPI PSRAM | Runtime memory budgets still require build and hardware measurements. |
| Reserved GPIO | GPIO35, GPIO36, and GPIO37 are consumed by Octal PSRAM | These GPIOs are unavailable for external peripherals, including the display. |
| Ambient operating range | −40 to 65 °C | Confirm enclosure and installation conditions separately. |

These facts are from the [official Espressif ESP32-S3-WROOM-1/WROOM-1U datasheet](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf).

## ODD-3 flash and RAM budget

The gateway now uses ESP-IDF custom partition-table mode with `firmware/gateway/partitions.csv`. It preserves the 16 MB flash and Octal PSRAM configuration while reserving space for the autonomous dashboard and OTA requirements.

| Partition | Offset | Size | Purpose |
|---|---:|---:|---|
| `nvs` | `0x9000` | `0x6000` | Persistent configuration |
| `otadata` | `0xF000` | `0x2000` | OTA selection metadata |
| `phy_init` | `0x11000` | `0x1000` | PHY initialization data |
| `ota_0` | `0x20000` | `0x300000` (3 MiB) | First gateway application image |
| `ota_1` | `0x320000` | `0x300000` (3 MiB) | Second gateway application image |
| `spiffs` | `0x620000` | `0x400000` (4 MiB) | Dashboard assets and OTA staging headroom |

The assigned regions end at `0xA20000`. There is also an alignment gap from the end of `phy_init` at `0x12000` to the start of `ota_0` at `0x20000`: `0xE000` (56 KiB). The trailing range from `0xA20000` to `0x1000000` is `0x5E0000` (5.875 MiB) and remains intentionally unallocated. Together, these unassigned ranges total `0x5EE000` (5.9296875 MiB). Bootloader and partition-table bytes occupy their own flash outside the app/data partitions and are not unallocated space. No speculative storage or coredump partition is included.

The current minimal firmware baseline is 212,640 bytes. The gateway application target is under 1.5 MiB and its hard limit is under 2 MiB; 3 MiB OTA slots leave margin for measured growth. SPIFFS has a target budget of at most 500 KiB for gzipped dashboard assets and reserves future headroom for staging a node image of at most 1.5 MiB. These bundle and staging sizes are targets, not measured artifacts.

### Measured Unity runtime evidence

A separate Unity test project built and ran on the target. Before allocating and freeing one 108,800-byte RGB565 framebuffer-sized 8-bit PSRAM block, `board_profile` reported:

| Measurement | Result |
|---|---:|
| Free internal heap | 311,967 bytes |
| Free PSRAM | 8,386,148 bytes |
| Largest PSRAM block | 8,257,536 bytes |
| 108,800-byte PSRAM allocation | Passed |
| Main task stack headroom after the run | 656 bytes of 8192 |

These heap values come from the Unity test app (81 tests), not the eventual full gateway firmware. The successful allocation validates PSRAM allocation only; it does not validate display DMA or throughput. ESP-IDF documents that ESP32-S3 DMA descriptors cannot reside in PSRAM and that PSRAM DMA bandwidth is limited.

The full on-device run of 2026-10-24 reported `81 Tests 0 Failures 0 Ignored` and `OK` over the native USB console, with the stack headroom measured in the same run. The project needs `CONFIG_UNITY_ENABLE_64BIT=y`, because the Sparkplug tests assert on 64-bit values (`seq`, `bdSeq`, timestamps), and `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192`, because `nvs_flash_init()` does not fit in the default 3584-byte stack (see the two defects above).

## Provisional carrier information

The photographed physical unit is cross-matched to user-supplied seller diagrams showing a dual-USB-C carrier and WS2812 RGB LED. An on-target, low-brightness smoke test visually confirmed red → green → blue → off on GPIO48. The exact carrier model/revision and remaining board wiring are still unverified. The following pinout remains planning context from a **user-supplied candidate image**, not manufacturer documentation. Validate physical markings before assigning other peripheral GPIOs.

### Candidate carrier diagram (provisional)

The image labels the following header signals:

```text
3V3  5V  GND  RST
GPIO0–GPIO21
GPIO35–GPIO48
```

The labels identify candidate **carrier header signals**. They do not establish board-side wiring for the display and must not be conflated with the display connector contacts.

| Candidate image label | Provisional interpretation | Planning status |
|---|---|---|
| GPIO0–GPIO21 | Candidate exposed GPIO set, subject to physical board/revision validation | No display assignments established. |
| GPIO35–GPIO37 | The image labels these header positions, but the confirmed N16R8 module consumes them for Octal PSRAM | Unavailable for external peripherals; do not treat them as usable GPIOs. |
| GPIO38–GPIO47 | Candidate exposed GPIO set, subject to physical board/revision validation | No display assignments established. |
| GPIO48 / RGB_LED | Seller schematic cross-matched to the board photo shows GPIO48 driving the WS2812; a target smoke test visually showed red → green → blue → off | Functionally confirmed on this physical unit by low-brightness RMT smoke; reserve for the onboard LED. This does not identify the carrier model/revision. |
| GPIO19 / USB_D− | The image identifies GPIO19 as native USB D− | Reserve while native USB is needed; do not propose it for the LCD. |
| GPIO20 / USB_D+ | The image identifies GPIO20 as native USB D+ | Reserve while native USB is needed; do not propose it for the LCD. |
| GPIO0 / BOOT | The image identifies GPIO0 as BOOT | Preserve boot-function considerations until the exact carrier is validated. |
| 3V3, 5V, GND, RST | The image labels these carrier header signals | Electrical characteristics and board wiring remain unverified. |

The candidate image does not confirm a carrier model, revision, electrical limits, or board-side display wiring. Do not treat it as manufacturer confirmation for the selected unit.

### Observed connected board (read-only probe)

A user-authorized esptool 4.12 probe of the connected board observed:

| Observation | Result | Evidence boundary |
|---|---|---|
| SoC | ESP32-S3 QFN56, revision v0.2; 40 MHz crystal | Reported by the ROM bootloader; exact board/module marking still needs visual confirmation. |
| PSRAM | 8 MB embedded PSRAM | Reported by esptool; consistent with the N16R8 candidate. |
| SPI flash | 16 MB; manufacturer ID `0x5E`, device ID `0x4018` | Reported by a read-only `flash_id` query. |
| USB interface | USB-Serial/JTAG | Reported by the connected target. |
| Onboard RGB | GPIO48 WS2812; low-brightness red → green → blue → off was visually observed | Seller schematic was cross-matched to the photographed layout; Unity/RMT returned OK and the user confirmed the physical colors. This is not manufacturer verification of carrier identity. |

These observations confirm the connected SoC and memory capacities, but do not independently identify the carrier model/revision or verify the display wiring. The probes did not write, erase, or flash the device.

### Boot mode of the connected unit: corrected diagnosis

**Status: resolved on 2026-10-24. The unit boots the application from the SPI flash. The earlier "GPIO46 held low" finding of this file was a misdiagnosis and has been withdrawn: there is no external pull-down on GPIO46, and GPIO46 does not select the boot mode on its own.**

The ESP32-S3 samples its strapping pins at reset. The datasheet (section 2.6, table 2-11) defines the boot mode as:

| Boot mode | GPIO0 | GPIO46 |
|---|---|---|
| SPI Boot (default) | 1 | any value |
| Joint Download Boot | 0 | 0 |
| Invalid combination | 0 | 1 |

Two facts follow, and both contradict the previous reading of this file.

**1. GPIO46 low does not block the SPI boot.** SPI Boot only requires GPIO0 = 1; the value of GPIO46 is irrelevant in that combination. The unit reads GPIO46 low because the chip itself pulls the pad down: table 2-9 of the datasheet gives GPIO0 a pull-up (bit 1) and GPIO45 and GPIO46 a pull-down (bit 0) at reset, so a floating GPIO46 reads 0 by design.

| Measurement | Value | Reading |
|---|---|---|
| `IO_MUX_GPIO46_REG` (`0x600090BC`) after reset | `0x00000a80` | `FUN_PD` (bit 7) set: the pad is held low by the chip's own internal pull-down, not by the carrier |
| `IO_MUX_GPIO46_REG` with `FUN_PD` cleared | `0x00000280` | `GPIO_IN1` bit 14 reads 1: the pad floats high, so **there is no external pull-down** on the carrier |
| `GPIO_OUT1`/`GPIO_ENABLE1` bit 14 driven high | `GPIO_IN1` bit 14 = 1 | The pad follows the GPIO driver, so nothing outside the GPIO peripheral holds it down |

The earlier conclusion ("the pad is held down by something outside the GPIO peripheral's influence") came from reading `GPIO_IN1` while the internal pull-down was still enabled. That read says nothing about the carrier.

**2. What really forced the ROM downloader was the reset, not the strap.** The USB-Serial/JTAG peripheral of the ESP32-S3 maps DTR to IO0 and RTS to EN:

- The *connection* reset of esptool (`--before default_reset`, the `USBJTAGSerialReset` sequence) asserts DTR, so IO0 goes low and the chip is sampled in Joint Download Boot (GPIO0 = 0, GPIO46 = 0). Every diagnostic that opened a ROM session kept the unit in the downloader.
- `idf.py flash` ends with `--after hard_reset`, which pulses RTS only and leaves DTR deasserted, so IO0 stays high and the chip boots the application (GPIO0 = 1).

Consequence: **flashing the board is enough to run it.** Measured on the connected unit:

| Step | Observed result |
|---|---|
| `idf.py -p /dev/ttyACM0 flash` | Ends with `Hard resetting via RTS pin`; the application then boots from flash on its own |
| Boot reason | `rst:0x15 (USB_UART_CHIP_RESET),boot:0x8 (SPI_FAST_FLASH_BOOT)` |
| ROM downloader | Only entered when a host tool connects with esptool's default reset |
| `idf.py monitor` | Same RTS-only reset, so the monitor also reaches the application |

`tools/board/boot_from_flash.py` reproduces the reset of the ESP-IDF monitor (an RTS pulse with DTR deasserted) and captures the console over the native USB port. It is the tool to use when the board has to be restarted without a host-side downloader session, for example after a power cut.

**Do not drive GPIO46 high from the host.** The only invalid strapping combination is GPIO0 = 0 with GPIO46 = 1, and a host that forces GPIO46 high while it holds the chip in the downloader lands exactly in that combination.

### Two defects the on-device run uncovered

Running the firmware on the unit was possible only after the boot diagnosis above, and it immediately exposed two defects that the host test runner could not see.

**The main task stack overflowed.** `app_main()` calls `nvs_config_init()`, which calls `nvs_flash_init()`. The NVS driver needs more stack than the 3584-byte default of `CONFIG_ESP_MAIN_TASK_STACK_SIZE`, and the run aborted with:

```text
***ERROR*** A stack overflow in task main has been detected.
```

The on-device Unity project now runs with `CONFIG_ESP_MAIN_TASK_STACK_SIZE=8192` and reports the measured headroom after the run: **656 B free of 8192 B**, so the peak consumption of the main task is 7536 B. The gateway carries the same setting, because it makes the same call from the same task.

**Three NVS keys were longer than NVS allows.** NVS answers a key longer than 15 characters with `ESP_ERR_NVS_KEY_TOO_LONG`, and the gateway used `mqtt_edge_node_id` (18), `mqtt_keepalive_sec` (19) and `mqtt_ddeath_timeout_sec` (23). The configuration load therefore failed on the target with:

```text
E (1468) gateway: MQTT bridge start failed: ESP_ERR_NVS_KEY_TOO_LONG
```

and no MQTT session was ever established. The keys are now `mqtt_edge_id`, `mqtt_keepalive` and `mqtt_ddeath_sec`, they live in `components/sparkplug_b/include/sparkplug_b_nvs.h`, and `components/sparkplug_b/test/test_nvs.c` fails the build if any key of the namespace exceeds the limit.

### Candidate carrier mechanical drawing (provisional)

A second user-supplied dimension drawing appears to report a **57.15 × 27.94 mm PCB outline**, **53.34 × 25.40 mm hole-center spacing**, and **2.54 mm (100 mil) header pitch**. These are drawing-reported values, not physical measurements, and the drawing has not yet been matched to the delivered carrier/revision. Do not release an enclosure or mounting design from these values alone.

| Other reported carrier fact | Source and confidence | Required confirmation |
|---|---|---|
| 44-pin dual-USB S3-WROOM-1 board | The [candidate seller listing](https://es.aliexpress.com/item/1005006418608267.html) identifies an S3 WROOM-1 44-pin dual-USB board, but its fetched product page did not expose complete specifications. | Compare the delivered board's markings, revision, and pinout with the listing. |
| Two USB-C ports: CH343P USB-UART and native USB | The [third-party YD-ESP32-S3 / DevKitC-style reference](https://github.com/profharris/YD-ESP32-S3_ESP32-S3-WROOM-1_Dev) reports this arrangement. Its README is marked work in progress. | Verify both ports and their functions on the exact unit. |
| WS2812 RGB LED on GPIO48 | User-supplied seller schematic matches the photographed layout; an on-target test and the user's visual observation confirmed red → green → blue → off. | Functionally confirmed on this physical unit; exact carrier make/revision remains unknown. |

## External display: partially confirmed profile

The user confirms that the selected unit has **8 pins**, an **ST7789V2** controller, a resolution of **170(H) RGB × 320(V)**, and a **4-wire SPI** interface. The following contact map comes from a **user-supplied pinout image**, not independent manufacturer verification. The numbers identify display connector contacts, **not ESP32 GPIO assignments**.

| Display contact | Image label | User-supplied function |
|---|---|---|
| 1 | GND | Power ground |
| 2 | VCC | Display main power input; reported operating voltage is 3.3 V, while logic input levels remain unverified |
| 3 | SCL | 4-wire SPI clock |
| 4 | SDA | 4-wire SPI data input |
| 5 | RES | Display reset; active polarity unspecified |
| 6 | DC | LCD data/command selection |
| 7 | CS | Display panel selection |
| 8 | BLK | Backlight control; the image says it is on by default and a low level turns it off |

### Reported display specifications

All values in this table are **user/listing-reported**, not physically verified on the delivered unit. They must not be supplemented with specifications from the separate 30-pin variant.

| Specification | Reported value | Source confidence | Unresolved interpretation / required confirmation |
|---|---|---|---|
| Controller and interface | ST7789V2; 4-wire SPI | User-confirmed profile; not hardware-validated | Confirm controller marking and interface behavior on the physical unit. |
| Resolution | 170(H) RGB × 320(V) | User-confirmed profile; not hardware-validated | Confirm active resolution during display bring-up. |
| Operating voltage | 3.3 V | User/listing-reported; not physically verified | This does not establish display logic input levels or safe MCU signal levels. |
| Nominal display size | 1.9 in | Reaffirmed by the user; not hardware-validated | The drawing-reported dimensions below are consistent with this nominal size; compare them with the exact delivered module before enclosure design. |
| Operating temperature | −20 to 70 °C | User/listing-reported; not physically verified | Confirm suitability for the actual enclosure and installation environment. |
| Viewing direction | 12 o'clock | User/listing-reported; not physically verified | Confirm viewing orientation on the physical panel. |
| Backlight construction | Two white LEDs in parallel | User/listing-reported; not physically verified | Confirm the internal backlight arrangement and BLK electrical requirements. |
| Operating current | 20 mA | User/listing-reported; not physically verified | Scope is explicitly unclear: it may describe the whole module, backlight, or another condition; it is not established as a per-LED or total-backlight rating. |

### Drawing-reported mechanical dimensions

The following dimensions come from the **latest user-provided mechanical drawing**. They are drawing-reported values, **not physical verification** of the delivered 8-pin module.

| Feature | Drawing-reported dimension |
|---|---:|
| Active area (AA) | 42.720 × 22.695 mm |
| LCD outline | 48.520 × 24.800 mm |
| Backlight outline | 49.720 × 25.800 mm |
| PCB outline | 62.000 × 29.000 mm |
| Mounting-hole center spacing | 58.000 × 25.000 mm |

**Nominal-size check:** `sqrt(42.720² + 22.695²) ≈ 48.37 mm`; `48.37 / 25.4 ≈ 1.904 in`, which is consistent with the nominal 1.9 in size. This is a calculation from the drawing-reported AA dimensions, not physical verification.

**Warning — withdrawn seller value:** The user withdrew the seller's incorrect `0.1155 × 0.1155 mm` pixel-pitch value. Do not use it or geometry derived from it, and do not infer a replacement pixel pitch. Compare the drawing with the exact delivered 8-pin module before enclosure design.

A full RGB565 framebuffer is 170 × 320 × 2 = 108,800 bytes (106.25 KiB), and allocation of that size in PSRAM passed in the minimal Unity app. Placement in the full gateway firmware, display DMA suitability, and throughput remain unmeasured.

### Remaining open items

- Compare the drawing-reported dimensions with the exact delivered 8-pin module, including its physical labels, before enclosure design.
- Verify the candidate mapping published below against the carrier in hand: the mapping is a proposal for that verification, not an approval to wire.
- Verify the VCC and logic voltage requirements and safe signal levels; the reported 3.3 V operating voltage does not determine the panel's permitted input range, its input current limits, or its behaviour with unpowered inputs.
- Verify what the reported 20 mA measures and the backlight current/control requirements; no LED driver or current-limit circuit is established here.
- Confirm the carrier wiring before making any electrical connection.

The [AliExpress display listing](https://es.aliexpress.com/item/1005008546903854.html) did not expose complete specifications. Its associated PDF appears to describe a distinct 30-pin variant; **do not apply that variant's pinout to this user's 8-pin unit**.

No display GPIO assignment or electrical connection is approved until the remaining open items are validated. The candidate mapping below exists so that validation has a concrete target.

## Display connection map — candidate, not approved for wiring

**Status: candidate mapping published for review. No wiring, no supply and no
first power-up are authorized.** Neither Authorization A (power-only electrical
characterization) nor Authorization B (signal and functional bringup) is
granted; see the release state below. Issue #67 publishes the candidate mapping
so the integrator knows what to look for on the carrier; every row stays
provisional until the evidence in the bring-up protocol is recorded.

Procedure: `docs/hardware/display-bringup-protocol.md`. The two documents are
one record split in half: the protocol produces the evidence, this profile
holds the map and the block.

### Candidate mapping (display contact → signal → GPIO candidate)

| Display contact | Panel signal | Carrier signal (candidate) | GPIO candidate | Basis | Status |
|---|---|---|---|---|---|
| 1 | GND | GND | — | Panel ground | Conditional: only once the panel's supply rating is documented and the carrier's ground is identified against a documented ground test point or module ground pin (protocol Stage 3). |
| 2 | VCC | 3V3 | — | Panel supply | Conditional: only with a documented supply rating, a documented rail capacity, and the PRE-POWER RELEASE GATE released by a named person. |
| 3 | SCL | SPI2 SCLK | **GPIO12** | SPI2 IOMUX | Provisional. |
| 4 | SDA | SPI2 MOSI | **GPIO11** | SPI2 IOMUX | Provisional. |
| 5 | RES | general output | **GPIO9** | Plain GPIO; the SPI2 QUADHD pad is unused in 4-wire SPI | Provisional. |
| 6 | DC | general output | **GPIO8** | Plain GPIO | Provisional. |
| 7 | CS | SPI2 CS0 | **GPIO10** | SPI2 IOMUX | Provisional. |
| 8 | BLK | backlight control | **GPIO7** | Module exposes IO7 as a general-purpose IO | Provisional and conditional: only if the panel's documented interface proves a compatible logic input. Otherwise the contact stays unconnected and no GPIO sources current into it. |

- The panel is a **4-wire SPI** part, so the quad-SPI pads are not used. GPIO9 is
  therefore available as a plain output for `RES`; it must not be routed as quad
  data.
- The **contact numbers in this table are the user-supplied pinout image's
  numbering, not validated positions**. Stage 1 of the bring-up protocol reads
  the order from the unit itself, and every connection uses those validated
  positions instead of these numbers.
- **GPIO13 stays unassigned**, held for a future MISO if a read path is ever
  needed.
- No TF card, no SDMMC and no other SPI device is assigned here. A future
  LCD + TF arrangement has to arbitrate the shared bus explicitly; that
  implementation does not exist yet.
- The signal choices are IOMUX-aligned with SPI2 (`CS0=10`, `SCLK=12`,
  `MISO=13`, `MOSI=11`). The ESP32-S3 GPIO matrix can route other signals, so
  the alignment is a preference, not a hard constraint.
- GPIO7 is published as `LCD_DRIVER_BACKLIGHT_GPIO_CANDIDATE` in
  `firmware/gateway/components/lcd_driver/include/lcd_driver.h`. It is a
  proposal, not a binding assignment, and the firmware assigns no GPIO.

### Reserved pins (must not appear in a display candidate)

| Pins | Reason |
|---|---|
| GPIO35, GPIO36, GPIO37 | Octal PSRAM on the confirmed N16R8 module. |
| GPIO19, GPIO20 | Native USB D− / D+. |
| GPIO43, GPIO44 | UART0 (TXD0 / RXD0) on the module. |
| GPIO0, GPIO3, GPIO45, GPIO46 | Strapping pins; GPIO0 is also BOOT. |
| GPIO48 | Onboard WS2812, functionally confirmed on this unit. |

### Evidence record (copy of record)

This is the record of record. The bring-up protocol keeps the working copy with
the same row IDs; **each measurement is taken once** and its value is recorded
here. Every row needs the instrument or document, the value, the unit, the date
and the verdict. **All physical results are pending.**

| ID | Requirement / measurement | Instrument or document | Value | Unit | Date | Status |
|---|---|---|---|---|---|---|
| A3 | Validated physical contact order and positions | — | | — | | pending |
| G1 | Panel supply, current, permitted input range, absolute max, input current limits, unpowered/back-power restrictions, reset polarity, power-up/power-down sequence | | | — | | pending |
| G2 | Panel `BLK` interface and input current limit | | | — | | pending |
| G3 | Carrier schematic: header → GPIO, rail per header, documented ground point | | | — | | pending |
| G4 | Carrier 3V3 rail capacity and current limit | | | — | | pending |
| B1 | Carrier ground header position, against a documented ground point | | | — | | pending |
| B2 | Panel ground contact position, against a documented point | | | — | | pending |
| C2 | Continuity header → documented GPIO point, per signal | | | — | | pending |
| C3 | Reserved pins confirmed absent from the candidate positions | | | — | | pending |
| D1 | Carrier 3V3 rail voltage | | | V | | pending |
| D3 | 3V3 rail under light load | | | V | | pending |
| P2 | Named human release of the PRE-POWER RELEASE GATE | — | | — | | pending — not granted |
| E1 | Supply source and current limit, with its derivation | | | — | | pending |
| E2 | Supply connections made, with validated positions | — | | — | | pending |
| E4 | Display-side supply voltage | | | V | | pending |
| E5 | Display current draw | | | mA | | pending |
| E6 | Backlight observation with `BLK` unconnected | — | | — | | pending |
| F0 | Common signal-reference ground established, rails discharged | | | — | | pending |
| F1 | Input-level compatibility verdict and safe sequence (or the block) | | | — | | pending |

### Release state — two authorizations, both pending

| Authorization | Scope | Requires | Status |
|---|---|---|---|
| **A — power-only electrical characterization** | Connect the panel's validated ground and supply contacts to one documented source; measure the panel's supply voltage, current and backlight behaviour | The PRE-POWER RELEASE GATE: Stages 1–5 complete with only A3, G1, G2, G3, G4, B1, B2, C2, C3, D1 and D3 recorded (optional D3 may record justified non-performance), followed by P2's named human release | **not granted — pending** |
| **B — signal and functional bringup** | Connect the panel's signal contacts, drive the controller, observe an image | Authorization A completed (E1, E2, E4, E5, E6), plus a documented safe power-up/power-down sequence and F1's documented full voltage/current-envelope and logic-level compatibility verdict; unknown or incompatible limits block B | **not granted — pending** |

A is released on documentation and carrier-side measurements, and it produces
the panel-side measurements that B needs. **Nothing in A requires the panel to
have been powered before A is granted**, and nothing in B starts until A is
complete. P2 is the release record, not a prerequisite to itself. E1/E2/E4/E5/E6
are recorded under A, not before it; F1's documentary assessment precedes B,
while F0's physical common-ground verification follows B with all sources off.
Later rows remain pending until their authorized stages. Documentation alone
never reaches a supply connection: the named release is what does.

| Release field | Value |
|---|---|
| Released by (name) | *pending* |
| Date | *pending* |
| Stages 1–5 complete | *pending* |
| Authorization A granted | *pending — not granted* |
| Authorization B granted | *pending — not granted* |

**The first display power-up stays blocked.** No panel supply contact is
connected, no signal contact is connected and no GPIO is driven until the
release above happens. A blank or noisy panel at power-up is not evidence of
anything and no result may be inferred from it.

The gate is non-circular by construction: the panel's current, permitted input
range and back-power restrictions come from its documentation, so the gate never
asks for a measurement of the powered panel in order to permit the first power.

### Sources for the candidate mapping

- [ESP32-S3-WROOM-1/WROOM-1U datasheet (official)](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)
  — module pin diagram: IO7…IO12 are exposed; IO22…IO34 are not.
- [ESP-IDF v5.4 SPI master documentation](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/spi_master.html)
  — SPI2 IOMUX: `CS0=10`, `SCLK=12`, `MISO=13`, `MOSI=11`, `QUADWP=14`,
  `QUADHD=9`, plus the GPIO-matrix alternatives.

Neither source documents the carrier in hand. The mapping proves only that the
module can carry it; the carrier wiring remains unknown.

## Migration implications

- The failed C6 candidate and its integrated LCD/TF pin map do not carry forward to this S3 candidate.
- GPIO35–GPIO37 must remain unassigned because the N16R8 module uses them for Octal PSRAM.
- The user-supplied seller schematic and a low-brightness on-target smoke check confirm the onboard WS2812 is driven by GPIO48 on this physical unit; GPIO19/GPIO20 remain candidate native USB D−/D+ signals and should stay reserved while native USB is needed.
- ODD-4's board-only checks passed: the Unity app reported `3 Tests 0 Failures 0 Ignored`; the user visually confirmed red → green → blue → off at 1,000 ms per state. This does not independently establish the carrier's make or revision.
- Preserve the autonomous browser dashboard and OTA as gateway requirements. ODD-3 configures the 16 MB custom partition layout and budgets; real full-gateway runtime measurements remain required to establish feasibility.
- Compare the drawing-reported dimensions and physical labels with the delivered 8-pin module before enclosure design; confirm board-side GPIO mapping, supply and logic levels, and backlight current/control before wiring it.

### ODD-4 display status

The external ST7789 remains disconnected and untested. Do not assign its RES/DC/CS/SCL/SDA/BLK pins or connect its VCC/backlight until the exact module is matched and its logic-level, backlight-current, and control requirements are confirmed. The candidate mapping in this profile is the target of that verification, not an approval to wire.

## Sources

- [Espressif ESP32-S3-WROOM-1/WROOM-1U datasheet (official)](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)
- [ESP-IDF v5.4 SPI master API reference](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/api-reference/peripherals/spi_master.html) — SPI2 IOMUX assignments used by the candidate mapping
- [Candidate seller listing](https://es.aliexpress.com/item/1005006418608267.html) — incomplete fetched specifications
- [YD-ESP32-S3 / ESP32-S3-WROOM-1 Dev reference](https://github.com/profharris/YD-ESP32-S3_ESP32-S3-WROOM-1_Dev) — third-party and marked work in progress
- [User-provided display listing](https://es.aliexpress.com/item/1005008546903854.html) — incomplete fetched specifications; its associated PDF appears to describe a different 30-pin variant
