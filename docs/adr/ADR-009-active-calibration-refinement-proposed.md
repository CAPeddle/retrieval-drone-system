# ADR-009: Active Calibration Refinement Using the Laser as a Probe

## Status
**Proposed** (2026-05-08). Phase 5+. Not implemented in v0.3. Implementation requires the experimental observability validation described under "Decision" below.

## Context
ADR-004 establishes a manual-Charuco extrinsic calibration procedure (Phase 1) and a static-marker health-monitoring procedure (Phase 2). Both rely on physical fiducials: someone places a Charuco board on the floor at install time, an AprilTag stays in view for ongoing health checks.

A different approach was proposed during v0.3 design: use the controlled laser itself as an *active probe*. The laser MCU steps the pan/tilt servos through a known sequence of angles; the camera records the pixel coordinate at which each laser dot appears; this builds a dense `(servo_angle, pixel) → floor_coordinate` mapping. With one or more known floor anchors and a parameterised laser-pose model, the system may be able to recover a refined pixel→floor homography with denser sampling than a manual Charuco calibration provides.

This is architecturally appealing for several reasons:
- Calibration becomes largely automated; the operator places one anchor and walks away.
- The dataset is dense (200+ points across the FoV in 30 seconds) compared to a manual 4–10-point Charuco calibration.
- Calibration health monitoring becomes continuous: every laser observation during normal operation is also a calibration datum.
- The same hardware (modulated laser, MCU, servos) is reused.

However, the approach has significant unanswered questions about whether it actually improves on Phase 1 calibration in practice.

## Decision (Proposed)
We will implement active calibration refinement as an *additive layer* on top of Phase 1 manual calibration, **only after** the following observability and accuracy preconditions are experimentally validated:

### Observability validation requirements

1. **Laser pose model is identifiable.** Given the planned sweep pattern, anchor configuration, and servo accuracy, the optimisation problem of jointly solving for laser emitter pose, camera pose, and pixel→floor mapping must be shown to be observable (i.e., the Jacobian is full-rank at the operating point). This will be demonstrated by simulation before any physical implementation.

2. **Servo angular repeatability is characterised.** The pan/tilt mechanism's angular repeatability over a representative sweep must be measured. Repeatability worse than 0.05° at the laser emitter renders the active-probe approach inferior to a careful manual Charuco calibration. The repeatability budget must be established empirically against the planned hardware (MID or PRO build per the CopilotServoCOntrol document) before the ADR moves out of Proposed status.

3. **Anchor sensitivity is bounded.** The accuracy of the recovered pixel→floor mapping must degrade gracefully (not catastrophically) as anchor positions are perturbed. A configuration that requires sub-millimetre anchor placement is operationally fragile; one that tolerates ±5 mm anchor placement is acceptable.

4. **Cross-validation against Phase 1 must succeed.** On at least three distinct camera installations, the active-probe-derived homography and the manual-Charuco-derived homography must agree to within the residual reprojection error of either method individually. Disagreement is a red flag for either approach.

### Conservative phrasing on the "one anchor solves everything" formulation

An earlier formulation of this ADR implied that a single Charuco anchor combined with a laser sweep could solve the full calibration problem. That formulation was too strong. The honest statement is:

> One or more known floor anchors, combined with laser sweep observations and a parameterised laser pose model, may permit refinement of the pixel→floor mapping. The problem is an optimisation whose observability depends on the number, spatial distribution, and accuracy of anchors; servo repeatability; and assumptions about the laser mount. Whether one anchor suffices is an empirical question to be answered by the validation in clauses 1–4 above.

### Architectural placement (when implemented)

Active calibration refinement will be implemented as:
- An offline calibration mode run via a separate Python tool, not integrated into the tracking core's hot path.
- Output: an updated calibration JSON file in the same format as Phase 1 calibration.
- The tracking core treats the refined calibration identically to a manual one (no special-casing).
- Online refinement (continuous re-fit during normal operation) is explicitly **out of scope** for ADR-009; that would be a future ADR.

## Consequences (when accepted)

**Positive:**
- Reduced human effort per camera install.
- Higher-density sampling potentially reduces residual reprojection error.
- The laser-control infrastructure has a justified second use (beyond aiming for retrieval), strengthening the case for investing in higher-grade servos.

**Negative:**
- Adds a dependency: calibration refinement now requires a working laser-control subsystem. Mitigated by keeping Phase 1 (manual) as a viable alternative; Phase 3 is *additive*, not replacing.
- Increases implementation complexity (optimisation routine, sweep planner, anchor detection, cross-validation tooling).

**Risks:**
- **Servo accuracy proves insufficient.** Phase 3 produces calibration worse than Phase 1. Mitigated by the observability validation gate.
- **Optimisation fails to converge in operationally interesting configurations.** Mitigated by retaining Phase 1 calibration as a seed for the optimisation, and by reporting Phase 1 calibration as a fallback if refinement diverges.
- **The validation effort itself is non-trivial.** Honest estimate: characterising servo repeatability and running the simulation-based observability analysis is a 2–4 week effort distinct from any calibration code.

## Alternatives Considered
- **Skip active calibration entirely.** The default if the validation gates above are not met. Phase 1 + Phase 2 (per ADR-004) remain the production calibration approach.
- **Online continuous calibration during operation.** Considered and explicitly deferred. Online re-fit during operation creates a feedback loop where bad observations corrupt calibration which corrupts future observations. Out of scope for ADR-009.
- **Use a structured-light projector instead of a single laser.** Considered. The single laser is preferred because it is already part of the system boundary for primary tracking; adding structured light is new hardware.

## Related ADRs
- ADR-004 (Hybrid Calibration Lifecycle) — Phase 3 of the lifecycle.
- ADR-005 (Active Laser Modulation) — provides the detection infrastructure used by the probe.
- ADR-008 (LaserController Adapter) — must be extended with a "drive to angle" command for sweep execution.
