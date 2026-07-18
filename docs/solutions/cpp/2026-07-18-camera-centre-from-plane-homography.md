---
title: "Camera centre from a world-plane homography: plane-pose extraction, not decomposeHomographyMat"
captured: 2026-07-18
applies_to: [tracking-core]
tags: [opencv, homography, plane-pose, decomposehomographymat, camera-centre, z-compensation, adr-010]
problem_type: pattern
source: internal
module: "tracking-core (TRK-018 CoordinateMapper, plan KTD-4)"
component: coordinate-mapping
---

# Camera centre from a world-plane homography: plane-pose extraction, not decomposeHomographyMat

## Summary

ADR-010 Z compensation needs the camera centre **C** in floor coordinates, but
the TRK-013 extrinsics artifact stores only the homography **H** (undistorted
pixels → FloorPlane2D metres, Z = 0) — no pose. C must be recovered from H and
the intrinsics K. The obvious-looking tool, `cv::decomposeHomographyMat`, is
contractually wrong for this: it models a homography **between two camera
images** (H = K(R + t·nᵀ/d)K⁻¹) and its internal K⁻¹·H·K normalisation is
meaningless when the "second view" is a metric floor frame rather than a
K-camera — the recovered R/t do not match physical geometry, and it returns up
to four candidate solutions besides. The correct pattern is Zhang-style
plane-pose extraction: B = K⁻¹·H⁻¹ ∝ [r1 r2 t], normalise by ‖b1‖,
orthonormalise via SVD, C = −Rᵀt, and resolve the single sign ambiguity by
requiring C_z > 0 (camera above the floor). About 25 lines, all at startup,
unique up to one physically-resolvable sign. The error was caught at plan
review — two independent reviewers converged on it before any code ran.

## Context

TRK-018 (plan KTD-4) implements coordinate mapping with object-class Z
compensation. The compensation formula is a similarity scaling about the camera
centre: for an object whose centroid sits at height h, the true floor position
is X_h = C + (C_z − h)/C_z · (X₀ − C), where X₀ is the naive Z = 0 projection
through H. Everything in that formula is available from the calibration
artifacts except C — the TRK-013 offline extrinsic calibration deliberately
stores only the 3×3 pixel→floor homography, not a full pose, because the hot
path only needs the projective map.

