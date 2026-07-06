# Agent Reference — Drone Ball Retrieval System

On-demand reference for the tracking core. CLAUDE.md is the always-read
contract; this file holds lookup detail. ADR summaries below are **pointers** —
cite the ADR ID and read `docs/adr/` for the authoritative decision and
rationale; do not treat this file as a second source of truth.

---

## Architectural baseline (§3)

Every load-bearing decision lives in `docs/adr/`. The consolidated readable
snapshot is `docs/design/v0.3-tracking-visualisation-system-design.md`. Before
answering a non-trivial design question, read the relevant ADR — reasoning that
contradicts an ADR without acknowledging it is invalid.

**Locked decisions (do not relitigate unless the user opens the door):**

| ADR | Subject | What it locks |
|-----|---------|---------------|
| ADR-001 | Hybrid C++/Python | C++ core, Python tooling, ZMQ between them. No pybind11 in the runtime path. |
| ADR-002 | ZeroMQ transport | PUB/SUB, core BINDs, consumers CONNECT, `ZMQ_SNDHWM=1`, `ZMQ_CONFLATE=1`, `ZMQ_LINGER=0`. |
| ADR-003 | Graceful coordinate degradation | Three tiers (`Full3D_World`, `Plane2D_World`, `ImagePixels`). v0.3 ships only `Plane2D_World`. |
| ADR-004 | Hybrid calibration lifecycle | Three phases. v0.3 implements Phases 1–2; Phase 3 deferred to ADR-009. |
| ADR-005 | Active laser modulation | 15 Hz, 50/50 duty, 4-frame PSD correlation, IR laser preferred. |
| ADR-006 | FloorPlane2D world frame | Origin at primary Charuco corner, X along long edge, right-handed, Z up. |
| ADR-007 | `safe_for_control` predicate | 8 clauses, single-snapshot evaluation, asymmetric hysteresis on flip-to-true. |
| ADR-008 | LaserController adapter | Separate Python process, owns serial to laser MCU, failsafe-off. |
| ADR-010 | Object-class Z compensation | Per-class height offset in CoordinateMapper. |

**Proposed (not authoritative):** ADR-009 — Active Calibration Refinement (laser
as probe), Phase 5+, gated on observability validation and servo-accuracy
characterisation. Do not reference ADR-009 in code comments, design docs, or new
ADRs. If active calibration becomes relevant, raise it with the user as an open
question, not a planned feature.

**Not locked** (propose a single recommendation when these arise; treat as design
mode): specific C++ libraries (ZMQ bindings, JSON, config, logging); build-system
specifics; the replay-test harness; the MAVLink adapter's implementation; Phase 3+
work.

### Directory layout (§8.7)

Real repo layout (the authoritative one — CLAUDE.md §8.7's aspirational
`services/`/`tools/`/`tests/` tree is not yet realised):

```
docs/adr/            Source of truth for decisions
docs/design/         Readable architectural snapshot
docs/agent-reference/ This file
tracking-core/       C++ core + Python viewer/tools/tests
  src/core/          C++ tracking core
  src/viewer/        Python viewer
  tools/             Python dev tools
  tests/             cpp_unit/, python_integration/
```

Intended future layout adds top-level `services/` (production Python: LaserController,
MAVLink adapter) and `config/` (YAML). Rule globs target the *real* layout.

---

## Hardware & environment (§4 detail)

Home environment, Raspberry Pi 5, children present. Treat these as preconditions.

**Hardware platform**
- **Compute:** Pi 5, 8 GB RAM, 64-bit Raspberry Pi OS. No GPU for NN inference.
- **Co-tenancy:** shared with adjacent subsystems (drone flight-controller stack).
  Assume ~60–70 % sustained CPU budget for the core, not 100 %.
- **Thermal:** throttles under sustained load without active cooling. Passive
  heatsink assumed, active fan permitted but not guaranteed. Surface thermal state
  in `system_health.cpu_temp_c` and degrade when throttling (ADR-002 heartbeat).
- **Camera:** 5 MP NoIR CSI (OV5647 family). Effectively monochrome under typical
  lighting (IR-cut filter absent). Rolling shutter. Manual focus ring — must be
  physically locked at install.
- **CSI cable:** ~15 cm practical reach. Multi-camera CSI on one Pi 5 is
  constrained → Phase 3+ multi-camera assumes a sibling Pi 3B sensor node.
- **Laser:** modulated under MCU control (ADR-005/008). IR preferred. Controllable
  hardware, not an environmental input.
- **Storage:** SD card. No hot-path writes; logging via async ring buffer to
  tmpfs or a dedicated mount.

**Environmental conditions (robustness assumption).** Components may be moved
between or during sessions. Environmental inputs can produce spurious signals
indistinguishable from intended ones at the pixel level. Designs that depend on
"nothing changes between calibration and use" are invalid.
- **Lighting:** variable (sunlight, LEDs, evening dim). Lock auto-exposure at
  calibration (`v4l2-ctl`). Constant-lighting assumptions fail.
