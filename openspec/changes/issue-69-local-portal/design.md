# Design: issue-69-local-portal

## D1 — Assets embedded in the app image (not SPIFFS)

The W01-v0.2 SPA build output (`web/dist`) is embedded into the firmware app
image via CMake `EMBED_TXTFILES` in the `http_server` component, not mounted
from the SPIFFS partition.

Rationale:
- The issue asks for "HTML/CSS/JS servidos desde flash"; the app image is flash.
- No filesystem dependency, no partition table change, no mount step — the
  existing `spiffs` partition stays untouched (its sizing question, listed in
  `openspec/config.yaml` open_constraints as pending, is not resolved here and
  remains open for any future SPIFFS consumer).
- Precedent: `brand_profile` already embeds binary data into the component
  (`target_add_binary_data`, `brand_profile/CMakeLists.txt:64-66`).
- Hermetic firmware builds: `dist/` is committed with stable filenames (vite
  config disables content hashing), so firmware CI needs no Node toolchain.

Budget: the W01 bundle is ~8.5 KB gzipped (~35–60 KB raw). App partitions are
3 MB (`ota_0`/`ota_1`, `partitions.csv`); embedding adds well under 100 KB of
rodata. Headroom asserted by the embedded-asset integrity test and the
reproducible-build CI gate.

## D2 — Auth model: single owner credential + RAM session

- One owner credential. Stored in NVS namespace `"portal"` (registered in the
  `nvs_config.h` namespace table) as its SHA-256 digest (mbedtls), never in
  plain text, never in Git. X-macro key list per the `sparkplug_b_nvs.h` pattern.
- Comparison is constant-time on the digest.
- Login (`POST /api/login`) with the correct credential mints a random session
  token; the token lives in RAM only (single active session — a new login
  invalidates the previous token; one owner, acceptable and simplest).
- The token is set as an HttpOnly cookie; protected endpoints and data
  (`GET /api/status`, credential change) reject requests without a valid token
  with HTTP 401 and an error body the SPA renders.
- Failed logins get a fixed 1 s delay (cheap brute-force hardening).
- Credential change requires the current credential; a rejected change leaves
  the stored digest and NVS counters unchanged.

## D3 — First provisioning mechanism: DECIDED (ADR-006)

Owner approved option **A** on 2026-11-19 (recorded in
`knowledge/Decisiones/006-portal-provision-inicial.md`): a random credential
generated at first boot (`esp_fill_random`), stored only as its SHA-256 digest
in NVS, revealed to the owner via the serial console (UART0/USB-CDC); first
login prompts a change (the W01 flow already models this). No secrets in Git;
recovery requires physical access.

Options considered and rejected:

- **B** — deterministic credential derived from device identity (eFuse MAC) and
  a manufacturing salt (fits the brand-secrets tooling); printable on a label.
  Weaker: anyone with the derivation rule and the device identity can compute it.
- **C** — fixed initial credential per brand profile (local secrets file,
  never committed). Simplest for classroom/lab deployments, weakest isolation
  between devices.

## D4 — Status API

`GET /api/status` (protected) returns a small JSON document from new
`wifi_manager` getters: AP (up, SSID from the brand profile, IP, channel, MAC),
STA (state, SSID, IP when connected), and uptime. Read-only; no configuration
endpoints in W02. Errors are reported as HTTP status + JSON error field that
the SPA renders (S3 "comunica los errores").

## D5 — Web adaptation (W01-v0.2 → W02)

The approved wireframes stay as-is. The SPA replaces `demo-device.js` state
transitions with API calls (`/api/login`, `/api/status`, credential change);
`ConfigView` is not offered in W02 (roadmap: `docs/replanificacion/04-roadmap-historias.md:9`
— Wi-Fi configuration is W03). No new views, so no new wireframes are required (S6).

## Sequence — access and status over the AP

```
Browser                 Gateway (esp_http_server over AP)
   |  GET /                      |
   |--------------------------->| serve index.html (flash rodata)
   |  GET app.js / app.css      |
   |--------------------------->| serve assets (flash rodata, MIME-typed)
   |  POST /api/login {cred}    |
   |--------------------------->| verify digest (NVS) -> 401 + delay on failure
   |  200 Set-Cookie: session   |
   |<---------------------------|
   |  GET /api/status (cookie)  |
   |--------------------------->| wifi_manager getters -> JSON
   |  200 {ap, sta, uptime}     |
   |<---------------------------|  (no cookie -> 401, SPA shows error)
   |  POST /api/credential      |  (change requires current credential)
```

## Memory budget

- Assets: flash rodata (embedded), 0 heap at request time (served from a
  descriptor table with length + MIME).
- Session state: one token + expiry scalar in static RAM (~40 B).
- `http_server` runs on the `esp_http_server` default task (stack configured in
  code, kept within the 4 KB default-task constraint noted in AGENTS.md; the
  server gets its own task, sized per `httpd_config_t`, asserted by the
  main-task headroom gate in the device test runner).
- `wifi_manager` getters: no allocation; fill caller-provided structs.

## Test mapping

- Host (`tools/host-tests`): digest/NVS logic (with shims), constant-time
  compare, credential-change rules, session token lifecycle, asset descriptor
  table integrity, status JSON serialization.
- Device/QEMU (`firmware/gateway/test`): route registration, 401 on
  unauthorized `/api/status`, login success/failure, getters return AP-up state.
- Web (`node --test`): API client mapping, view rendering for
  locked/unauthorized/error states.
- `[hw]` tagged, excluded from CI: full browser-over-AP walkthrough (S3) —
  documented as pending physical verification until the board is available.
