# ST7789V2 display bring-up protocol (ESP32-S3 gateway)

**Status: procedure prepared, not executed. The first display power-up is BLOCKED.**

This document is a procedure, not a record of facts. Every value it produces is
measured by the integrator and then written into the canonical hardware profile,
`docs/hardware/esp32-s3-wroom-1-n16r8.md`, which remains the single source of
truth for the gateway hardware and holds the candidate connection map and the
evidence record. This protocol links to that map instead of repeating it.

**Why this exists:** issue #67 requires a verified connection map before the panel
is connected and powered for the first time. The canonical profile already states
the display contact map, the candidate GPIOs and the carrier reservations. What is
missing is the physical evidence that lets an integrator wire the two boards
without damaging the carrier, the USB console or the PSRAM — and the documentation
that says whether the panel may be powered at all.

## Scope

| In scope | Out of scope |
|---|---|
| Identify the carrier and the panel, and record what is confirmed and what is pending | Implementing the display driver (`esp_lcd_*`), LVGL, or the panel reset sequence |
| Gather the documentation that decides supply, input limits and backlight interface | Assigning a GPIO as a binding wiring decision |
| Verify the carrier header positions that carry the candidate GPIOs | Turning the panel on: that is gated on a named release |
| Record the evidence needed to lift the block | Converting seller-reported figures into measurements |

## Authorization model — two separate releases

Powering the panel and wiring its signals are **different decisions**. They are
released separately, and neither is granted today.

| Authorization | What it permits | What it requires | Status |
|---|---|---|---|
| **A — power-only electrical characterization** | Connect the panel's **validated** ground and supply contacts to one documented source, then measure the panel's supply voltage, current and backlight behaviour | The PRE-POWER RELEASE GATE below: Stages 1–5 complete, only the pre-power record IDs listed in P1.7 filled in, and a **named human release** | **not granted — pending** |
| **B — signal and functional bringup** | Connect the panel's signal contacts, drive the controller, observe an image | Authorization A completed, **plus** a documented safe power-up/power-down sequence **plus** a documented input-level compatibility verdict | **not granted — pending** |

A is not a step inside B's evidence. A is released on documentation and
carrier-side measurements, and it produces the panel-side measurements that B
needs. **Nothing in A requires the panel to have been powered before A is
granted**, and nothing in B starts until A is complete. Documentation alone does
not reach the first supply connection: the named release is what does.

## Safety rules — read before touching the hardware

1. **Do not power the display yet.** Until the PRE-POWER RELEASE GATE is
   released, the panel's supply rating, input limits and backlight interface are
   unknown. An unidentified, rating-unknown panel is not connected to any rail.
2. **Do not apply 5 V to the display supply contact** unless the panel's own
   documentation states that its supply is 5 V. The candidate scheme feeds the
   panel from the carrier's **3V3** rail, and that is a candidate, not an
   authorisation.
3. **Never drive `BLK` from a GPIO and never source current into it.** Leave the
   contact unconnected until its interface is documented. A lamp that lights, or
   a voltage measured on a floating contact, does **not** establish that `BLK`
   is a logic input.
4. **Never assume the USB shield is bonded to the circuit ground.** Ground is
   identified against a documented ground test point or module ground pin, with
   local unpowered continuity on that board.
5. **Never tie two positive rails together, never backfeed the carrier's 3V3
   from another source, and never run two supplies in parallel.** One source
   feeds the panel.
6. **Never place a meter across two boards** whose grounds are not verified as
   common. Measure each board against its own ground.
7. Unplug the carrier's USB before rewiring. Never rewire a powered board.
8. When the meter is in current mode, start on the highest current range, and
   never leave the meter in current mode across a supply — in that mode the
   meter is nearly a short circuit.
9. Do not press BOOT (GPIO0) and do not reset the carrier while the panel is
   connected, except where a stage explicitly asks for it.

## What the current bench is missing

The bench has the carrier, the panel and a multimeter. That is enough for
Stage 1, Stage 3 and Stage 5. It is **not** enough to power the panel: a
multimeter reads a voltage, and a voltage reading does not prove the panel's
permitted input range, its input current, its transient tolerance or its reset
polarity. Stage 2 requires documents, not instruments, and Stage 4 needs the
carrier schematic that G3 asks for. A current-limited supply is useful for
Stage 6, but its limit is derived from the panel's documented rating — an
arbitrary limit is not a safety measure.