- **Reflective surfaces:** glossy floors, glass, screens. Modulation (ADR-005) is
  the primary defence; assume the laser can hit more than one surface.
- **Floor flatness:** approximately planar at room scale (cm) but not flat — rugs,
  thresholds, transitions introduce local 5–10 mm biases. Z=0 does not hold
  globally.
- **Floor surface:** the ball rolls; expect motion when the laser is active.

**Human factors.** Children present: a child may cross the FoV, move furniture /
camera / markers, relocate the laser/ball/drone, or introduce a second laser (the
one-laser rule is a social constraint, not enforced). Visible-spectrum 15 Hz
flicker is perceptible — IR avoids it. No safety certification; do not propose
designs that escalate risk (e.g. higher laser power than specified). Single
operator; no auth, no access control beyond network reachability.

**Network / deployment.** Home LAN; bandwidth is not a constraint at v0.3 rates
(<1 Mbps). Latency to sibling nodes is variable — no sub-ms assumptions. No
internet dependency (no cloud, telemetry, or auto-update). Household mains; allowed
to fail closed (laser off, drone landed) on power loss.

---

## Coordinate frames (ADR-003/006/010 — pointer)

- **Tiers (ADR-003):** `Full3D_World` (Phase 3+), `Plane2D_World` (v0.3),
  `ImagePixels`. Graceful degradation between tiers.
- **FloorPlane2D (ADR-006):** Z=0 is the floor surface; origin anchored to the
  primary calibration Charuco at install, X along the long edge, right-handed, Z
  up. All multi-camera fusion happens in this frame.
- **Z compensation (ADR-010):** per-object-class height offset applied in the
  CoordinateMapper. Required because most tracked objects' centroids are not on
  the floor. See the Z=0 pitfall below.

---

## Calibration lifecycle (§4 + ADR-004)

Calibration drifts — cameras get bumped, focus rings nudged, boards moved.
"Calibrated once at install" is wrong on a months-long horizon.

- **Three phases (ADR-004):** v0.3 implements Phase 1 (install-time intrinsic +
  extrinsic) and Phase 2 (continuous health check using static markers in the
  FoV). Phase 3 (active refinement) is deferred to ADR-009 (Proposed).
- Always route through `calibration_status` in `system_health`; respect
  `INVALID` / `DEGRADED` / `VALID`. Surface drift — do NOT silently self-correct.
- Focus ring must be physically locked at install (tape / set screw / glue); it
  cannot be detected as invalidated without a known-size reference in frame (R-05).
- Per-camera intrinsics JSON is required before a camera deploys; a shared profile
  is permitted only during early development.

---

## Risks & debt (§13)

A **risk** may cause incorrect/unsafe/unexpected behaviour (named trigger, impact,
mitigation). **Debt** is a deliberate compromise with a resolution trigger.
Identifiers are never reused.

**Active risks**
- **R-01 — `safe_for_control` ignores uncertainty composition.** When
  `laser.uncertainty_m + ball.uncertainty_m` approaches `ALIGNMENT_TOLERANCE_M`
  (2 cm), the predicate may emit true only by luck → false positives. Mitigation:
  ADR-007 replay gate requires zero false positives; promote clause 9 into v0.3 if
  replay surfaces uncertainty-correlated false positives.
- **R-02 — Floor non-planarity not validated at install.** Rugs/thresholds/slope →
  5–10 mm bias near features → false negatives (ADR-007 clause 8). Mitigation: an
  install-time flatness probe (board at N positions; assert max reprojected-Z
  deviation) is required before ship; tolerance not yet pinned.
- **R-03 — Modulation/shutter row-time interaction near frame edges.** Rolling
  scan (16.6 ms at 60 fps) vs modulation period (67 ms at 15 Hz) may degrade
  correlation at top/bottom → faster-than-expected `Lost`. Resolution during
  detector implementation.
- **R-04 — Specular reflection produces a modulated ghost detection.** Glossy
  floor/glass/screen → two valid observations for one laser; tracker may attach
  the wrong one during fast motion. Mitigation: documented residual (ADR-005);
  secondary disambiguation (prefer the brighter cluster) at implementation.
- **R-05 — Camera focus drift after install.** A bumped focus ring silently
  invalidates intrinsics; undetectable without a known-size reference. Mitigation:
  physically lock the ring at install (accepted residual if the operator declines).
- **R-06 — Pi 5 thermal throttling under sustained load.** Latency doubles, budget
  violations, frame drops. Mitigation: surface `cpu_temp_c`; degrade (target 30 fps,
  `tracker_state = DEGRADED`) above 80 °C; validate under the performance suite.
