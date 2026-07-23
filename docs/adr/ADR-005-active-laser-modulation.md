# ADR-005: Active Laser Modulation

## Status
Accepted (2026-05-08)

## Context
The original v0.2 detector design assumed the laser dot would be the brightest cluster of pixels in an IR-sensitive camera, allowing zero-cost CPU thresholding. This assumption fails in real home environments:

- Sunbeams through windows produce bright clusters that exceed any laser pointer's apparent intensity.
- Specular reflections of the laser itself (off polished tabletops, glassware, screens, glossy floors) produce bright clusters at locations other than the true laser hit point.
- A second laser pointer in the room (a plausible scenario in a household with children) produces a second indistinguishable bright cluster.
- Auto-exposure and ambient lighting changes alter the threshold at which "bright" is defined.

A static-brightness detector cannot distinguish the intended laser from any of these. The detector needs a temporal signature that is unique to the controlled laser source.

Forces:
- Detection must be robust to ambient bright sources.
- The system has full control over the laser hardware (it is part of our system boundary, not a third-party emitter).
- At 60 fps capture, Nyquist limits the modulation frequency we can robustly detect.
- The modulation must not introduce safety hazards (e.g., visible flicker triggering photosensitive responses).

## Decision
The laser will be actively modulated at a known frequency. The detector will treat any pixel as a candidate laser observation only if its temporal intensity pattern correlates with the expected modulation pattern over a rolling window.

Concrete parameters:

- **Modulation frequency:** 15 Hz default at 60 fps capture. Promote to 22 Hz when running at 90 fps capture, once thermal headroom is proven.
- **Duty cycle:** 50/50.
- **Detection window:** 4 frames (one full modulation period at 15 Hz / 60 fps).
- **Detection method:** frequency-domain correlation (power spectral density at the modulation frequency), phase-insensitive. Phase locking between the laser MCU and the Pi 5 is not required.
- **Laser type:** infrared preferred over visible-spectrum to eliminate visible flicker. The 5 MP NoIR camera is sensitive to IR; the operator can verify laser activity via the camera feed.
- **Pattern source of truth:** the modulation frequency and duty cycle are configured in the tracking core's YAML configuration. At startup, the tracking core transmits these parameters to the laser MCU over serial (per ADR-008). The MCU is the actor that physically modulates the laser; the core is the consumer that knows what pattern to expect.

### Derivation of 15 Hz

For robust detection (correlation SNR rather than mere reconstruction), we require ≥ 4 samples per modulation period:

```
f_modulation_max = f_capture / 4
                = 60 / 4
                = 15 Hz
```

This gives a per-period detection latency of 1 / 15 = 67 ms. This latency is added to the tracking pipeline and is acceptable under the latency-robust contract (the consumer hovers when fresh data is unavailable; see ADR-007).

### Initial-detection grace period

After core startup, the detector ignores observations until 2 full modulation cycles (~134 ms) have elapsed, ensuring the correlation window contains a complete signal. This grace period is published in `system_health.tracker_state` as `INITIALISING` and only transitions to `RUNNING` after the grace expires.

## Consequences

**Positive:**
- Sunbeams, ambient bright sources, and unmodulated reflections are rejected at the detector level — they have no power at the modulation frequency.
- A specular reflection of the modulated laser will *also* be modulated, but a secondary disambiguation rule (size envelope, brightness, or "primary detection only") can resolve this. Documented as a known residual ambiguity for v0.3.
- The detector is robust to auto-exposure changes (PSD is invariant to multiplicative gain).
- The same laser hardware is reusable for active calibration refinement (ADR-009).

**Negative:**
- 67 ms inherent detection latency at 60 fps. This shrinks the controller's available reaction time within a 100 ms total budget.
- The detector is now stateful and requires a pre-allocated rolling buffer of 4 frame snapshots.
- Visible-spectrum lasers will exhibit 15 Hz flicker, perceptible to humans. Mitigated by preferring IR.

**Risks:**
- **Modulation/shutter aliasing:** if `f_capture` becomes an integer multiple of `f_modulation`, the correlation can degenerate. Mitigated by selecting non-harmonic ratios (60 / 15 = 4 is at the limit; consider 14 Hz or 16 Hz if empirical detection SNR proves marginal).
- **Rolling-shutter row-time skew:** at 16.6 ms scan time and 15 Hz modulation, the top and bottom of a single frame may capture different modulation states. Detector must use per-row temporal alignment OR accept degraded correlation near frame edges. Resolution deferred to implementation; flagged as a v0.3 validation requirement.
- **Two modulated lasers, same pattern:** if a second pointer of the same model is introduced, both will produce valid detections. The system specification states there will only ever be one laser; this is a hard configuration constraint, not a runtime assumption. Documented in the v0.3 design document.

## Alternatives Considered
- **Static brightness threshold (v0.2 baseline).** Rejected as documented above.
- **Colour-based detection (red laser on RGB sensor).** Rejected: the NoIR sensor lacks colour information; ambient red sources (LEDs, sunset) still confuse the detector; doesn't solve the multi-source problem.
- **Spatial-pattern-based detection (multi-dot or structured laser).** Rejected: complicates the laser hardware and the calibration probe usage in ADR-009.
- **Higher modulation frequencies (e.g., 30 Hz at 60 fps).** Rejected: violates the 4-samples-per-period rule, reducing correlation SNR.

## Related ADRs
- ADR-008 (LaserController Adapter) — owns the serial link that programmes the laser MCU with this modulation pattern.
- ADR-009 (Active Calibration Refinement) — depends on this modulation infrastructure.
- ADR-007 (`safe_for_control` Predicate) — consumes detector output and applies geometric tests.

