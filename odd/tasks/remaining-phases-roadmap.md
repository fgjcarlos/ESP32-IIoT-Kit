# Remaining phases: small, complete stories
Objective: plan the remaining product work and publish non-duplicate GitHub stories, without implementing firmware or interfaces.
Branch: `codex/remaining-phases-roadmap`; delivery: ask-on-risk, forecast 300 authored lines; checks: document/link/form/dependency readback (no runtime changes).

## Specs

- S1: "Revisa el repo, lo que llevamos hecho y me gustaria crear un plan para las demas fases restantes del proyecto, las issues correspondientes. Ten en cuenta la nueva placa y la nueva pantalla, con la que tendremos que diseñar los datos que vamos a mostrar, creando primero los wireframes para darlos por validos."
- S2: "y la parte de la pagina web alojada? no aparece, para la configuracion del mismo?"
- S3: "ok crea un plan para llevar a cabo todo y crea las issues necesaria, eso si me gustaria que fuesen historias pequeñas, funcionalidades completas."
- S4: "Si" — response to authorization to query existing issues and create necessary issues in `fgjcarlos/ESP32-IIoT-Kit` using this machine's authenticated GitHub CLI session.
- Authorized scope: documentation and issue creation only; no firmware/UI implementation, push, PR, issue reopening, or protected-label changes. Wireframes and hardware measurements are future deliverables, not approvals granted by this plan.
- Acceptance: a dependency-ordered roadmap, small vertical implementation stories with observable acceptance criteria, explicit hardware/wireframe gates, current open-and-closed duplicate classification, and confirmed target-host readback for every created issue.

## Tasks

- [x] T1 (S1–S3), inline: canonical roadmap written; mapping trigger handled by read-only explorer; no writer trigger. Structural checks passed; commit: `4d1de93bfb7b7b5cde0e92c1895525e269a164a1`.
- [x] T2 (S1–S4), inline: 35 stories published and read back using the technical-proposal form; issue links reconciled; all checks passed. Commit: this work unit; identity recorded in the Engram mirror after commit.

## Log

- L1 (2026-10-28): "ok crea un plan para llevar a cabo todo y crea las issues necesaria, eso si me gustaria que fuesen historias pequeñas, funcionalidades completas."
- L2 (2026-10-28): "Si" authorizes the specific GitHub query/create operation and CLI session described in S4.
- L3: Read-only explorer mapped authoritative phases, actual runtime gaps and closed-issue boundaries. Parent verified hardware and issue-16 archive. Closed #8–#16 and #21 are historical delivery evidence, not proof of full product runtime.
- L4: GitHub discovery confirmed repository identity, main default branch, enabled issues, current YAML form and enhancement label. Complete open-and-closed lookup: 10 issues, all closed. Full issue bodies were inspected in private temporary storage; no raw body copies belong in this log.
- L5: Reuse #11 Sparkplug B, #12 branding, #13 gateway OTA and #14 CI. New stories cover distinct deferred/runtime/integration outcomes; do not reopen or automatically comment on the completed proposals.
- L6: No meaningful runnable RED for passive planning documents; structural, dependency, privacy and exact GitHub readback checks replace runtime tests. Firmware builds and hardware checks are not applicable to this change.
- L7: Created docs/replanificacion/04-roadmap-historias.md: 27 v1 stories and 8 optional Phase 6 stories, with vertical outcomes and explicit validation exceptions. Added a locator to the original corrected phase plan; no source-of-truth hardware/protocol facts duplicated.
- L8: T1 committed as 4d1de93bfb7b7b5cde0e92c1895525e269a164a1. Native assessment: passive/non_executable_only, review_due=false, reason=passive; boundary advanced to that commit.
- L9: Created and independently read back GitHub issues #67–#101: 27 v1 stories (#67–#93), 8 optional Phase 6 stories (#94–#101). Each is OPEN with enhancement; no protected labels, comments, reopenings or changes to existing issues. All 35 mutations classified confirmed; no retries or uncertain outcomes.
- L10: Checked exact title/body roundtrip and stable target-host identity per creation. Checked 35 unique roadmap entries, issue links, dependency graph without cycles and local file references. mkdocs build --strict passed (dependency deprecation/advisory output, no build failure); firmware/Unity/hardware tests are N/A for this documentation-only change.
- L11: Roadmap publication does not approve wireframes, select unknown electrical values, authorize firmware implementation, spend money or publish a release. Next: H01 (#67) physical evidence and W01 (#68) first-access/configuration wireframes. No push or PR requested.
