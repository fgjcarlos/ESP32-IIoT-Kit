# ESP32-C6-LCD-1.47 board profile

> **Superseded failed candidate.** The ESP32-C6-LCD-1.47 was reported non-functional and is no longer the selected gateway candidate. ODD-1 now tracks the ESP32-S3-WROOM-1-N16R8 module/carrier candidate with an external 1.9-inch SPI ST7789 panel in [the current S3 profile](esp32-s3-wroom-1-n16r8.md). This document is retained only as historical detail for the failed C6 candidate.

The former gateway board was the Spotpear/Waveshare ESP32-C6-LCD-1.47. This page records source-confirmed hardware facts for the initial ESP-IDF profile; it does **not** claim that the gateway application, display, TF card, dashboard, or OTA path ran on hardware.

## Quick path

1. Configure the gateway project for the `esp32c6` target and ESP-IDF v5.3.1 or later.
2. Centralize the pin mapping below before adding LCD, RGB, or TF drivers.
3. Put the LCD and TF devices on one SPI bus with independent chip-select device configurations.
4. Measure application size, free heap, display-buffer strategy, and physical peripheral behavior before accepting a partition or runtime budget.

## Confirmed profile

| Area | Confirmed board fact | Implementation consequence |
|---|---|---|
| MCU | ESP32-C6FH4 | Use the ESP-IDF `esp32c6` target. |
| Memory | 4 MB flash, 512 KB HP SRAM, 16 KB LP SRAM | The former 8 MB/PSRAM gateway assumptions do not apply. Partition and RAM budgets require measurements. |
| Display | ST7789, 172 × 320 pixels | A full RGB565 frame is 110,080 bytes; whether any full, partial, or double buffer is viable is unmeasured. |
| LCD SPI | MOSI GPIO6, SCLK GPIO7, CS GPIO14, DC GPIO15, RST GPIO21, BL GPIO22 | Configure the display with these board-specific signals. |
| RGB LED | WS2812B-0807 on GPIO8 | Use a timing-compatible WS2812/RMT-style driver after physical validation; color order and runtime behavior are unmeasured. |
| TF slot | MISO GPIO5, MOSI GPIO6, SCLK GPIO7, CS GPIO4 | Configure a separate TF SPI device with its own chip select. |

## SPI bus sharing

The LCD and TF slot share MOSI (GPIO6) and SCLK (GPIO7), but use distinct chip selects: GPIO14 for LCD and GPIO4 for TF. ESP-IDF's SPI master documentation supports multiple devices on a shared bus through separate device handles and chip-select lines.

Design implication: initialize one SPI bus for the shared signals, then add independent LCD and TF devices to it. Coordinate all transfers through a clear bus-ownership boundary (for example, a single service or mutex). ESP-IDF warns that one SPI device must not be accessed concurrently from multiple tasks; the design must also avoid interleaving assumptions between the LCD and TF clients on the shared bus.

## Resource and feasibility caveats

- The 4 MB flash limit is source-confirmed, but no C6 partition table, application image size, dashboard bundle size, or OTA-slot allocation has been measured or selected.
- The 512 KB HP SRAM and 16 KB LP SRAM capacities are source-confirmed, but free heap after Wi-Fi, ESP-NOW, HTTP, display, and TF initialization is unmeasured.
- A standalone browser dashboard and OTA remain project requirements. Their fit on this board is not proven by this profile and must not be traded away without an explicit product decision.
- The cited board material identifies the listed pins and components, but this document does not confirm unlisted wiring, board-revision differences, electrical levels, display-controller settings, or connected-hardware behavior.

## Sources

- [Spotpear/Waveshare board page](https://spotpear.com/index.php/wiki/ESP32-C6-1.47-inch-LCD-Display-Screen-LVGL-SD-WIFI6-ST7789.html)
- [Manufacturer schematic (PDF)](https://files.waveshare.com/wiki/ESP32-C6-LCD-1.47/ESP32-C6-LCD-1.47_schemetics.pdf)
- [ESP-IDF v5.3.1 ESP32-C6 getting started](https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32c6/get-started/index.html)
- [ESP-IDF v5.3.1 SPI master driver](https://docs.espressif.com/projects/esp-idf/en/v5.3.1/esp32c6/api-reference/peripherals/spi_master.html)
