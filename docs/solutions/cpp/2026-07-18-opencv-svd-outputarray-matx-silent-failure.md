---
title: "cv::Mat wrappers around Matx are copies — OpenCV OutputArray results silently lost"
captured: 2026-07-18
applies_to: [tracking-core]
tags: [opencv, svd, outputarray, matx, silent-failure, homography]
problem_type: pattern
source: internal
module: "tracking-core (TRK-017 homography loader, condition-number gate)"
component: coordinate-mapping
severity: medium
root_cause: wrong_api
resolution_type: code_fix
symptoms:
  - "sigma_min reads 0.0 for every input, including known-healthy fixtures"
  - "A near-singular gate rejects all homographies unconditionally"
  - "No compiler warning, no runtime error — the SVD call reports success"
---

# cv::Mat wrappers around Matx are copies — SVD output silently lost

## Summary

`cv::Mat(const Matx&)` *copies* the Matx data into a new Mat; it does not alias
it. Passing that wrapper as an OpenCV OutputArray is therefore a silent no-op on
the original: `create()` reallocates the temporary's buffer, the results land in
storage that dies at the end of the expression, and the named Matx keeps its
zero-initialised contents. In the TRK-017 homography loader this made every
singular value read back as 0.0, the condition number effectively infinite, and
*every* homography — including a mathematically healthy fixture — rejected as
near-singular. No warning at any point. Reach for this when an OpenCV output
value is inexplicably zero, or when a validity gate rejects an input you can
prove is fine.

## Context

TRK-017's homography loader gates degenerate extrinsics by spectral condition
number: SVD of the (unit-normalised) 3×3 homography, then
`sigma_max / sigma_min` compared against `coordinate.condition_number_max`. The
singular values were requested like this:

```cpp
cv::Matx<double, 3, 1> singular_values;
cv::SVD::compute(cv::Mat(h_normalised), cv::Mat(singular_values));
const double sigma_max = singular_values(0, 0);
const double sigma_min = singular_values(2, 0);
```

## Symptoms

- `HomographyLoaderTest.ValidFixtureLoads` failed with "extrinsics homography
  is near-singular" on a synthetic fixture constructed to be well-conditioned.
- Every homography was rejected, regardless of conditioning or threshold.
- `sigma_min == 0.0` on inputs whose smallest singular value is provably
  positive — the tell that the values were never written back.

## What didn't catch it / diagnosis path

The compiler and OpenCV are both silent: the wrapper conversion is legal, the
OutputArray reallocation is by design, and no runtime check notices that the
destination was a temporary. The first hypothesis was a genuinely too-strict
threshold — and a real secondary issue surfaced first: the raw pixel→metre
homography's condition number is unit-dominated (~1e4–1e6 for healthy
geometries), fixed by Hartley-style unit normalisation of both domains before
conditioning. But the failure *persisted* after normalisation with the
threshold at 1e5, which isolated the plumbing: the singular values were simply
not arriving. Two distinct bugs, one symptom; the known-answer fixture — a
healthy input that MUST pass — is what kept the investigation honest. An
invalid-input-must-throw suite alone would have passed this bug happily.

## Approach / Pattern

Give the OutputArray a real `cv::Mat` it can own and reallocate, then read
back explicitly:

```cpp
cv::Mat singular_values;  // SVD reallocates its output; a Matx wrapper would be copied
cv::SVD::compute(cv::Mat(h_normalised), singular_values);
const double sigma_max = singular_values.at<double>(0);
const double sigma_min = singular_values.at<double>(2);
```

Generalised rules:

1. **Never pass a copying wrapper (Mat-from-Matx, Mat-from-C-array) as an
   OpenCV OutputArray.** Reallocation orphans your destination. Pass a real
   `cv::Mat` and read back. (Mat *inputs* wrapping a Matx are fine — copies
   read correctly.)
2. **A zero where the mathematics guarantees a positive value** (singular
   values of a non-zero matrix, norms, determinants of known-good inputs) is a
   plumbing suspect before it is a maths suspect.
3. **Pair rejection tests with known-answer fixtures.** A healthy input that
   must pass is the only kind of test that catches silent-zero failures in a
   validity gate.

## Trade-offs

- `singular_values.at<double>(i)` is less tidy than Matx indexing, and the
  element type is now a runtime convention rather than a compile-time one.
- OpenCV does offer templated `cv::SVD::compute(w, u, vt)` overloads that write
  into Matx directly; the Mat-output form with explicit read-back was chosen as
  the least surprising. Performance is irrelevant — this is a startup-only
  path.

## Source links

- Fixed conditioning gate: `tracking-core/src/core/coordinate/homography_loader.cpp` (SVD conditioning check, TRK-017)
- Known-answer fixture that caught it: `HomographyLoaderTest.ValidFixtureLoads` in the TRK-017 unit suite
- Threshold: `coordinate.condition_number_max` in `tracking-core/config/tracking_core.yaml`
- Sibling TRK-017-adjacent silent bug: [serialised artifact metadata must be derived from behaviour](../python/2026-07-18-artifact-metadata-derived-not-asserted.md) — same pipeline, same "passed every behavioural test, caught only by a value-checking test" shape, different mechanism
- Another OpenCV API-contract trap, same subsystem: [OpenCV version-portability seam](2026-07-18-opencv-aruco-version-portability-seam.md)
- Same-batch "the test setup, not the code" precedent: [composite test targets must model occlusion](2026-07-18-composite-test-targets-model-occlusion.md)
- Sibling TRK-018 pattern whose orthonormalisation step applies this rule: [camera centre from a world-plane homography](2026-07-18-camera-centre-from-plane-homography.md)
