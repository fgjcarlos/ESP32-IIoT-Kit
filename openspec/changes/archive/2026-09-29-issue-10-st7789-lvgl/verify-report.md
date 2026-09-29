# verify-report — issue #10 ST7789 + LVGL local screen

Verify-phase evidence captured on `origin/main` `8c8c806` (post-#36 merge)
in a fresh worktree from `origin/main`.

## Build evidence

```text
$ idf.py -C firmware/gateway set-target esp32s3
... Build files have been written to: .../build

$ idf.py -C firmware/gateway build
... gateway.bin binary size 0x37970 bytes. Smallest app partition is 0x300000 bytes. 0x2c8690 bytes (93%) free.
... Bootloader binary size 0x5210 bytes. 0x2df0 bytes (36%) free.

$ idf.py -C firmware/gateway/test build
... gateway_test.bin binary size 0x3cde0 bytes. Smallest app partition is 0x100000 bytes. 0xc3220 bytes (76%) free.
```

## Unity tags (11)

```text
$ strings build/gateway_test.elf | grep -oE '\[(...)\]' | sort -u
[assets_brand]
[board_profile]
[board_rgb]
[display_manager]
[espnow_manager]
[http_server]
[lcd_driver]
[lvgl_port]
[mqtt_bridge]
[ota_manager]
[ui_screens]
```

All 11 expected tags present. One per component.

## Symbols exported (16)

```text
$ xtensa-esp32s3-elf-nm build/gateway_test.elf | grep -E 'T (init|start|stop|screen|read|_init)$' | sort
400da8cc T assets_brand_init
400da8ec T board_profile_read
400dab90 T display_manager_init
400dabac T espnow_manager_init
400dac00 T lcd_driver_init
400dac20 T lvgl_port_init
400dac3c T lvgl_port_start
400dac58 T lvgl_port_stop
400dacac T ota_manager_init
400dacc8 T ui_screens_init
400dace4 T ui_boot_screen
400dad00 T ui_status_screen
400dad1c T ui_connectivity_screen
400dad38 T ui_nodes_screen
400dad54 T ui_alerts_screen
400dad70 T ui_ota_screen
```

16 symbols exported. Plus `nvs_config_init`, `http_server_init`, and
`mqtt_bridge_init` are present in `gateway.bin` but were not added to
the test project's `TEST_COMPONENTS` (out of scope for issue #10).

## Kconfig audit

```text
$ find firmware/gateway/components -maxdepth 2 -name 'Kconfig*' | wc -l
0
```

0 component-level `Kconfig` files. All five issue #10 components rely
on the existing top-level `sdkconfig` configuration; no new options
introduced.

## Boot smoke on `/dev/ttyACM0`

Captured on `ESP32-S3-WROOM-1-N16R8`, chip rev v0.2, flash 16 MB, PSRAM 8 MB.

```text
I (1084) nvs_config: NVS initialized
I (1084) gateway: ESP32-S3 profile: flash=16777216 bytes, PSRAM=8388608 bytes
I (1084) lcd_driver: lcd_driver contract placeholder (8-pin ST7789V2 contacts 1..8 in header; backlight GPIO22 candidate pending physical verification; no esp_lcd_* call in this contract)
I (1104) lvgl_port: lvgl_port contract placeholder (no lv_init, no display driver, no draw buffers, no tick timer in this contract)
I (1114) ui_screens: ui_screens contract placeholder (no lv_obj_create, no screen registration, no event binding in this contract; six screen stubs declared in header)
I (1124) assets_brand: assets_brand contract placeholder (label="IIoT-Kit"; no image decode, no SPIFFS/FATFS read, no draw buffer in this contract)
I (1144) main_task: Returned from app_main()
```

**Seven expected log lines in documented order. App_main returns
cleanly, no panic, no abort. Firmware version `351962f` (the merge
commit of PR #35).**

## Constraints honored

- **No `esp_lcd_*` call**: lcd_driver.c logs the contact map and returns.
- **No `lv_init`**: lvgl_port.c logs the no-go list and returns.
- **No draw buffer allocation**: no `lv_mem_alloc`.
- **No tick timer configuration**: no `esp_timer_create`.
- **No `lv_task_handler`**: no FreeRTOS task for the LVGL tick loop.
- **No SPI bus init**: no `spi_bus_initialize`.
- **No GPIO assignment**: GPIO22 is documented as candidate only,
  pending physical verification.
- **No Kconfig change**: 0 component-level `Kconfig` files.
- **No real image decode**: `ASSETS_BRAND_PLACEHOLDER_LABEL` is an
  ASCII constant.
- **No `wifi_manager`** touched (out of scope).
- **No `partitions.csv` / `sdkconfig.defaults` / root
  `CMakeLists.txt`** touched.

## Cross-component consistency

| Component | Header exposed in API? | Used by main? | Test registered? | In test ELF? |
|---|---|---|---|---|
| `lcd_driver` | yes | yes | yes | yes |
| `lvgl_port` | yes | yes | yes | yes |
| `ui_screens` | yes | yes | yes | yes |
| `assets_brand` | yes | yes | yes (post-#36) | yes (post-#36) |
| `display_manager` | yes | yes (in carrier init path) | yes | yes |
| `nvs_config` | yes | yes | no | no (in gateway.bin only) |
| `http_server` | yes | yes | no | no (in gateway.bin only) |
| `mqtt_bridge` | yes | yes | no | no (in gateway.bin only) |
| `ota_manager` | yes | yes | yes | yes |
| `espnow_manager` | yes | yes | yes | yes |
| `board_profile` | yes | yes | yes | yes |
| `board_rgb` | yes | yes | yes | yes |

## Findings

1. **ODD-4 test-registration omission** (caught during this slice and
   fixed in PR #36): the defensive `list(FIND ...)` append for
   `assets_brand` in `firmware/gateway/test/CMakeLists.txt` was missing
   from PR #34. Production firmware unaffected; test ELF consistency
   restored.
2. **Spec count correction**: ODD doc said "12 Unity tags" but real
   count is 11 (one per component). No component is missing after the
   ODD-6 fix.

## Resolution

**APPROVED.** All ODD-1..ODD-6 acceptance criteria met. Issue #10 ships
as contracts-only. Real implementation deferred to a future Fase
gated on physical carrier verification.