# ESP32-S3-WROOM-1-N16R8 gateway candidate

**Current status: selected candidate; connected SoC and memory probe-confirmed, but carrier revision and display remain unvalidated.** This profile replaces the failed ESP32-C6-LCD-1.47 candidate for gateway planning. It distinguishes the read-only probe observations from the exact module marking, provisional carrier information, and unknown external-display details.

## Quick path

1. Use `esp32s3` when firmware work begins.
2. Reserve GPIO35, GPIO36, and GPIO37: the N16R8 module uses them for Octal PSRAM.
3. Do not wire the display until physical labels, board-side GPIO mapping, supply and logic levels, and backlight requirements are validated.
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
| Free internal heap | 386,295 bytes |
| Free PSRAM | 8,386,148 bytes |
| Largest PSRAM block | 8,257,536 bytes |
| 108,800-byte PSRAM allocation | Passed |

These heap values come from the minimal Unity test app, not the eventual full gateway firmware. The successful allocation validates PSRAM allocation only; it does not validate display DMA or throughput. ESP-IDF documents that ESP32-S3 DMA descriptors cannot reside in PSRAM and that PSRAM DMA bandwidth is limited. After the user released BOOT/GPIO0 and physically reset the board, monitor output from the already-flashed Unity app reported `2 Tests 0 Failures 0 Ignored` and `OK`. The 45-second monitor then ended with timeout exit 124 only after that successful test output.

## Provisional carrier information

The exact purchased carrier, revision, and physical unit have not been confirmed. The following is planning context from a **user-supplied candidate carrier pinout image**, not manufacturer documentation or a verified board pinout. Validate the physical board markings and revision before assigning any peripheral GPIO.

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
| GPIO48 / RGB_LED | The image identifies GPIO48 as connected to the onboard RGB LED | Reserve for the onboard LED; do not propose it for the external LCD without a deliberate future multiplexing decision. |
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

These observations confirm the connected SoC and memory capacities, but do not independently identify the carrier model/revision or verify the display wiring. The probes did not write, erase, or flash the device.

### Candidate carrier mechanical drawing (provisional)

A second user-supplied dimension drawing appears to report a **57.15 × 27.94 mm PCB outline**, **53.34 × 25.40 mm hole-center spacing**, and **2.54 mm (100 mil) header pitch**. These are drawing-reported values, not physical measurements, and the drawing has not yet been matched to the delivered carrier/revision. Do not release an enclosure or mounting design from these values alone.

| Other reported carrier fact | Source and confidence | Required confirmation |
|---|---|---|
| 44-pin dual-USB S3-WROOM-1 board | The [candidate seller listing](https://es.aliexpress.com/item/1005006418608267.html) identifies an S3 WROOM-1 44-pin dual-USB board, but its fetched product page did not expose complete specifications. | Compare the delivered board's markings, revision, and pinout with the listing. |
| Two USB-C ports: CH343P USB-UART and native USB | The [third-party YD-ESP32-S3 / DevKitC-style reference](https://github.com/profharris/YD-ESP32-S3_ESP32-S3-WROOM-1_Dev) reports this arrangement. Its README is marked work in progress. | Verify both ports and their functions on the exact unit. |
| WS2812 RGB LED on GPIO48 | The same third-party work-in-progress reference reports this wiring. | Verify the LED presence, GPIO, color order, and runtime behavior on the exact unit. |

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
- Map the display contacts to safe board-side ESP32-S3 GPIOs; no MCU GPIO assignments are established here.
- Verify the VCC and logic voltage requirements and safe signal levels; 3.3 V operating voltage does not determine logic input levels.
- Verify what the reported 20 mA measures and the backlight current/control requirements; no LED driver or current-limit circuit is established here.
- Confirm the carrier wiring before making any electrical connection.

The [AliExpress display listing](https://es.aliexpress.com/item/1005008546903854.html) did not expose complete specifications. Its associated PDF appears to describe a distinct 30-pin variant; **do not apply that variant's pinout to this user's 8-pin unit**.

No display GPIO assignment or electrical connection is approved until the remaining open items are validated.

## Migration implications

- The failed C6 candidate and its integrated LCD/TF pin map do not carry forward to this S3 candidate.
- GPIO35–GPIO37 must remain unassigned because the N16R8 module uses them for Octal PSRAM.
- The user-supplied candidate carrier image identifies GPIO48 as RGB_LED and GPIO19/GPIO20 as USB D−/D+; reserve them accordingly until the carrier is verified.
- Preserve the autonomous browser dashboard and OTA as gateway requirements. ODD-3 configures the 16 MB custom partition layout and budgets; real build and hardware measurements remain required to establish feasibility.
- Physical smoke checks for boot, carrier RGB behavior, and the exact display belong to ODD-4 after the hardware is available.
- Compare the drawing-reported dimensions and physical labels with the delivered 8-pin module before enclosure design; confirm board-side GPIO mapping, supply and logic levels, and backlight current/control before wiring it.

## Sources

- [Espressif ESP32-S3-WROOM-1/WROOM-1U datasheet (official)](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)
- [Candidate seller listing](https://es.aliexpress.com/item/1005006418608267.html) — incomplete fetched specifications
- [YD-ESP32-S3 / ESP32-S3-WROOM-1 Dev reference](https://github.com/profharris/YD-ESP32-S3_ESP32-S3-WROOM-1_Dev) — third-party and marked work in progress
- [User-provided display listing](https://es.aliexpress.com/item/1005008546903854.html) — incomplete fetched specifications; its associated PDF appears to describe a different 30-pin variant