- **R-07 — Pi 5 + drone-FC co-tenancy CPU contention.** May invalidate the 60 %
  budget assumption. Mitigation (not yet specified): `cgroup`/`nice` at deployment.
  Blocked until drone integration (Phase 3+); unblocks with D-04.

**Active debt**
- **D-01** — ADR-007 clause 9 (uncertainty margin) deferred to v0.4; promote if
  replay shows uncertainty-correlated false positives.
- **D-02** — ADR-009 active calibration deferred to Phase 5+; gated on servo
  repeatability characterisation AND simulated observability validation.
- **D-03** — Per-camera intrinsic calibration files not yet generated; shared
  profile only during early development.
- **D-04** — MAVLink adapter is contract-only; Phase 3+.
- **D-05** — Replay-test harness does not yet exist; blocking on v0.3 ship
  (ADR-007). Framework choice is open.
- **D-06** — No CI pipeline; acceptable for a single developer. `mypy --strict` in
  CI is a future expectation.

When working in an area that touches a risk/debt item: if resolved, move it to a
historical note with the resolving commit/ADR; if severity/scope changes, edit the
entry in the same change; new risks get the next free `R-NN`/`D-NN`.

---

## Testing & validation (§9 detail)

Ship-gate summary is in CLAUDE.md; the detail follows.

**Categories** (each test belongs to exactly one): **Unit** (single module, no
I/O, no threads, <100 ms; GoogleTest / `pytest`); **Integration** (multiple
modules, controlled I/O, may spawn threads; every commit); **Replay** (recorded
data through the production pipeline vs ground truth / recorded reference — the
ADR-007 gate lives here); **Performance** (latency/throughput/memory/CPU vs
budgets, on the actual Pi 5, blocking on release).

**ADR-007 replay gate (non-negotiable v0.3 ship gate).** ≥3 recorded scenarios,
≥5 minutes total at production frame rate, each with ground truth for laser, ball,
and expected `safe_for_control` per frame. Passes only if `safe_for_control = true`
is emitted solely when ground truth says the laser is within `ball.radius_m +
ALIGNMENT_TOLERANCE_M` of the ball centre AND settled (< `LASER_SETTLED_SPEED_M_PER_S`)
AND all other ADR-007 clauses hold. **Zero false positives** — the cardinal sin.
False negatives are reported but do not fail the test.

**Performance budgets (Pi 5):** end-to-end latency (capture→publish) median ≤30 ms
at 60 fps, p99 ≤50 ms; frame drop ≤0.5 % over 5 min; core CPU ≤60 % of one core at
60 fps; steady-state RSS within 5 % over 30 min; thermal degradation triggers
within 1 s above 80 °C. Budgets violated consistently → revise via an ADR, not by
silently widening the test.

**What to test:** every state-machine transition (trigger and its absence);
boundary conditions (empty/single/gating-distance/out-of-region); each ADR-007
clause failing individually with the right `unsafe_reasons`; schema round-trip
(producer serialise → Python deserialise, no loss). **Don't test:** third-party
internals, purely declarative config values, trivial accessors.

**When tests and reality disagree:** if a test passes but the system is wrong, the
test is wrong — write a failing regression test first, then fix. Never relax
assertions to make a test pass. Intermittent tests are broken — fix the
non-determinism or quarantine with a TODO referencing an issue.

---

## Common pitfalls (§10 detail)

The safety-critical subset is summarised always-read in CLAUDE.md; full detail here.

- **P-01 — Z=0 treated as universal.** Projecting pixels onto Z=0 is correct only
  for on-floor centroids. The ball/drone/foot are not. Apply ADR-010 before
  floor-plane distances. A 6 cm ball at the floor projects ~3 cm off under oblique
  geometry — exceeds the 2 cm tolerance → systematic false negatives.
- **P-02 — Assuming the brightest pixel is the laser.** The v0.2 detector did this
  and was wrong. ADR-005 correlates intensity over time against the modulation
  pattern because sunlight/LEDs/reflections/a second pointer all produce bright
  pixels. A fast path bypassing the correlation window is a correctness regression.
- **P-03 — Conflating per-stage latency and pipeline depth.** The 4-frame window is
  ~67 ms of inherent latency by design, not per-stage slowness. Don't "optimise the
  detector"; the consumer hovers when data is stale. Don't reopen ADR-005 without
  the user's instruction.
- **P-04 — Treating ZMQ silence as a signal.** The core heartbeats ≥1 Hz even when
  the data plane is silent. Silence = "process dead", never "tracker dead" from a
  data-plane gap. The heartbeat thread is load-bearing — never let the core go
  silent while running.
- **P-05 — The "small Python adapter" trap.** Every external system is a separate
  process over ZMQ (ADR-001) — no pybind11/shared-memory bolt-ons. The cost is one
  process and one subscriber; the cost of violating it is non-deterministic core
  behaviour.
