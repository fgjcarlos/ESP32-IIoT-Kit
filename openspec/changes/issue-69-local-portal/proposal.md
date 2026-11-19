# Proposal: issue-69-local-portal

## Intent

The gateway serves a protected local web portal from its own flash over the AP
interface. The owner connects to the AP, unlocks the portal with the provisioned
credential, and sees real gateway status — without STA or Internet.

**What**: Replace the `http_server` stub with a real `esp_http_server` that
serves the W01-v0.2 Preact SPA (adapted to a real API) embedded in the firmware
image, plus a minimal auth layer (single owner credential in NVS, session
cookie) and a status API backed by new `wifi_manager` status getters.

**Why now**: Issue #69 (W02 — Base) is the next roadmap story; its dependency
W01 (#68, wireframes) is closed with the W01-v0.2 demo validated, and all reused
foundations (#9 base, #21 AP+STA, #11 Sparkplug B, #12 brand, #13 OTA, #14 CI)
have landed. This story does not reopen #16 (LCD/ESP-NOW runtime and physical
validation remain deferred).

**Success**: A browser connected to the gateway AP (no STA/Internet) renders the
approved access flow and a status page fed by real gateway state; protected
operations and data reject unauthorized access; API, interface and embedded
asset tests are green host-side and under QEMU.

## Scope

### In Scope
- `wifi_manager`: read-only status getters (AP SSID/IP/channel, STA state/SSID/IP)
- `portal_auth` (new component or sublayer of `http_server`): single owner
  credential — SHA-256 in NVS namespace `"portal"`, constant-time compare,
  session token in RAM, first-provisioning mechanism per agreed ADR
- `http_server`: real implementation over `esp_http_server`; asset routes,
  `/api/login`, `/api/status`, credential change endpoint; 401 on unauthorized
- Web W01-v0.2 adaptation: real API client replacing demo data; login flow;
  status view live; credential view live; Wi-Fi config view deferred to W03
- Asset pipeline: stable output filenames, `dist/` committed, `EMBED_TXTFILES`
  embedding, embedded-asset integrity test
- `app_main` wiring: server starts with the AP ready
- Unity tests (host + device/QEMU) for auth, routes, API and embedded assets;
  `[hw]`-tagged browser-over-AP check documented as pending physical verification
- Documentation at its canonical sources; openspec + ADR for the provisioning decision

### Out of Scope
- Sensor nodes, history/logging, multiple accounts, Bun/Node backend (S5)
- Wi-Fi configuration from the portal (W03)
- WebSocket/live push, LCD/ESP-NOW runtime, physical validation (deferred by #16)
- MQTT/Sparkplug B integration changes

## Capabilities

### New Capabilities
- `local-portal` — see `specs/local-portal/spec.md`
