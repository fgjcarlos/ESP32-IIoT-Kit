# archive-report — issue #10 ST7789 + LVGL local screen

## Outcome

**COMPLETED.** Issue #10 ships as contracts-only via 6 chained PRs
(#31, #32, #33, #34, #35, #36). All merged to `main`. GitHub issue #10
closed as `completed` with a documentation comment
(id 5899398252).

## Final state

- **`origin/main` HEAD**: `8c8c806` (merge of PR #36).
- **PR list**:
  - #31 `feat(gateway): add lcd_driver component contract (interface only)` — MERGED at `f4b25db`.
  - #32 `feat(gateway): add lvgl_port component contract (interface only)` — MERGED at `0f9d21b`.
  - #33 `feat(gateway): add ui_screens component contract (interface only)` — MERGED at `fe1608e`.
  - #34 `feat(gateway): add assets_brand component contract (interface only)` — MERGED at `7baab99`.
  - #35 `feat(gateway): wire display chain init chain into app_main (contracts only)` — MERGED at `351962f`.
  - #36 `test(gateway): register assets_brand in TEST_COMPONENTS + cumulative evidence (ODD-6)` — MERGED at `8c8c806`.
- **GitHub issue #10**: CLOSED (reason: completed).

## Delivery shape

- **Workflow**: ODD (Organic Driven Development).
- **Chain strategy**: feature-branch-chain. No long-lived feature branch.
- **Delivery strategy**: ask-on-risk. No destructive operations required.
- **Review budget**: 400 lines/PR. Largest PR was ODD-3 at 151 lines; well under budget.
- **Slice count**: 6.
- **Total lines added**: +551/-1 across 7 files modified (cumulative).

## Verification artifacts

- **Build**: success on `idf.py build` (main + test).
- **Boot smoke**: 7 log lines in order on `/dev/ttyACM0`.
- **Kconfig**: 0 component-level `Kconfig` files.
- **Tags**: 11 in test ELF, one per component.
- **Symbols**: 16 in test ELF (production has more).

## Decisions captured

- **Resolution**: 170×320 (issue body typo `172×320` was wrong).
- **Backlight GPIO**: GPIO22 candidate, pending physical verification,
  no binding.
- **SPI bus shared constraint**: documented in headers, no arbitration code.
- **Contracts-only**: no `esp_lcd_*`, no `lv_init`, no draw buffer, no tick timer.

## Open follow-ups (NOT in this change)

- Real `esp_lcd_*` bring-up (gated on physical carrier verification).
- Real LVGL wiring (`lv_init`, display driver, draw buffers, tick timer).
- Real screen implementations (each `ui_*_screen` stub logs and returns).
- Real brand assets (logo, palette, font).
- TF card driver (separate issue, owns the other side of the SPI bus).
- WiFi manager (issue #21, deferred).

## Bugs caught and fixed during delivery

- **ODD-4 omitted defensive append**: `list(FIND ...)` for
  `assets_brand` in `firmware/gateway/test/CMakeLists.txt` was missing
  from PR #34. Production firmware unaffected. Fixed in PR #36 (5 lines).

## Artifacts preserved in this archive

| File | Purpose |
|---|---|
| `state.yaml` | Final state, all PRs, evidence |
| `explore.md` | Surface mapping, risk classification |
| `proposal.md` | Outcome, scope, slice plan, non-goals |
| `design.md` | Component dependency graph, per-component design, test rationale |
| `tasks.md` | Per-task summary with PRs and commits |
| `apply-progress.md` | Slice-by-slice progress with build evidence |
| `verify-report.md` | Final verification, build, tags, symbols, Kconfig, boot smoke |
| `archive-report.md` | This file |
| `odd-task-issue-10-st7789-lvgl.md` | Snapshot of the ODD feature document at archive time |
| `specs/` | Empty: the contracts are the headers themselves |

## References

- GitHub issue: https://github.com/fgjcarlos/ESP32-IIoT-Kit/issues/10
- ODD feature document: `odd/tasks/issue-10-st7789-lvgl.md`
- Issue #9 archive: `openspec/changes/archive/2026-09-29-issue-9-gateway-firmware-foundation/`
- `CLAUDE.md` — gateway memory facts and physical carrier constraints.