- **P-06 — Reproposing options when the user has decided.** After "go with X," don't
  relist rejected alternatives, hedge, or volunteer a fresh comparison unless the
  user reopens it. Exception: a new constraint/measurement that materially changes a
  rejected alternative — surface that explicitly.
- **P-07 — Inventing user context.** Don't invent experience level, tools,
  constraints, or unbought hardware. Ask or state assumptions as assumptions — the
  user cross-checks against other models.
- **P-08 — Calibration as a one-time event.** Calibration drifts; route through
  `calibration_status`. Do NOT propose continuous active recalibration — that's
  ADR-009 (Proposed). Current correct behaviour on drift is to surface it, not
  self-correct.

---

## Workflow conventions (§11.2/11.3/11.6)

**Checkpoint workflow (§11.2)** for substantial deliverables (a design doc, a new
ADR pack, a multi-file refactor, long-form analysis): (1) propose a structure and
get sign-off; (2) draft section by section, small enough to react to; (3) lock each
section before the next ("accepted" is the lock); (4) assemble at the end. Default
for any artifact over ~500 lines or whose structure is itself under design. Small
deliverables (a single ADR from a decided position, a bug fix, a one-file change)
skip it — deliver directly.

**File creation (§11.3):** real files for real deliverables (create actual files and
present them); outputs mirror the intended directory layout; drafts offered for
review are inline markdown, not files; present a delivery batch once, index/summary
first.

**Session opening/closing (§11.6):** no long context-recovery ritual — reading
CLAUDE.md, scanning the ADR README, and the current design doc is enough; load more
on demand. Don't summarise the conversation unprompted at the end. If a session
produced load-bearing decisions not yet in ADRs, surface that *during* the
conversation and offer to draft the ADR before closing.

---

## Glossary (§12)

Project-specific meanings; generic CV/robotics/SE terms (Kalman filter, homography,
intrinsics, reprojection error, rolling shutter, gating, Mahalanobis distance,
MAVLink, mutex, lock-free, pub/sub, …) are assumed known.

- **Tracking core / the core:** the single C++ headless process that ingests frames,
  detects, tracks, maps coordinates, evaluates `safe_for_control`, and publishes the
  ZMQ stream. One per Pi 5. Distinct from consumers (LaserController, MAVLink
  adapter, viewer).
- **Hot path:** the per-frame path at 60–90 Hz with a determinism contract; the
  discipline rules apply only here. Startup/config/control-message handling is not
  hot-path.
- **Adapter:** a separate process mediating between the core's ZMQ stream and an
  external protocol/hardware. Committed: LaserController (ADR-008), MAVLink (Phase 3+).
- **Track lifecycle states** (capitalised when referring to the state): Confirmed,
  Provisional, Predicted, Occluded, Lost, Retired.
- **Track:** a tracked-object instance with persistent identity across frames
  (managed by the Tracker). **Observation:** a single detection in a single frame
  from a single camera (input to the Tracker; image-plane coords). Tracks have
  floor-plane coords via the CoordinateMapper.
- **Floor frame / FloorPlane2D:** the shared world frame (ADR-006); Z=0 is the floor.
- **The predicate:** `safe_for_control` (ADR-007). Singular.
- **Snapshot:** a single `TrackingState` at one evaluation cycle; the predicate is
  evaluated on a snapshot, never on cross-time-aged data. The published ZMQ message
  is a serialised snapshot plus a system-health block.
- **The latency-robust contract:** the core never lies about freshness or safety;
  consumers degrade by holding position. Implemented jointly by ADR-002, ADR-007, and
  the heartbeat thread.
- **The hard configuration constraint:** a fact the system relies on but does not
  enforce (canonically: exactly one laser in the room). Violation → undefined
  behaviour.
- **Modulation pattern:** the laser on/off sequence (ADR-005; 15 Hz, 50/50 for v0.3).
- **The seam:** the interface between the core and the drone subsystem, bridged by the
  MAVLink adapter (Phase 3+). The core does not speak MAVLink; the drone does not
  subscribe to ZMQ.
- **v0.3 scope:** one camera, one laser, one ball, tabletop camera, floor frame; no
  drone integration, no multi-camera fusion exercised.
- **Phase 3+/4+/5+:** sequencing bands — Phase 3 (multi-camera, drone-as-tracked),
  Phase 4 (closed-loop laser aiming), Phase 5 (active calibration, ADR-009). Not a
  committed roadmap.

**Acronyms:** ADR, AGC, CSI, dbrs (Drone Ball Retrieval System namespace), EKF, FC,
FoV, GIL, HWM, IR, MCU, NED, NoIR, PSD, RAII, RT, SBC, SPSC, TTL, V4L2, VPE
(VISION_POSITION_ESTIMATE), YAML, ZMQ.
