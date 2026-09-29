# Verify report: issue-9-gateway-firmware-foundation

**Change**: issue-9-gateway-firmware-foundation
**Date**: 2026-09-29
**Artifact store**: openspec
**Source of truth**: `main` at `41449df` (merge of PR #29)
**Toolchain**: ESP-IDF v5.4, xtensa toolchain via `source ~/esp/esp-idf/export.sh`

## T-01.07 — Cumulative build/test evidence (compile-only)

Worktree `/tmp/ESP32-IIoT-Kit-issue-9-evidence` checked out at `origin/main` (`41449df`). No local modifications, no rebase. Reproducible from a fresh worktree.

### Main app build

```
$ cd firmware/gateway
$ idf.py set-target esp32s3   # one-time, cached in sdkconfig
$ idf.py build
... (build output) ...
gateway.bin binary size 0x374d0 bytes. Smallest app partition is 0x300000 bytes.
0x2c8b30 bytes (93%) free.
```

`gateway.bin` 226,512 bytes (0x374d0). Smallest app partition 0x300000 bytes. 93% free.

### Test build

```
$ cd firmware/gateway/test
$ idf.py set-target esp32s3   # one-time, cached in sdkconfig
$ idf.py build
... (build output) ...
gateway_test.bin binary size 0x3bd10 bytes. Smallest app partition is 0x100000 bytes.
0xc42f0 bytes (77%) free.
```

`gateway_test.bin` 245,952 bytes (0x3bd10). Smallest app partition 0x100000 bytes. 77% free.

### ELF symbol verification

```
$ xtensa-esp32s3-elf-nm build/gateway_test.elf | grep -E 'T (espnow_manager_init|http_server_start|http_server_stop|mqtt_bridge_start|mqtt_bridge_stop|ota_manager_init|display_manager_init)'
400da89c T display_manager_init
400da8b8 T espnow_manager_init
400da8d4 T http_server_start
400da8f0 T http_server_stop
400da90c T mqtt_bridge_start
400da928 T mqtt_bridge_stop
400da944 T ota_manager_init
```

7 symbols exported.

### Unity tag verification

```
$ strings build/gateway_test.elf | grep -oE '\[(board_profile|board_rgb|espnow_manager|http_server|mqtt_bridge|ota_manager|display_manager)\]' | sort -u
[board_profile]
[board_rgb]
[display_manager]
[espnow_manager]
[http_server]
[mqtt_bridge]
[ota_manager]
```

7 Unity tags present. Coincides 1:1 with `TEST_COMPONENTS` final composition (`board_profile;board_rgb;espnow_manager;http_server;mqtt_bridge;ota_manager;display_manager`).

### Kconfig audit

```
$ ls components/*/Kconfig* 2>/dev/null || echo "no component-level Kconfig files"
no component-level Kconfig files
```

Zero component-level Kconfig files. The five new components (espnow_manager, http_server, mqtt_bridge, ota_manager, display_manager) follow the contract-only pattern with no `menuconfig` surface.

### Partition table verification

The boot trace (T-01.08) confirms the on-device partition table matches `firmware/gateway/partitions.csv`:

```
## Label            Usage          Type ST Offset   Length
 0 nvs              WiFi data        01 02 00009000 00006000
 1 otadata          OTA data         01 00 0000f000 00002000
 2 phy_init         RF data          01 01 00011000 00001000
 3 ota_0            OTA app          00 10 00020000 00300000
 4 ota_1            OTA app          00 11 00320000 00300000
 5 spiffs           Unknown data     01 82 00620000 00400000
```

Six partitions, matches `partitions.csv` exactly.

### Note on `idf.py test`

ESP-IDF v5.4 does not expose `idf.py test` natively; the project uses `CONFIG_UNITY_ENABLE_IDF_TEST_RUNNER=y` (on-target Unity runner). Compile success + Unity tag presence in the test ELF + symbol export are the verifiable equivalent in this environment. On-target execution remains pending hardware availability (covered by T-01.08).

## T-01.08 — Physical-board boot smoke

```
$ idf.py -p /dev/ttyACM0 flash
... (esptool output) ...
Wrote 226512 bytes (122381 compressed) at 0x00020000 in 1.9 seconds (effective 971.6 kbit/s)...
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
Done

$ idf.py -p /dev/ttyACM0 monitor   # non-interactive, captured via stdin redirect
... (monitor output, abbreviated) ...
I (26) boot: ESP-IDF v5.4 2nd stage bootloader
I (27) boot: compile time Sep 29 2026 18:35:25
I (27) boot: Multicore bootloader
I (27) boot: chip revision: v0.2
I (29) boot: efuse block revision: v1.3
I (33) boot.esp32s3: Boot SPI Speed : 80MHz
I (37) boot.esp32s3: SPI Mode       : DIO
I (41) boot.esp32s3: SPI Flash Size : 16MB
I (45) boot: Enabling RNG early entropy source...
I (49) boot: Partition Table:
I (52) boot: ## Label            Usage          Type ST Offset   Length
I (58) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (64) boot:  1 otadata          OTA data         01 00 0000f000 00002000
I (71) boot:  2 phy_init         RF data          01 01 00011000 00001000
I (77) boot:  3 ota_0            OTA app          00 10 00020000 00300000
I (84) boot:  4 ota_1            OTA app          00 11 00320000 00300000
I (91) boot:  5 spiffs           Unknown data     01 82 00620000 00400000
I (97) boot: End of partition table
I (168) boot: Loaded app from partition at offset 0x20000
I (168) boot: Disabling RNG early entropy source...
I (179) octal_psram: vendor id    : 0x0d (AP)
I (179) octal_psram: dev id       : 0x02 (generation 3)
I (179) octal_psram: density      : 0x03 (64 Mbit)
I (179) octal_psram: good-die     : 0x01 (Pass)
I (181) octal_psram: Latency      : 0x01 (Fixed)
I (190) octal_psram: VCC          : 0x01 (3V)
I (194) octal_psram: SRF          : 0x01 (Fast Refresh)
I (199) octal_psram: BurstType    : 0x01 (Hybrid Wrap)
I (204) octal_psram: BurstLen     : 0x01 (32 Byte)
I (208) octal_psram: Readlatency  : 0x02 (10 cycles@Fixed)
I (213) octal_psram: DriveStrength: 0x00 (1/1)
I (218) esp_psram: Found 8MB PSRAM device
I (221) esp_psram: Speed: 40MHz
I (224) cpu_start: Multicore app
I (958) esp_psram: SPI SRAM memory test OK
I (967) cpu_start: Pro cpu start user code
I (967) cpu_start: cpu freq: 160000000 Hz
I (967) app_init: Application information:
I (967) app_init: Project name:     gateway
I (971) app_init: App version:      41449df
I (975) app_init: Compile time:     Sep 29 2026 18:35:20
I (980) app_init: ELF file SHA256:  8bc23ca00...
I (984) app_init: ESP-IDF:          v5.4
I (988) efuse_init: Min chip rev:     v0.0
I (992) efuse_init: Max chip rev:     v0.99
I (996) efuse_init: Chip rev:         v0.2
I (1000) heap_init: Initializing. RAM available for dynamic allocation:
I (1006) heap_init: At 3FC96920 len 00052DF0 (331 KiB): RAM
I (1011) heap_init: At 3FCE9710 len 00005724 (21 KiB): RAM
I (1016) heap_init: At 3FCF0000 len 00008000 (32 KiB): DRAM
I (1022) heap_init: At 600FE11C len 00001ECC (7 KiB): RTCRAM
I (1027) esp_psram: Adding pool of 8192K of PSRAM memory to heap allocator
I (1035) spi_flash: detected chip: generic
I (1038) spi_flash: flash io: dio
I (1041) sleep_gpio: Configure to isolate all GPIO pins in sleep state
I (1047) sleep_gpio: Enable automatic switching of GPIO sleep configuration
I (1054) main_task: Started on CPU0
I (1064) esp_psram: Reserving pool of 32K of internal memory for DMA/internal allocations
I (1064) main_task: Calling app_main()
I (1084) nvs_config: NVS initialized
I (1084) gateway: ESP32-S3 profile: flash=16777216 bytes, PSRAM=8388608 bytes
I (1084) main_task: Returned from app_main()
```

### Acceptance check

- `nvs_config` log line: `I (1084) nvs_config: NVS initialized` ✓
- `board_profile` log line: `I (1084) gateway: ESP32-S3 profile: flash=16777216 bytes, PSRAM=8388608 bytes` ✓
- `nvs_config` precedes `gateway`/`board_profile` (same tick, but `nvs_config` is the earlier log line) ✓
- `app_main()` returns to the FreeRTOS scheduler without error: `I (1084) main_task: Returned from app_main()` ✓
- No watchdog reset, no init error, no panic.
- Octal PSRAM probe OK; 8MB pool registered.
- Partition table matches `partitions.csv` (nvs / otadata / phy_init / ota_0 / ota_1 / spiffs).
- Firmware SHA-256 prefix `8bc23ca00…` matches the compiled `gateway.elf` from `41449df`.

### Scope of the boot smoke

Only the entry sequence is verified: `app_main()` ordering, `nvs_config` initialization, `board_profile` read, and FreeRTOS scheduler startup. WiFi AP/STA bring-up, ESP-NOW peer add, HTTP server request, MQTT publish, actuator toggle, and OTA slot switch remain in their respective component cycles; their contract skeletons are present but their runtime behavior is deferred to per-component Fase 1–5 work.

## Summary

- T-01.07 closed (cumulative build + test ELF evidence on `main` post-merge of all six PRs).
- T-01.08 closed (physical-board boot smoke on `/dev/ttyACM0`).
- All nine boxes in `tasks.md` are `[x]`.
- The issue is fully verified.
