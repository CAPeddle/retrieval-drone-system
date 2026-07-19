---
title: "A detector replica is not the oracle: threshold provenance runs the production binary"
captured: 2026-07-19
applies_to: [tracking-core]
tags: [threshold-provenance, replica-testing, probe-binary, opencv, oracle]
problem_type: pattern
source: internal
module: "tracking-core (TRK-010 ball detector gates)"
component: detection
applies_when: "Deriving measured provenance for any detection threshold or gate value; whenever a reimplementation of production logic is proposed as measurement apparatus"
---

# Replicas are direction-finders, not oracles — sweep thresholds with a probe binary against the production lib

## Summary

A reimplementation of a detector — even one that calls "the same OpenCV
functions" with the same parameters — is a different binary, a different
library build, a different platform. It will agree with production almost
everywhere and diverge exactly at decision boundaries, which is where
thresholds live. When deriving measured provenance for a threshold, use the
replica to find the interesting region cheaply, then run the definitive sweep
through the production code itself: a ~40-line probe binary compiled on the
target against the shipped static library. Reach for this whenever a replica
sweep and the real test gate disagree — the gate wins, and the disagreement
itself is signal worth chasing.

## Context

`ball.brightness_threshold` shipped at a provisional 200, but the real ball
peaks at 191 grey — the detector was blind to its own target. The task was to
replace the guess with a measured value backed by the recorded clip library,
under the standing zero-false-positive discipline (§5, ADR-007's cardinal
sin). This extends the probe-binary recipe from
[composite test targets must model occlusion](2026-07-18-composite-test-targets-model-occlusion.md):
there, a probe diagnosed a detector on extracted frames; here, a probe swept a
threshold with the production oracle.

## What happened

**The replica said clean.** A faithful Python/cv2 replication of the C++
`BallDetector` — same GaussianBlur k=5, `THRESH_BINARY`, `MORPH_CLOSE` 5×5
ellipse, `RETR_EXTERNAL` contours, πr² area bounds, circularity
4πA/P² ≥ 0.82, solidity ≥ 0.85, best-by-circularity×area selection — swept
130–190 over the recorded library and reported **zero false positives at
every value**. Candidate: 130.

**The gate said otherwise.** With 130 applied, the on-Pi suite failed
`ReplayIntegration.NormalClipNoBallFalsePositives`: "4 ball false positives in
64 quality-passed frames". The replica and the production detector disagreed
on marginal blobs whose circularity flickers around the 0.82 gate — pip-wheel
OpenCV in the dev venv and the Pi's system OpenCV extract contours and
arc lengths subtly differently on borderline shapes. Close is not equal at a
decision boundary.

## Approach / Pattern

Compile a small probe **on the Pi**, linking the production static lib and its
FetchContent dependencies, so the sweep exercises the exact code and library
build that the gate does:

```bash
g++ -O2 -std=c++17 \
  -I src/core/include \
  -I ../build-release/_deps/yaml-cpp-src/include \
  -I ../build-release/_deps/spdlog-src/include \
  probe.cpp \
  ../build-release/src/core/libtracking_core_lib.a \
  .../libyaml-cpp.a .../libspdlog.a \
  $(pkg-config --cflags --libs opencv4) -pthread
```

The probe (~40 lines) loads the real `Config`, constructs the real
`FrameQualityAssessor` and `BallDetector`, overrides the one field under test
from argv, and prints per-clip counts: frames, quality-admitted, detections,
and detections near the known ball position. The definitive 6-thresholds ×
6-clips sweep took minutes — and the per-location output revealed that every
"false positive" was in fact *at* the physical ball, a discovery about the
gate's premise the replica could never have surfaced.

Generalised rules:

1. **Replicas are direction-finders, never provenance oracles.** Any
   reimplementation, in any language, diverges from production exactly at
   decision boundaries — which is where thresholds live.
2. **Provenance sweeps run the production code on the target platform.** A
   probe binary against the shipped static lib costs ~40 lines and minutes;
   that price buys the only answer that matches the gate.
3. **Design probes to output more than the pass/fail question.** Per-location
   counts here exposed the gate-premise finding; a boolean would have hidden it.
4. **When replica and gate disagree, the gate wins — then chase the
   disagreement.** Both of this session's deepest findings came from treating
   the mismatch as evidence rather than noise.

## Trade-offs

- The probe hard-codes build-tree include and archive paths, so it rots when
  the dependency layout changes; it is a disposable instrument, not a
  maintained target. Promote it into the CMake tree only if the sweep recurs.
- Requires a completed release build and access to the Pi — heavier than a
  local Python sweep. The replica keeps its place for cheap coarse
  exploration; only the final numbers must come from the oracle.
- Overriding a single config field from argv keeps the probe honest but means
  every other parameter is pinned to the shipped config — a multi-parameter
  sweep needs a deliberate redesign, not more argv flags.

## Source links

- Threshold under test: `ball.brightness_threshold` in `tracking-core/config/tracking_core.yaml` (provisional 200; measured ball peak 191)
- The gate that overruled the replica: `ReplayIntegration.NormalClipNoBallFalsePositives` in the tracking-core replay suite
- Production lib the probe links: `tracking-core/build-release/src/core/libtracking_core_lib.a` (via `tools/pi5-remote-test.sh` builds)
- Precedent probe-binary recipe: [composite test targets must model occlusion](2026-07-18-composite-test-targets-model-occlusion.md)
- Same-subsystem library-build divergence: [OpenCV version-portability seam](2026-07-18-opencv-aruco-version-portability-seam.md)
- Probe-binary precedent this extends: [composite test targets must model occlusion](2026-07-18-composite-test-targets-model-occlusion.md)
- The premise discovery the probe surfaced: [replay-gate premises are physical-scene assertions](../workflow/2026-07-19-replay-gate-premises-are-scene-assertions.md)
- Same-session sibling: [decompose physical blockers via remote probing](../workflow/2026-07-19-decompose-physical-blockers-via-remote-probing.md)