The original plan draft specified `cv::decomposeHomographyMat` to recover the
pose. The name is a perfect match for the problem statement ("decompose a
homography into R and t"), which is precisely why it survived into a plan. Two
independent code reviewers flagged it during plan review; one verified the
contract directly in `/usr/include/opencv4/opencv2/calib3d.hpp`. No wrong code
was ever run — but only because the review checked the function's *model*, not
its name.

## Why decomposeHomographyMat is wrong here

`cv::decomposeHomographyMat` implements Malis & Vargas' decomposition of a
homography **induced by a plane between two camera views sharing intrinsics
K**:

    H = K (R + t·nᵀ/d) K⁻¹

Internally it computes K⁻¹·H·K to strip the intrinsics from *both sides* before
decomposing. That is the contract, and it is stated in the calib3d
documentation and header: the input is an image-to-image homography.

Our H is nothing of the sort. It maps undistorted pixels to **metric floor
coordinates** — the "second view" is an orthographic, metre-scaled world plane,
not a projection through K. Feeding it to `decomposeHomographyMat` makes the
K⁻¹·H·K normalisation apply K to a side of the mapping that never involved a
camera. The function runs happily and returns rotations and translations; they
simply do not correspond to the physical camera pose. This is the worst kind of
wrong: no error, plausible-looking numbers, and a bias that would only surface
downstream as a Z-compensation residual.

Two further strikes, even if the model matched:

- **Four-way ambiguity.** It returns up to four candidate (R, t, n) solutions
  and leaves disambiguation to the caller (visibility constraints, reference
  points). Every filtering branch is a wrong-branch risk.
- **Wrong direction of fit.** The two-view formulation needs the plane normal
  and depth d to reconstruct a pose; our problem already *knows* the plane
  (Z = 0, metric) — the world-plane structure makes the extraction nearly
  closed-form. Reaching for the general tool discards the structure that makes
  the specific problem easy.

The generalised rule: **when a library function's name matches your problem,
check its model — the contract lives in the maths, not the name.** And its
corollary: `decomposeHomographyMat` is for image-to-image homographies only;
world-plane homographies use plane-pose extraction.

## Approach / Pattern

The Zhang-style extraction, as implemented in the `CoordinateMapper`
constructor (`tracking-core/src/core/coordinate/coordinate_mapper.cpp`). All of
it runs once at startup; the hot path only ever sees the finished
`camera_centre_` and normalised `homography_`.

**Step 1 — normalise H with a fixed sign.** H is Frobenius-normalised, sign
fixed so the projective denominator at the principal point is positive (the
TRK-013 loader already proves single-sign over the working region, so one point
suffices). This makes the later absolute epsilon on the denominator meaningful
and pins the scale that the extraction depends on:

```cpp
cv::Matx33d h = artifacts.extrinsics.homography;
double frob = 0.0;
for (int r = 0; r < 3; ++r)
    for (int c = 0; c < 3; ++c) frob += h(r, c) * h(r, c);
h *= 1.0 / std::sqrt(frob);
if (h(2, 0) * cx + h(2, 1) * cy + h(2, 2) < 0.0) h *= -1.0;
homography_ = h;
```

**Step 2 — form B = K⁻¹·H⁻¹.** H⁻¹ maps floor → pixels. Under the pinhole
model, pixel ∼ K·[r1 r2 t]·(X, Y, 1)ᵀ for a Z = 0 point, so
B = K⁻¹·H⁻¹ ∝ [r1 r2 t] — the first two rotation columns and the translation,
up to one common scale:

```cpp
const cv::Matx33d b = camera_matrix_.inv() * homography_.inv();
const cv::Vec3d b1{b(0, 0), b(1, 0), b(2, 0)};
const cv::Vec3d b2{b(0, 1), b(1, 1), b(2, 1)};
const cv::Vec3d b3{b(0, 2), b(1, 2), b(2, 2)};
```

**Step 3 — recover the scale.** r1 is a unit vector, so λ = ±1/‖b1‖. Then
r1 = λ·b1, r2 = λ·b2, and r3 = r1 × r2 completes a right-handed frame.

**Step 4 — orthonormalise.** Calibration noise means [r1 r2 r3] is only
approximately a rotation. Take the nearest rotation via SVD: R = U·Vᵀ, with a
determinant correction (flip a column of U if det < 0):

```cpp
cv::SVD::compute(cv::Mat(rot), w, u, vt);
cv::Mat r_mat = u * vt;
if (cv::determinant(r_mat) < 0.0) {
    u.col(2) *= -1.0;
    r_mat = u * vt;
}
```

**Step 5 — camera centre.** t = λ·b3, and C = −Rᵀ·t, in floor coordinates.

**Step 6 — resolve the one sign ambiguity physically.** λ's sign is the only
ambiguity in the whole extraction. Try both; keep the one that places the
camera above the floor (C_z > 0). If neither works the calibration artifact is
unusable — fail loudly at startup, never limp on:

```cpp
for (const double sign : {1.0, -1.0}) {
    const double lambda = sign / norm_b1;
    // ... steps 3–5 ...
    if (centre[2] > 0.0) { camera_centre_ = centre; found = true; break; }
}
if (!found) {
    throw CalibrationError(
        "plane-pose extraction failed: no sign choice places the camera above "
        "the floor (C_z > 0)");
}
```

Startup code may throw (the hot-path no-exceptions rule does not apply here);
`CalibrationError` at construction is the correct failure mode.

Versus `decomposeHomographyMat`: the solution is unique up to that single
physically-resolvable sign — no four-branch candidate filtering — and the whole
extraction is ~25 lines whose maths is visible on the page. Per-observation
cost afterwards is one homography multiply plus the similarity scale
X_h = C + (C_z − h)/C_z · (X₀ − C); nothing pose-related touches the hot path.
Prefer the formulation with fewer ambiguous branches when both exist — every
branch you must filter is a branch you can filter wrongly.

## Verification

Two gates pin the extraction, at different altitudes
(`tracking-core/tests/cpp_unit/test_coordinate_mapper.cpp`):

- **Known-geometry fixture** (`RecoversCameraCentreFromHomography`): the
  fixture builds H analytically from a camera at (0, 0, 1) with 45° tilt (the
  ADR-010 worked-example geometry), and the test requires the recovered C to
  match to 1e-6 in all three components. This is the unit that catches the
  failure class directly — a sign slip, a transposed R, or a skipped
  orthonormalisation fails *here*, with an interpretable three-number diff, not
  in the field as a mystery bias.
- **End-to-end bias gate** (`Adr010WorkedExampleBiasIsCorrected`): a ball
  centroid at Z = 0.03 m projected naively through H shows > 2 cm forward bias
  (asserted — the test proves the problem exists in this geometry); the
  compensated projection must land within 5 mm of truth. A wrong C that somehow
  slipped past the fixture surfaces as a > 5 mm residual here.

The pairing is the point: pose-from-homography code must be pinned by a
known-geometry fixture recovering the camera centre *and* an end-to-end bias
gate. The fixture localises the fault; the gate guarantees the property the
system actually needs. Had `decomposeHomographyMat` shipped, the fixture is the
test that would have failed immediately.

## Trade-offs

- **Hand-rolled maths versus a library call.** Twenty-five lines of linear
  algebra we own and must maintain, against a one-call function that is wrong
  for this input. Not close — but the hand-rolled code is only defensible
  because the known-geometry fixture pins it to 1e-6. Without that test the
  library call's battle-tested implementation would be a real argument.
- **Startup-only cost.** SVD and matrix inversion at construction are free by
  hot-path standards; the design deliberately keeps the per-frame path to a
  multiply and a scale. The price is that a recalibration requires
  reconstructing the mapper — acceptable, since calibration reload is already a
  cold-path event.
- **The C_z > 0 disambiguator assumes a camera above the floor.** True for
  every ceiling/tripod mount this project will use; a floor-level camera
  looking up at a ceiling plane would need a different physical constraint. The
  assumption is enforced (loud `CalibrationError`), not silent.
- **Reviewer time versus runtime failure.** The trap cost two reviewers a
  contract check at plan review. The alternative cost was a plausible-looking
  wrong C producing a systematic Z-compensation bias — exactly the class of
  error that erodes the 2 cm alignment tolerance and manufactures
  `safe_for_control` false negatives.

## Source links

- `tracking-core/src/core/coordinate/coordinate_mapper.cpp` — constructor
  lines 54–113: normalisation, plane-pose extraction, sign resolution; the
  comment at the extraction site records why `decomposeHomographyMat` does not
  apply.
- `tracking-core/tests/cpp_unit/test_coordinate_mapper.cpp` —
  `RecoversCameraCentreFromHomography` (the 1e-6 fixture) and
  `Adr010WorkedExampleBiasIsCorrected` (the > 2 cm / ≤ 5 mm bias gate).
- `tracking-core/src/core/coordinate/homography_loader.cpp` — the single-sign
  guarantee over the working region that Step 1's one-point sign fix relies on.
- `/usr/include/opencv4/opencv2/calib3d.hpp` — the `decomposeHomographyMat`
  contract (two-view model, up to four solutions) as verified during review.
- ADR-010 — object-class Z compensation, the consumer of the recovered camera
  centre; TRK-018 is the implementing ticket, KTD-4 the plan decision record;
  TRK-013 defines the extrinsics artifact that stores H without a pose.
- Zhang, "A Flexible New Technique for Camera Calibration" (IEEE TPAMI 2000) —
  the origin of the [r1 r2 t] plane-pose extraction used here.
- Sibling, same pipeline and same day: [cv::Mat wrappers around Matx are copies — OutputArray results silently lost](2026-07-18-opencv-svd-outputarray-matx-silent-failure.md) — the orthonormalisation step here is a live application of that rule
- Same "check the model, not the name" family: [OpenCV version-portability seam](2026-07-18-opencv-aruco-version-portability-seam.md)
- Producer side of the artifact this consumes: [serialised artifact metadata must be derived from behaviour](../python/2026-07-18-artifact-metadata-derived-not-asserted.md)