---

## Amendment (accepted 2026-07-23) — two-period detection window

> **Status: ACCEPTED (2026-07-23), user sign-off recorded.** Proposed by the
> TRK-009 implementation (plan `docs/plans/2026-07-21-001`, unit U9) and accepted
> by the user. This amendment **supersedes the "Detection window: 4 frames" line
> in the Decision above**; per the append-only ADR convention the original text is
> left in place and this section is authoritative for the window length and its
> consequences. The reaction-bound question this amendment raised has been decided
> (see "Reaction-bound decision" below).

**What changes.** The **detection window is two full modulation periods (8 frames
at 60 fps / 15 Hz), not one (4 frames).** Everything else in the Decision is
unchanged.

**Why the 4-frame window was insufficient (the load-bearing reason).** A 4-point
DFT cannot distinguish one modulation period from a single luminance step: the
sequences `[0,0,a,a]` and one square-wave period have identical power at the
modulation bin and identical (zero) power at the adjacent bin. Any moving edge or
light-switching transient therefore produces the same bin response as the laser,
and such a transient survives exactly `confirm_threshold` sliding windows — a
structural false-SAFE path. Over an 8-frame window the modulation sits at bin 2
(two periods), and the worst-case luminance step (edge runs of 2 or 6 ON samples)
carries bin power `2a²` against the true signal's `8a²` — a **6 dB separation**.
Because a bright step can still exceed a dim laser's absolute power floor, step
rejection is carried **jointly** by that separation and a **spectral-purity gate**:
under the two-sided convention `purity = 2·|X₂|² / (N·(Σx² − (Σx)²/N))` a pure
on-frequency signal scores 1.0 and the worst-case step scores 1/3, so a purity
floor above 1/3 (config-enforced structural minimum > 0.4) rejects steps the power
floor alone would admit.

**Latency consequences.**
- **First-detection latency ~133 ms** (8 frames at 60 fps), doubled from the
  original 67 ms. This exactly equals the existing 2-cycle grace period, and the
  grace period and the correlation-window fill are now **one mechanism** (a single
  contiguous-admitted-frame counter drives both).
- **Worst-case flip-to-false after the laser departs ~117 ms (derived).** Residual
  window power sustains fresh-timestamped detections at the old position for up to
  4 frames (~67 ms) after departure; the predicate then goes false via the laser
  `age_max_ms` clause (50 ms). The consumer-side transport-staleness rule does
  **not** cover this tail — it checks transport freshness, and these detections are
  transport-fresh with stale content — so the age clause is what closes it.

**Integral-bin constraint (accepted).** The window length derives as
`2 · target_fps / modulation_frequency_hz` and must be an exact integer ≥ 8
(config-validated). This **forecloses the "consider 14 Hz or 16 Hz" aliasing
fallback** noted in the Risks above: `2·60/14 = 8.57` and `2·60/16 = 7.5` are
non-integral, so neither is representable without a non-integer-bin correlator
(Goertzel-class), which is out of scope for v0.3. Accepted as a constraint of this
amendment; if empirical SNR later forces a non-harmonic ratio, that is a new ADR.

**Residual exposure (recorded, alongside the two-modulated-lasers constraint).** A
single ambient source that aliases *exactly* onto the modulation bin — e.g. a
45 Hz PWM source sampled at 60 fps aliases to 15 Hz — **is detected** by design;
temporal correlation cannot distinguish it from the laser. This is a known,
accepted residual for v0.3 (the same class as the "two identical modulated
pointers" constraint), surfaced by the detector's on-bin-interferer test as
expected behaviour rather than a failure.

**Reaction-bound decision (2026-07-23).** The derived worst-case flip-to-false is
~117 ms, above the 100 ms total reaction budget referenced in the Consequences.
**Decision: accept ~117 ms as the provisional v0.3 bound** (the replay gate, R13,
asserts flip-to-false within a 120 ms derived bound), and **defer any tightening
to the threshold re-provenance after the operator's modulated-laser recording
session.** Rationale:

- The 117 ms is a *derived* worst case from *provisional* thresholds (no real
  modulated-laser footage exists yet); the real stale-tail length and the right
  purity floor are unmeasured. Committing to ≤100 ms now ships a tighter guessed
  purity floor and a 100 ms gate that could fail on real footage and force a change
  anyway. The 120 ms gate is honest and passable today.
- In v0.3 nothing actuates (the D8 hardware-actuation gate is not built), so the
  17 ms delta has no physical consequence yet — this chooses the provisional
  default, and deferring costs only a replay-gate re-run if the bound is later
  tightened.
- The worst case is the narrow "settled laser instantly vanishes (off/occluded)"
  scenario; a laser *moving* to an unsafe location trips clause 7 (settled-speed)
  far sooner, and a laser that is off has no beam to be unsafe about.

**If tightening is chosen later, prefer the purity-floor route.** Raising
`psd_purity_min` above 5/8 shortens the stale tail and is **laser-only, a pure
config change, fully reversible, and touches no ADR-007 clause.** Reducing the age
clause is entangled: ADR-007's `AGE_MAX_MS` is a **single shared constant used by
both clause 5 (laser) and clause 6 (ball)**, so lowering it also tightens the ball
freshness gate; tightening the laser age *alone* would require splitting
`AGE_MAX_MS` into laser/ball thresholds — an ADR-007 clause change, not a config
tweak. The recording session becomes a measure-and-maybe-tighten step, not a
tighten-now step. This deferral holds unless an external requirement mandates a
demonstrated ≤100 ms reaction at v0.3 sign-off regardless of no actuation, in which
case tighten the purity floor and treat the session as validate-or-loosen.
