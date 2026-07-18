---
title: Composite test targets must model occlusion, not just target appearance
captured: 2026-07-18
applies_to: [tracking-core]
tags: [testing, replay-harness, ball-detector, synthetic-compositing, occlusion]
problem_type: pattern
source: internal
module: tracking-core/tests/cpp_unit
severity: medium
applies_when:
  - "Pasting synthetic targets into real recorded footage for true-positive detector tests"
  - "A shipped-config detector fails a composited-frame test while the false-positive replay gates still pass"
  - "Diagnosing per-frame detector rejections where unit-test assertions give no visibility"
symptoms:
  - "CompositeBallIsDetectedOnRealFrames failed 0/10 on-target (Pi 5, ov5647 recordings)"
  - "Pasted bright disc merged with the room's over-threshold bright regions into one large non-circular blob"
  - "Every candidate contour had circularity below 0.66, under the 0.82 gate"
root_cause: logic_error
resolution_type: test_fix
---

# Composite test targets must model occlusion, not just target appearance

## Summary

When compositing a synthetic target into real recorded footage for a
true-positive detector gate, paste the target's *physical effect on the scene*,
not just its appearance. An opaque object occludes whatever is behind it; a
naive paste of bright pixels produces a physically impossible frame in which
the target merges with the scene's own bright clutter. The fix is one line —
clear an occlusion halo before drawing the target — but the principle
generalises to any synthetic-on-real compositing. Reach for this when a
composited-frame test fails while the zero-false-positive gates pass: that
combination usually means the detector is right and the test's compositing is
wrong.

## Context

TRK-010 shipped the `BallDetector` (fixed `brightness_threshold: 200`,
`min_circularity: 0.82`, convexity ≥ 0.85) alongside zero-false-positive replay
gates over real Pi 5 recordings (TRK-031 harness). A zero-FP gate alone is
trivially satisfied by a detector that detects nothing, so U3 added a
counterweight: `CompositeBallIsDetectedOnRealFrames` pastes a synthetic white
ball into ten quality-gated real frames and requires the shipped config to
locate it within 2 px in at least nine.

On-target the counterweight failed **0/10** — while the FP gates passed. Unit
tests on synthetic-only images had always passed, so the failure appeared only
when the synthetic target met real footage.

## Diagnosis: the probe-binary recipe

GTest assertions say *that* nothing was detected, not *why*. The useful move
was to extract one failing real frame from the recording and run a small local
probe binary that reruns the detector's pipeline stages and dumps every
candidate contour with its metrics (area, circularity, convexity). That showed
the mechanism directly: after `cv::threshold` at 200, the pasted disc had
merged with the room's own bright regions (windows, reflections) into one large
blob — every contour scored circularity < 0.66, below the 0.82 gate. The
detector was **correctly** rejecting non-circular bright clutter; the FP gates
passing simultaneously was the corroborating evidence.

Two tempting fixes were rejected:

- **Lowering `min_circularity` or the brightness threshold** — forbidden
  (never relax assertions to make a test pass), and 0.82 is load-bearing: a
  perfect square's circularity is π/4 ≈ 0.785, so anything lower re-admits
  square-ish blobs.
- **Moving the paste position to an empty image region** — dodges the realism
  question and weakens the gate to "detects a ball on a clean background",
  which the synthetic unit tests already cover.

## Approach / Pattern

Model the occlusion. A real ball is opaque: it hides the background behind its
silhouette, so its thresholded contour is separated from adjacent bright
clutter by its own boundary. Reproduce that by clearing a slightly larger dark
halo before drawing the bright disc:

```cpp
cv::Mat composite = frame.clone();
// A real ball occludes whatever is behind it, including any bright room
// clutter — model that by clearing a small halo before drawing the disc
// so the synthetic ball is isolated (without it, the disc merges with
// bright background regions and its circularity is destroyed, which
// tests scene interaction rather than detector recall).
cv::circle(composite, paste_at, paste_radius + 8, cv::Scalar::all(0), cv::FILLED);
cv::circle(composite, paste_at, paste_radius, cv::Scalar::all(255), cv::FILLED);
```

The before state was the second `cv::circle` alone. The `+ 8` margin only
needs to exceed the blur kernel's mixing distance so thresholding cannot bridge
the gap; it is not a tuned constant.

The general form of the pattern, for any synthetic-on-real compositing:

1. **Composite the physics, not the sprite.** Opaque targets occlude
   (clear-then-draw); translucent or emissive targets blend. Ask what the
   sensor would actually have seen.
2. **Pair every zero-FP gate with a true-positive counterweight** on the same
   footage, so the detector is bounded from both directions and "detect
   nothing" cannot pass.
3. **When recall fails but FP gates pass, suspect the test before the
   thresholds.** Diagnose with the extracted-frame probe binary before touching
   any shipped default.

## Trade-offs

- The halo isolates the target from *legitimate* scene interaction too — this
  test proves recall of a clean, occluding ball, not recall under partial
  occlusion or against adjacent bright objects. Those need real ball-in-scene
  recordings (bench follow-up already logged for the provisional `ball.*`
  defaults).
- A composited target can never validate the shipped defaults' true-positive
  performance on a *real* ball (different rim shading, motion blur, IR
  response). The counterweight bounds the gate; measured provenance still
  comes from bench recordings, per the TRK-008 precedent.

## Worked example

The full test is
[`test_replay_integration.cpp`](../../../tracking-core/tests/cpp_unit/test_replay_integration.cpp)
(`CompositeBallIsDetectedOnRealFrames`): ten quality-gated frames from the
`normal` scenario, paste at frame centre with radius
`(expected_radius_px_min + expected_radius_px_max) / 2`, detection counted when
the centroid lands within 2 px. After the halo fix: ≥ 9/10 detected and the
ball/marker FP gates still at zero — 82/82 green on the Pi 5, both configs.
The circularity gates that drove the diagnosis live in
[`ball_detector.cpp`](../../../tracking-core/src/core/tracking/ball_detector.cpp).

## Source links

- Test with the occlusion-halo compositing: `tracking-core/tests/cpp_unit/test_replay_integration.cpp` (composite true-positive gate)
- Detector gates that correctly rejected the merged blob: `tracking-core/src/core/tracking/ball_detector.cpp`
- Shipped provisional defaults with provenance comments: `tracking-core/config/tracking_core.yaml` (`ball:` section)
- Replay harness the gate runs on: `tracking-core/src/core/replay/replay_source.cpp` (TRK-031), driven by `tools/pi5-remote-test.sh`