## Stage 1 — Identify the two units (no power)

| Step | Action | What to record |
| --- | --- | --- |
| A1 | Photograph both faces of the carrier: silkscreen, revision markings, both USB-C ports, buttons, and every header label. | Photo set; any legible marking that identifies make/revision. |
| A2 | Photograph the display: front, back, the silkscreen next to the eight contacts, and the connector. | Photo set; the contact labels as read from the unit itself. |
| A3 | Read the contact order from the display silkscreen. Do not assume it equals the user-supplied pinout image. | **The validated physical position of every contact that will be connected**, including the ground contact and the supply contact. |
| A4 | Record the carrier's header positions and their printed labels. The candidate image labels GPIO0–GPIO21 and GPIO35–GPIO48, but the unit in hand decides. | Label list per header row. |

**Pass:** photographs archived and the contact order read from the unit itself. A
contact order taken from a listing is not evidence. From here on, every step
refers to the **validated contact positions** recorded in A3 — never to a
contact number taken from a listing.

## Stage 2 — Documentation gate (no electrical action)

Nothing is connected to the panel until every row below is filled with a
document, not a recollection. A missing document is a **stop**, not an
assumption.

| Gate | Document needed | Why it decides something |
| --- | --- | --- |
| G1 | The panel's own datasheet or module documentation: supply voltage range; supply current; **permitted input-voltage range for normal operation**; **absolute maximum ratings** (a limit, never a target); **input current limits**; **restrictions on driving inputs while unpowered and on back-powering the panel**; **documented power-up and power-down sequence**, if one exists; **reset polarity** | Decides the supply rail, whether the MCU's output levels are compatible with the panel's inputs, whether the panel may be powered before or together with the MCU, and whether the signal stage is reachable at all. |
| G2 | The panel's backlight documentation: what `BLK` is (logic enable, PWM input, LED anode/cathode, or not connected) and its input current limit if it is a logic input | Decides whether `BLK` may ever be driven by a GPIO. Until this is known, `BLK` stays unconnected. |
| G3 | The carrier's schematic: which header position carries which GPIO, which rail feeds which header, and **a documented ground test point or module ground pin** | Decides the header-to-GPIO binding, the supply path, and the only legitimate reference for the ground identification in Stage 3. |
| G4 | The carrier's rail capacity and the recommended current limit for the 3V3 header | Decides whether the panel can be fed from the carrier rail at all, and what the current limit must be. |

**Pass:** G1–G4 each point to a document, a page or a revision. If the panel's
documentation cannot be found, the panel stays unpowered and the first
power-up stays blocked; that outcome is a valid result of this protocol.

## Stage 3 — Unpowered local identification (one board at a time)

Both boards are unpowered and are handled separately. No measurement crosses
from one board to the other.

| Step | Action | What to record |
| --- | --- | --- |
| B1 | With the carrier unpowered and **isolated**, take the ground reference from the **documented ground test point or module ground pin** (G3). **Do not assume the USB shield is bonded to the circuit ground.** Measure from each candidate header ground position to that documented point. | The carrier ground header position, plus the document reference used. |
| B2 | With the panel unpowered and isolated, identify the panel's ground contact: measure **each** contact against the contact the panel's own documentation labels as ground. The ground contact is **never measured against itself**. | The panel's validated ground contact position (A3). |
| B3 | Cross-check the positions from B1 and B2 against the labels and the silkscreen; note any position where the reading is ambiguous or where the board shows a resistance instead of a short. | Confirmation per board, with the ambiguous positions listed. |

**Pass:** each board's ground is identified on that board, against a documented
point, with no continuity check between the two and no contact measured against
itself. If a board's ground cannot be identified locally, stop there — a guessed
ground is the fastest way to damage both boards.

## Stage 4 — Carrier header → GPIO verification (display disconnected)

The display stays **completely disconnected** for this stage. The candidate
scheme names GPIOs; the carrier labels headers, and the unit in hand is the
only authority on which header position carries which GPIO. The method is a
verified document plus an accessible test point — never a wire that loops
through the display.

