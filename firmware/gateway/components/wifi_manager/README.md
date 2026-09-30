# wifi_manager (Phase 0, T0.6.1-T0.6.2)

AP+STA lifecycle for the ESP32-S3 IIoT gateway. Exposes a small
deterministic surface used by `app_main` and by the on-target Unity
runner.

## Lifecycle

1. `wifi_manager_init(&credentials)` brings up netif, the default event
   loop, the Wi-Fi driver in `WIFI_MODE_APSTA`, configures AP+STA,
   starts the driver, registers event handlers, and (when the local
   credentials header carries an SSID) kicks `esp_wifi_connect()`.
2. The gateway stays running. Event handlers log connect/disconnect
   events and IP changes.
3. `wifi_manager_deinit()` stops Wi-Fi, deletes the STA reconnect timer,
   and deinits the driver.

`wifi_manager_init` is **not** idempotent. Call it once during boot
and `wifi_manager_deinit` once during teardown.

## Credential handling (do not regress)

- The canonical credential source is `/.env` at the repo root. It is
  gitignored. The user edits it by hand.
- `tools/gen-wifi-credentials.sh` reads `/.env` and emits
  `components/wifi_manager/include/wifi_credentials.local.h`. That
  header is gitignored. The helper runs on demand and also before each
  build when invoked from the project root.
- `wifi_manager.c` includes the header via `#if __has_include(...)`
  so the build with an empty `.env` (CI mode) compiles cleanly: the
  macros stay undefined, STA stays disabled, and the AP comes up
  open with Kconfig defaults.
- **Never** commit a secret. Never paste a secret into a commit
  message, an issue body, a PR body, a chat log, or a public log.
  The helper script's `--dry-run` mode prints `(set)` or `(empty)`
  for each key, never the literal value.

## Boot order (do not regress)

`wifi_manager_init` calls the following sequence. Each step is
required by ESP-IDF; calling them out of order is a runtime hazard.

1. `nvs_flash_init` (handled by `nvs_config_init` in app_main before
   this function is called).
2. `esp_netif_init`.
3. `esp_event_loop_create_default`.
4. `esp_netif_create_default_wifi_ap` and `esp_netif_create_default_wifi_sta`.
5. `esp_wifi_init(&WIFI_INIT_CONFIG_DEFAULT())`.
6. `esp_wifi_set_mode(WIFI_MODE_APSTA)`.
7. `esp_wifi_set_config(WIFI_IF_AP, ...)` and (when STA SSID present)
   `esp_wifi_set_config(WIFI_IF_STA, ...)`.
8. `esp_wifi_start()`.
9. Register event handlers for the six Wi-Fi events plus the two IP
   events listed in the header file.
10. (When STA SSID is present) `esp_wifi_connect()`.

## T0.6.3 reminder

When the C3 sensor nodes join the network in T0.6.3, the AP and STA
must share the same 2.4 GHz channel as the ESP-NOW peers. ESP-NOW
and Wi-Fi coexistence is out of scope for this issue.