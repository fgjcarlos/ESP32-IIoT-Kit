# Issue 67: nonclosing review chain
Target: `fgjcarlos/ESP32-IIoT-Kit`, verified default `main` at `5335cce`; strategy `feature-branch-chain`.
Tracker: draft/no-merge `feat/issue-67-tracker`; verification uses existing build/test evidence plus fresh slice/label/host readback.

## Specs
- S1: User: "no no hace falta, si esta la issue resuelte abre PR". No orientation change.
- S2: User selected "Sí, abrir sin cerrar #67" and "Sí, añadir status:approved a #67". All PRs use `Refs #67`; no issue closure, merge or unrequested electrical-proof claims.
- S3: User previously selected "Cadena sobre rama de funcionalidad (recomendada)". Preserve a draft/no-merge tracker and five child review slices, each at most 400 authored lines.
- S4: Preserve the unrelated untracked PNG, original branches and authenticated actor `fgjcarlos`; use explicit HTTPS/GitHub credentials, never ambient SSH.
- S5: Child boundaries: protocol `de5bea3` (394), map `65d2a91` (292), pattern `ee0e7bc` (370), app `abc24c2` (332), specification/regressions/physical observations through `1db4b4c` (382).

## Tasks
- [x] P1 (S1–S5), parent: actor ADMIN/MAINTAIN and target/default verified; issue label mutation read back preserving `enhancement` and OPEN; fresh `origin/main` fetched; tracker worktree registered.
- [x] P2 (S3–S5), parent + bounded body writer: branches and bodies prepared; slice diffs and focused checks passed; body writer mvu0xyxd-6-cpxy and parent structural readback accepted.
- [x] P3 (S1–S5), parent: tracker #104 draft and children #105–#109 created and read back with exact head/base/body/labels and slice sizes; all use Refs #67.
- [x] P4 (S1–S5), parent: job-10 verified all final links, head/base/body/type-label and CI snapshots; checks observed green or still running, no failures. No merge or approval claims.

## Log
- L1: "no no hace falta, si esta la issue resuelte abre PR".
- L2: Conditional request resolved explicitly by questionnaire choices in S2. Minimal image diagnostic complete by user observation; issue 67 electrical/identity documentation remains incomplete, so publication is nonclosing.
- L3: Issue 67 approval granted and confirmed: `enhancement,status:approved`, state OPEN. Target `main` freshly fetched to `5335ccefded24c1a09ae91710da6287b73f0c4cb`; local original main/feature refs remain untouched.
- L4: Five measured slices are 394/292/370/332/382 lines. `shellcheck tools/lcd-smoke/run-host.sh` and original feature diff whitespace checks passed. New source changes are not needed for publication.
- L5: Prior evidence: gateway/device builds; QEMU112/112 (hardware tags excluded); diagnostic host6/6, full diagnostic build, independent verifier PASS, authorized flash/hashes and user-visible bands+border. Initial console capture remained ROM DOWNLOAD (failed check), later physical observation established execution. Native review unavailable (managed assets/untracked declaration), no native approval claimed.
- L6: Atomic HTTPS push confirmed all six new branches; no PR exists yet. Tracker7def660; slice heads a49eafd/08e0b0a/97dac0f/4c2e9b8/85a5bb5. Final worktree host6/6,ShellCheck,MkDocs strict and whitespace checks passed; content equals previously tested firmware/tools. Body writer mvu0xyxd-6-cpxy still awaited; do not mark P2/P3 complete.
- L7: Bodies received and reviewed; native assessment of publication metadata passive/reviewDue=false. Parent will replace ordinary PR-number placeholders with actual creation results and verify each mutation before the next. No automatic retries, merges or issue closure.
- L8: Creation job-8 exit0: tracker #104 draft plus children #105/#106/#107/#108/#109, each exact head/base/body/title/type-label and size verified before the next mutation. Final dependency-number links and CI snapshot are being checked by job-9; no merge.
- L9: `gh pr edit 104` failed with deprecated Projects Classic projectCards lookup. No PR recreation. Corrected route reads each current body first, patches via REST only if unchanged from our own initial publication, or accepts an already matching final body; unexpected drift stops. Job-10 in progress; final success not yet claimed.
- L10: Job-10 exit0: #104–#109 dependency links and exact metadata verified, tracker remains draft, issue67OPEN. Observed checks successful or running (firmware/reproducibility pending on some children); no green-CI or merge-ready claim. Final publication record stays in the last review slice.
