# Concepts

Shared domain vocabulary for this project — entities, named processes, and status concepts with project-specific meaning. Seeded with core domain vocabulary, then accretes as ce-compound and ce-compound-refresh process learnings; direct edits are fine. Glossary only, not a spec or catch-all.

## Replay testing

### Scenario
A recorded clip of real camera footage under a named condition (normal, underexposed, overexposed) used to replay the perception pipeline deterministically without hardware. Scenarios are the unit the ship gate counts: the release gate requires a minimum number of distinct scenarios, not a minimum duration of one.

### Replay Gate
A test that runs a detector over a Scenario's frames and asserts its behaviour on real footage — most importantly zero false positives, the project's cardinal failure. A failing Replay Gate is calibration signal, never a tuning target: thresholds are not adjusted to make one pass.

### True-Positive Counterweight
A companion test to a zero-false-positive Replay Gate that composites a synthetic target into the same real footage and requires the detector to find it. It bounds the detector from the opposite direction, so "detect nothing" cannot satisfy the gates. Composited targets must model the target's physical effect on the scene (an opaque target occludes what is behind it), not merely its appearance.

### Frame Quality
The per-frame admission status assigned before detection: GOOD (use), DEGRADED (use but flag), REJECT (skip). Detection gates are evaluated only over quality-admitted frames, so a Replay Gate failure means the detector misbehaved on a frame it was supposed to handle.

### Provisional Default
A numeric configuration default shipped without measured provenance from real footage, marked as provisional at its definition site. It survives only until a bench recording exists to measure the real value; replacing a Provisional Default with a measured one is expected follow-up work, not tuning.

### On-Target Run
A build-and-test cycle executed natively on the deployment hardware (the Pi 5), as opposed to the development box. Load-bearing rather than a nicety: version-seamed code compiles only one branch per environment, and the replay recordings live only on the target, so an On-Target Run is the sole existence proof for the deployed branch and the Replay Gates. A change to either is unverified until the On-Target Run is green.

## Calibration

### Calibration Artifact
A versioned JSON file produced by an offline calibration tool (intrinsics per camera, extrinsics per camera-to-floor mapping) and consumed by the runtime at startup. Its metadata fields describe what the producing tool actually did and must be derived from the tool's behaviour, never asserted as constants; a well-formed Calibration Artifact is self-consistent — the transform it stores, applied to the inputs it stores, reproduces the outputs it stores — so a consumer can verify it without trusting the metadata.

### Floor Anchor
A physical marker placed at a hand-measured floor position whose detected image location, paired with its measured floor coordinates, anchors the camera-to-floor mapping. Anchors are few and hand-measured, so a bad one must fail the calibration's validation gates rather than be silently excluded as an outlier.