| Step | Action | What to record |
| --- | --- | --- |
| C1 | With the carrier unpowered, use the G3 schematic to find a test point, pad or module pin that is documented as carrying each candidate GPIO. | The document reference and the reachable point for each signal. |
| C2 | With the carrier unpowered, measure continuity between the carrier header position and that documented point. | Continuity confirmed / not confirmed per signal. |
| C3 | Repeat for the reserved pins in the canonical profile, to prove they are *not* on the header positions that would be used. | The reserved positions that are confirmed absent from the candidate set. |
| C4 | Photograph each header with its label legible and the wire attached to the position under test. | Photo per signal. |

**Pass:** every candidate signal is bound to a header position using a documented
point. A candidate whose header position cannot be identified this way stays
unbound and unverified, and no wiring decision may use it. If the carrier
documentation does not exist at all, every position stays unverified and the
published map remains a candidate. A method that requires the display to be in
the loop is not an acceptable method.

## Stage 5 — Carrier rails, display disconnected

| Step | Action | What to record |
| --- | --- | --- |
| D1 | Power the carrier from USB. Measure the voltage at the carrier's 3V3 header position, against the carrier's own ground. | 3V3 rail voltage. |
| D2 | Measure the voltage at the carrier's 5V header position. | 5V rail voltage (informational). |
| D3 | If a 1 kΩ resistor is available, connect it from 3V3 to GND and re-measure the 3V3 rail. | Voltage under a ~3 mA load, showing the rail holds. |

**Pass:** the 3V3 rail sits inside the module's recommended supply range and does
not sag under a light load. The ESP32-S3 module operates from a 3.3 V rail; if
the measured value is outside the datasheet's recommended operating conditions,
stop here. This stage says nothing about the panel's own requirements — those
come from G1 and G4.

## PRE-POWER RELEASE GATE — before any panel supply connection

No supply contact is connected before this gate is released. It has two parts:
the evidence, and the named release.

### P1 — Evidence required (Stages 1–5 complete)

| # | Requirement | Produced by | Record row |
| --- | --- | --- | --- |
| P1.1 | The panel's exact physical contact order, with the validated position of every contact that will be connected | Stage 1 (A3) | A3 |
| P1.2 | The panel's ground contact, identified with the panel unpowered and isolated, against a documented point | Stage 3 (B2) | B2 |
| P1.3 | The carrier's ground, identified with the carrier unpowered and isolated, against a documented ground test point or module ground pin | Stage 3 (B1) | B1 |
| P1.4 | Every candidate signal bound to a carrier header position, verified with the panel disconnected, against a documented point | Stage 4 (C2) | C2 |
| P1.5 | The carrier's 3V3 rail measurement and its documented capacity and current limit | Stage 5 (D1, D3) + gate G4 | D1, D3, G4 |
| P1.6 | The panel's exact electrical documentation: supply range and current, permitted input-voltage range, input current limits, unpowered-input and back-power restrictions, `BLK` interface, reset polarity, and a documented power-up/power-down sequence if one exists | Gates G1, G2 | G1, G2 |
| P1.7 | Only the pre-power evidence subset complete: document or instrument, value, unit, date and verdict for these IDs. An optional D3 not performed must record that disposition and its justification, not a fabricated result. | Stages 1–5 | A3, G1, G2, G3, G4, B1, B2, C2, C3, D1, D3 |

P2 records the subsequent human release; it is not a prerequisite to itself.
Rows E1, E2, E4, E5 and E6 remain pending until Authorization A permits
Stage 6. Rows F0 and F1 remain pending until the separately gated Stage 7;
F1's documentary compatibility verdict is required before Authorization B.
None of these P2/E/F rows is part of P1's pre-power subset.

**P1 never asks for a measurement of the powered panel.** The panel's current,
its input limits and its permitted input range come from its documentation
(G1); the measurements come later, under Authorization A. That keeps the gate
free of a circular requirement.

### P2 — Named human release

The gate is released by a **named person** who has read the evidence record and
states that Stages 1–5 are complete and that Stage 6 may start. A complete
document set does not release the gate by itself: the release is a decision
with a name attached.

| Field | Value |
|---|---|
| Released by (name) | *pending* |
| Date | *pending* |
| Stages 1–5 complete | *pending* |
| Authorization A (power-only characterization) granted | *pending — not granted* |

**Current state: P1 is not satisfied and P2 has not happened. No supply
connection is authorised.**

## Stage 6 — Power-only electrical characterization (Authorization A)

Reachable only with Authorization A released. Documentation alone does not reach
this stage.

