# Vendored Sparkplug B protobuf definition

| Field | Value |
|-------|-------|
| Source repository | https://github.com/eclipse/tahu |
| Source path | `sparkplug_b/sparkplug_b.proto` |
| Source branch | `3.x` (Sparkplug B v3) |
| Source commit (vendored on) | see `odd/tasks/issue-11-sparkplug-b/design.md` §3.2 |
| License | Eclipse Public License 2.0 (EPL-2.0) |
| SPDX-License-Identifier | `EPL-2.0` |

The full text of the Eclipse Public License v2.0 is reproduced in the
header comments at the top of `sparkplug_b.proto`. The upstream copyright
notice (Cirrus Link Solutions, 2015/2018) is preserved verbatim.

## Why vendored

The IIoT gateway needs a deterministic, offline-compilable Protobuf schema.
Depending on a network-side Eclipse Tahu release at every build would couple
our CI to GitHub availability. Vendoring lets us:

- pin a known-good spec version per gateway feature branch;
- regenerate nanopb stubs deterministically without fetching upstream at
  configure time;
- audit schema changes against the spec PDF before accepting them.

## How to update

1. Pull the new commit from `https://github.com/eclipse/tahu/blob/3.x/sparkplug_b/sparkplug_b.proto`.
2. Diff against the local copy. Sparkplug B is generally wire-stable within
   a major version; only accept changes that match the canonical spec PDF
   `https://sparkplug.eclipse.org/specification/version/3.0/documents/sparkplug-specification-3.0.0.pdf`.
3. Update the "Source commit" row in this file.
4. Re-run `idf.py build` and exercise the conformance harness in
   `tools/sparkplug-conformance/` (commit 11).

Do NOT switch to upstream `master` blindly — it tracks Sparkplug A
additions and unstable Java tooling paths.