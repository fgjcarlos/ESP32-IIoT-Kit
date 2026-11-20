# Tasks: issue-69-local-portal

**Change**: issue-69-local-portal
**Date**: 2026-11-19
**Artifact store**: openspec
**Total tasks**: 9
**Delivery strategy**: ask-on-risk (`feature-branch-chain`)

---

## Wave 1 — Foundations (sequential)

### T-01.01 — First-provisioning decision (gate)
Agree the first-provisioning mechanism with the owner (options A/B/C in
`design.md` D3); record the ADR in `knowledge/Decisiones/` and the Log of
`odd/tasks/issue-69-portal-local-protegido.md`. No auth code before this lands.
**Test**: — (decision artifact)

### T-01.02 — `wifi_manager` status getters
Read-only getters: AP (up, SSID, IP, channel), STA (state, SSID, IP), MAC
format reuse. No behavior change to existing API.
**Test**: `cd firmware/gateway/test && idf.py test` (+ host if shims allow)

## Wave 2 — Auth and assets (sequential; T-02.02 parallel-safe with T-02.01)

### T-02.01 — `portal_auth`: credential + session (per approved ADR)
NVS namespace `"portal"` (X-macro keys, SHA-256 digest, constant-time compare),
first-boot provisioning per ADR, session token mint/invalidate, credential
change rules. Register namespace in `nvs_config.h` table.
**Test**: host (`tools/host-tests`) + device suite

### T-02.02 — Asset pipeline: stable dist + embedding
`web/vite.config.js` (stable filenames, no hashing), un-ignore `web/dist`,
commit built assets; `EMBED_TXTFILES` in `http_server` with a path→MIME
descriptor table; embedded-asset integrity test.
**Test**: host asset-integrity test + `idf.py build`

## Wave 3 — Server and SPA (T-03.01 and T-03.02 are independent units)

### T-03.01 — `http_server` real implementation
`esp_http_server`: asset routes, `POST /api/login`, `GET /api/status`,
credential change endpoint; 401 + error body on unauthorized; 1 s failure
delay; app_main wiring (server starts with AP ready).
**Test**: device/QEMU suite (`tools/ci/run-device-tests.sh`) + `idf.py build`

### T-03.02 — Web W01→W02 adaptation
API client replacing `demo-device.js`; login/status/credential-change against
the real endpoints; error rendering; `ConfigView` deferred to W03 (nav without
config); `node --test` suite green; README updated to product-portal status.
**Test**: `cd firmware/gateway/web && npm run test`

## Wave 4 — Verification and delivery

### T-04.01 — Full verification battery
`idf.py build` clean; host tests; QEMU device tests incl. main-task headroom
gate; no external-resource references in served assets (grep + integrity
test); no secrets committed (grep for credential literals).
**Test**: all of the above, plus independent `gentle-ai-verify` (high-risk item: security/auth)

### T-04.02 — Documentation, issue close, PR chain
Canonical docs updated once (hardware profile untouched; portal documented at
its source of truth); conventional commits, chained PRs ≤400 lines on
`feature/issue-69-*`; comment/close issue #69.
**Test**: CI green on PR heads; `mkdocs build --strict` if docs touched