| Step | Action | What to record |
| --- | --- | --- |
| E1 | Choose **one** source: the carrier's 3V3 rail, or an isolated regulated supply whose voltage matches G1 and whose current limit is derived from G1's documented current. Never both, never in parallel, never backfeeding 3V3, never tying positive rails together. | The source and its documentary basis. |
| E2 | All sources OFF, carrier USB detached, and rails discharged (verified with the meter): wire the panel's validated ground and supply contacts (A3) to the single chosen source. With carrier 3V3, use its validated ground (B1); with an isolated supply, use that supply's return and keep the carrier completely disconnected. Only ground and supply are wired; no signals. | Validated positions, source, and verified power-off state. |
| E3 | Only after wiring is complete, enable the chosen source according to its documented safe sequence. For carrier 3V3, attach USB now; for an isolated supply, enable that supply while the carrier remains disconnected. Shut down, detach USB and verify discharged rails before changing any connection, including inserting a series current meter. | Source enable/shutdown sequence followed. |
| E4 | Measure the voltage at the panel's own supply contact, against the panel's own ground. | Display-side supply voltage. |
| E5 | Measure the current the panel draws, with the source's limit in force. | Display current draw (mA) and the limit in force, with the limit's derivation. |
| E6 | With the `BLK` contact unconnected, observe the backlight. | Backlight lit / not lit / flickering — an observation only. |

**Pass:** the display-side supply voltage, the current draw and the derived
current limit are recorded, and the backlight observation is recorded as an
observation. `BLK` stays unconnected: this stage does not classify it, because
the classification comes from G2's document, not from a lamp.

## Stage 7 — Signal connections and levels (Authorization B)

Reachable only with Authorization B granted: Authorization A completed, a
documented safe power-up/power-down sequence available, and a documented
input-level compatibility verdict.

| Step | Action | What to record |
| --- | --- | --- |
| F0 | **Before any conductor is attached**: all sources OFF, the carrier's USB detached, every rail discharged (verified with the meter), then establish the **verified common signal-reference ground** between carrier, panel and, if an isolated supply is used, its return. Verify it with the boards unpowered, using each board's documented ground point. Never tie positive rails, never backfeed 3V3, never run two supplies in parallel. | The ground points used, the continuity result, and the discharge verification. |
| F1 | **On paper, before Authorization B and before signal wiring**: require the entire expected/worst-case voltage and current envelope at every receiving input to remain within its documented normal operating limits throughout startup, operation and shutdown, including either board unpowered and any reverse-driven case. Range overlap is insufficient. Also require guaranteed output levels to satisfy the receiver's documented VIH/VIL thresholds. Absolute maxima are never operating targets. Unknown or incompatible limits block Authorization B until a documented compatible interface (level shifting/isolation where needed) is established. **Without a documented safe power-up/power-down sequence, nothing is connected.** | Per-signal full-envelope and logic-level comparison, interface, verdict, and safe sequence or block. |
| F2 | Connect the signal contacts one at a time, to the positions validated in Stage 4, following the documented safe sequence. | Which position each contact was attached to, and the sequence followed. |
| F3 | Record the level at each signal contact, against the common ground established in F0. | One voltage per signal. |

**Pass:** every signal level is recorded and consistent with the panel's
documented permitted input range. **A multimeter reading does not prove the
permitted input range, `VIH`/`VIL`, signal current, transient tolerance or
reset polarity** — those come from G1. Reset polarity comes from G1's
documentation; if it is undocumented it stays unverified. Driving a reset pin
from diagnostic firmware is out of scope for issue #67 and belongs to the
display implementation work.

## Stage 8 — Functional image bringup

**Stage 8 is functional bringup, not the first powered test:** Stage 6 already
powered the panel and Stage 7 already connected its signals. Stage 8 is where
the controller is initialised and an image is expected.

| Step | Action | What to record |
| --- | --- | --- |
| G-1 | Apply the documented safe power-up sequence to the carrier and the panel. | The sequence followed and the outcome. |
| G-2 | Run the ST7789 reset and initialisation sequence and observe the panel. | A stable image, or the observed symptom. |
| G-3 | Record everything that differs from the expected behaviour. | The differences. |

