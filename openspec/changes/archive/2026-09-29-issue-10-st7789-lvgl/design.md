# design — issue #10 ST7789 + LVGL local screen

## Component dependency graph

```
display_manager  (issue #9, PR #29)
    |
    +-- lcd_driver  (ODD-1, PR #31)
         |
         +-- lvgl_port  (ODD-2, PR #32)
              |
              +-- ui_screens  (ODD-3, PR #33)

assets_brand  (ODD-4, PR #34) — standalone, no display chain dependency
```

`main.c` (ODD-5, PR #35) calls all four new inits plus `display_manager_init`
(which is owned by the carrier init path, not added in this slice).

## Per-component design

### `lcd_driver`

```
include/lcd_driver.h  — public API + Doxygen + hardware hold + contact map
lcd_driver.c          — log-only init stub
test/test_lcd_driver.c — [lcd_driver] ESP_OK init + GPIO22 candidate lock
```

Header contract:

```c
esp_err_t lcd_driver_init(void);
#define LCD_DRIVER_BACKLIGHT_GPIO_CANDIDATE 22  // [pending physical verification]
```

Hardware hold bullets inherited verbatim from `display_manager.h`.

### `lvgl_port`

```
include/lvgl_port.h   — public API + Doxygen + SPI bus shared constraint
lvgl_port.c           — log-only init/start/stop stubs
test/test_lvgl_port.c — [lvgl_port] ESP_OK init/start/stop cycle
```

Header contract:

```c
esp_err_t lvgl_port_init(void);
esp_err_t lvgl_port_start(void);
esp_err_t lvgl_port_stop(void);
```

The header does NOT include `display_manager.h` or `lcd_driver.h`;
callers include them explicitly when they need the constants. This keeps
the dependency graph clean and prevents header transitive includes from
leaking display constants into unrelated components.

### `ui_screens`

```
include/ui_screens.h  — public API + Doxygen + SPI bus constraint echo
ui_screens.c          — log-only orchestrator + 6 screen stubs
test/test_ui_screens.c — [ui_screens] non-NULL pointers + call sequence
```

Header contract:

```c
esp_err_t ui_screens_init(void);
void ui_boot_screen(void);
void ui_status_screen(void);
void ui_connectivity_screen(void);
void ui_nodes_screen(void);
void ui_alerts_screen(void);
void ui_ota_screen(void);
```

The non-NULL pointer test uses `TEST_ASSERT_NOT_NULL((void *)&ui_<name>_screen)`
to force the linker to retain the symbols; without this, GCC could
dead-code-eliminate the empty stubs.

### `assets_brand`

```
include/assets_brand.h  — public API + label constant + scope-of-contract note
assets_brand.c          — log-only init stub
test/test_assets_brand.c — [assets_brand] label match + length lock + ESP_OK
```

Header contract:

```c
esp_err_t assets_brand_init(void);
#define ASSETS_BRAND_PLACEHOLDER_LABEL "IIoT-Kit"
```

The label test asserts both literal `"IIoT-Kit"` AND exact length 8.
Length assertion locks the constant against accidental drift
(whitespace trailing, unicode, version suffix would all break it).

### `main.c`

Init chain in dependency order after `board_profile_read`:

```c
lcd_driver_init();    // ODD-1
lvgl_port_init();     // ODD-2
ui_screens_init();    // ODD-3
assets_brand_init();  // ODD-4
```

Each call is wrapped in:

```c
const esp_err_t err = <init>();
if (err != ESP_OK) {
    ESP_LOGE(TAG, "<name> init failed: %s", esp_err_to_name(err));
    return;
}
```

Conservative: a failed init stops the boot chain rather than continuing
with a partially initialized display subsystem.

### `main/CMakeLists.txt`

`REQUIRES` extended with the 4 new components. The existing pattern is
to list only what `main.c` calls directly, plus the transitive deps that
the linker needs to resolve symbols. We list all four here even though
`lcd_driver.h` and `lvgl_port.h` are included only for their prototypes
— it makes the dependency explicit and prevents linker errors when
future code in `main.c` references these symbols.

### `test/CMakeLists.txt`

Defensive `list(FIND ...)` append for each new component, following the
pattern from ODD-1:

```cmake
list(FIND TEST_COMPONENTS "<name>" <name>_test_component_index)
if(<name>_test_component_index EQUAL -1)
    list(APPEND TEST_COMPONENTS "<name>")
endif()
```

ODD-4 omitted this append. ODD-6 restores it.

## Test design rationale

The project uses `idf.py build` (not `idf.py test`) for verification.
The verifiable evidence per slice is:

1. **Build success**: `idf.py build` (main + test) returns 0.
2. **Unity tag presence**: `strings <elf> | grep <tag>` finds the
   source-local `TAG` constant declared in each component's `.c` file.
   The tag is the only observable side-effect of an empty-stub init
   (the function returns `ESP_OK` after one log line).
3. **Symbol export**: `xtensa-esp32s3-elf-nm <elf> | grep <symbol>`
   confirms the linker retained the public API entry point.
4. **Kconfig audit**: `ls components/*/Kconfig*` returns 0.
5. **Boot smoke**: flash `gateway.bin` to `/dev/ttyACM0`, capture monitor
   output, assert the 7 log lines in order.

This is the test equivalent for an on-target Unity runner when the
runner is not available (no `idf.py monitor` in the environment).
The contract-only stubs are exactly what makes this approach possible:
the empty-stub init is observable through the log tag it produces.

## Memory footprint

| Slice | gateway.bin | gateway_test.bin |
|---|---|---|
| ODD-1 | 0x3a400 (92% free) | 0x3c300 (76% free) |
| ODD-2 | 0x3a400 (92% free) | 0x3c6c0 (76% free) |
| ODD-3 | 0x3a400 (92% free) | 0x3ca60 (76% free) |
| ODD-4 | 0x3a400 (92% free) | 0x3cdc0 (76% free) |
| ODD-5 | 0x37970 (93% free) | 0x3ca80 (76% free) |
| ODD-6 | 0x37970 (93% free) | 0x3cde0 (76% free) |

ODD-5 is smaller because the fresh build dir was created with
`idf.py set-target esp32s3`, regenerating `sdkconfig` from
`sdkconfig.defaults`. The regenerated config produces a slightly leaner
binary. Not a regression.

ODD-6 test ELF grows by 32 bytes vs ODD-5 (one component registration).
Production ELF unchanged.

## Operational notes

- `idf.py set-target esp32s3` is required in fresh build dirs because
  the `sdkconfig` is not pre-populated.
- `sdkconfig.defaults` is preserved across the regeneration; the four
  critical options (`CONFIG_ESPTOOLPY_FLASHSIZE_16MB`,
  `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"`,
  `CONFIG_SPIRAM_MODE_OCT`, `CONFIG_UNITY_ENABLE_IDF_TEST_RUNNER`) all
  remain set.
- `xtensa-esp32s3-elf-nm` requires sourcing
  `~/esp/esp-idf/export.sh` first.
- Boot smoke on `/dev/ttyACM0` requires RTS toggle to force a clean
  boot; just reading serial after the fact will catch the post-boot
  silence, not the init lines.

## References

- ODD feature document: `odd/tasks/issue-10-st7789-lvgl.md`
- Issue #9 archive: `openspec/changes/archive/2026-09-29-issue-9-gateway-firmware-foundation/`
- `CLAUDE.md` — gateway memory facts and physical carrier constraints.