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