This stage belongs to the display implementation stories (issue #86 / #87) and
is not reached by this protocol alone. A blank or noisy panel is **not** a pass
and **not** evidence of anything; no result may be inferred from it.

## Stop conditions

- Any missing document in Stage 2: stop, leave the panel unconnected and
  unpowered, and record which document is missing.
- The PRE-POWER RELEASE GATE not released by a named person: no supply
  connection, no matter how complete the paperwork looks.
- Any measured value outside the module's recommended operating conditions:
  stop, leave the panel disconnected from every GPIO, and record the value and
  the instrument used.
- If the carrier's 3V3 rail cannot supply the panel's documented current
  without sagging, do not power the panel from the carrier rail; use a separate
  regulated supply and record that decision.
- If the backlight interface stays undocumented, the panel may still be used
  without backlight control only if the documentation says the backlight is on
  by default; otherwise it stays disconnected.
- If no safe power-up/power-down sequence is documented, the signal stage stays
  blocked even after Authorization A is granted.

## Evidence record

Fill this table and copy the result into the canonical profile's evidence
record, which uses the same row IDs and is the copy of record. Every row needs
the instrument or document, the value, the unit, the date and the verdict; a row
without them is not evidence. **Each measurement is taken once** — the canonical
profile holds the record, this table is the working copy. **All rows are
currently pending.**

| ID | Requirement / measurement | Instrument or document | Value | Unit | Date | Status |
| --- | --- | --- | --- | --- | --- | --- |
| A3 | Validated physical contact order and positions | — | | — | | pending |
| G1 | Panel supply, current, permitted input range, absolute max, input current limits, unpowered/back-power restrictions, reset polarity, power-up/power-down sequence | | | — | | pending |
| G2 | Panel `BLK` interface and input current limit | | | — | | pending |
| G3 | Carrier schematic: header → GPIO, rail per header, documented ground point | | | — | | pending |
| G4 | Carrier 3V3 rail capacity and current limit | | | — | | pending |
| B1 | Carrier ground header position, against a documented ground point | | | — | | pending |
| B2 | Panel ground contact position, against a documented point | | | — | | pending |
| C2 | Continuity header → documented GPIO point, per signal | | | — | | pending |
| C3 | Reserved pins confirmed absent from the candidate positions | | | — | | pending |
| D1 | Carrier 3V3 rail voltage | | | V | | pending |
| D3 | 3V3 rail under light load | | | V | | pending |
| P2 | Named human release of the PRE-POWER RELEASE GATE | — | | — | | pending — not granted |
| E1 | Supply source and current limit, with its derivation | | | — | | pending |
| E2 | Supply connections made, with validated positions | — | | — | | pending |
| E4 | Display-side supply voltage | | | V | | pending |
| E5 | Display current draw | | | mA | | pending |
| E6 | Backlight observation with `BLK` unconnected | — | | — | | pending |
| F0 | Common signal-reference ground established, rails discharged | | | — | | pending |
| F1 | Input-level compatibility verdict and safe sequence (or the block) | | | — | | pending |

## What this protocol deliberately does not do

- It does not authorize a display GPIO assignment or a supply connection. The
  scheme stays provisional. Authorization A needs only P1's explicit pre-power
  subset plus P2's named release; Authorization B additionally needs the later
  measurements and documented signal compatibility and safe sequencing.
- It does not power an unidentified panel, and it does not choose a current
  limit without a documented rating behind it.
- It does not treat `VIH`/`VIL` as safety limits, and it does not use an
  absolute maximum rating as an operating target.
- It does not classify `BLK` from a lamp response or from a floating voltage;
  that classification requires the panel's circuit or input specification.
- It does not assume the USB shield is bonded to the circuit ground, and it
  does not tie positive rails or run supplies in parallel.
- It does not measure, restate or infer a pixel pitch. The seller's withdrawn
  `0.1155 × 0.1155 mm` value is never reused.
- It does not import specifications from other 1.9-inch ST7789 modules, from
  the listing's 30-pin variant, or from any other board's documentation. Only
  the unit in hand and the official Espressif datasheet are authorities here.
- It does not cover the display's mechanical dimensions; those stay
  drawing-reported until compared against the delivered module.

## Related documents

- Canonical hardware profile (source of truth, holds the candidate map and the
  evidence record): `docs/hardware/esp32-s3-wroom-1-n16r8.md`
- Feature plan and evidence log: `odd/tasks/issue-67-connection-map.md`
- Retired candidate: `docs/hardware/esp32-c6-lcd-1.47.md`
