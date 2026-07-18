---
title: Serialised artifact metadata must be derived from behaviour, not asserted
captured: 2026-07-18
applies_to: [tracking-core]
tags: [calibration, extrinsics, artifact-contract, undistortion, metadata]
problem_type: pattern
source: internal
module: "tracking-core/tools (calibrate_extrinsics.py, TRK-013)"
component: tooling
severity: high
symptoms:
  - "calibrate_extrinsics.py hardcoded undistorted_coordinates true while --intrinsics (the only path invoking cv2.undistortPoints) is an optional argument"
  - "Default/tested path fitted the floor homography on raw distorted pixels yet the artifact labelled them undistorted"
  - "TRK-017's runtime consumer would undistort points before applying a homography fitted on distorted ones — systematic floor-coordinate bias feeding the safe_for_control chain (ADR-006/ADR-007)"
  - "Stored floor_anchor_points did not reproduce the stored floor coordinates under the stored homography (artifact not self-consistent)"
root_cause: logic_error
resolution_type: code_fix
---

# Serialised artifact metadata must be derived from behaviour, not asserted

## Summary

Any metadata field in a serialised artifact that describes what the producer
*did* (coordinate regime, units, applied transforms, input versions) must be
computed from the code path actually taken — ideally from the same expression
that selects the branch — never written as a literal expressing intent. Pair
that with self-consistency: store enough of the inputs that the stored
transform applied to the stored inputs reproduces the stored outputs, so a
consumer can verify the artifact without trusting its flags. Reach for this
whenever a tool emits an artifact that a separate (possibly not-yet-built)
consumer will obey.

## Context

TRK-013's `tracking-core/tools/calibrate_extrinsics.py` writes the extrinsics
JSON that TRK-017's runtime coordinate mapping will consume — a frozen
cross-ticket contract under ADR-006. The tool hardcoded
`"undistorted_coordinates": True` in the serialised artifact, but
`--intrinsics` is an *optional* argument: on the default path the homography is
fitted on raw distorted pixel centroids, yet the artifact labelled itself as
operating on undistorted pixels. The metadata lied about what the code had
done.

If shipped, the consequence is a silent systematic bias: the TRK-017 runtime,
obeying the flag, would undistort incoming pixels and then apply a homography
that was fitted on *distorted* ones. Every floor-plane coordinate would carry a
smooth, plausible error — no crash, no gate failure — feeding directly into
distance computations and ultimately `safe_for_control`, which sits in
ADR-007's zero-false-positive domain. Caught pre-merge by an adversarial code
reviewer (the phase's only P1 finding); the consumer is not yet built, so no
runtime bias was ever measured.

## What didn't catch it

- **Functional tests, all green throughout.** Homography recovery to
  sub-centimetre accuracy, rejection of too-few anchors, rejection of collinear
  layouts — every behavioural test passed on every commit, because none of them
  asserted the metadata contract. The bug was invisible to tests that only
  checked behaviour.
- **The schema-presence test.** The existing test iterated over the expected
  keys and asserted `key in data` — it confirmed `undistorted_coordinates`
  *existed* but never that its *value* was truthful. Key-presence checks verify
  the shape of a contract, not its honesty.

## Approach / Pattern

Fix in commit `c42da16`, three parts — derive, self-verify, pin both branches.

**Before** (`write_json` payload):

```python
        "reprojection_error_m": reprojection_error_m,
        # The homography operates on UNDISTORTED image pixels; the runtime
        # consumer (TRK-017) must undistort before applying it.
        "undistorted_coordinates": True,
```

**After** — the flag is derived in `run()` from the same predicate that selects
the branch, and threaded into `write_json`:

```python
    undistorted = args.intrinsics is not None
    image_pts = undistort(image_pts, Path(args.intrinsics) if undistorted else None)
```

```python
        "reprojection_error_m": reprojection_error_m,
        # Whether the homography operates on undistorted pixels — true only when
        # --intrinsics was supplied. The runtime consumer (TRK-017) must
        # undistort first iff this is true; a false value means the fit was on
        # raw distorted pixels and the contract differs.
        "undistorted_coordinates": undistorted,
```

Self-consistency — the stored anchors are the points the fit actually used, so
`H(stored anchors)` reproduces the stored floor coordinates within the stored
reprojection error, whichever regime produced them:

```python
    # Store the image points the homography was actually fitted on (undistorted
    # when intrinsics were supplied) so applying H to them reproduces the floor
    # coords — self-consistent with the undistorted_coordinates flag.
    anchors = [
        {
            "marker_id": int(marker_id),
            "floor_x_m": float(layout[marker_id][0]),
            "floor_y_m": float(layout[marker_id][1]),
            "image_x_px": float(image_pts[k][0]),
            "image_y_px": float(image_pts[k][1]),
        }
        for k, marker_id in enumerate(matched_ids)
    ]
```

Regression tests pin both branches of the only argument that changes the
contract (`tracking-core/tests/python_integration/test_calibrate_extrinsics.py`):

```python
    # No --intrinsics -> the fit is on raw pixels, so the flag must be False.
    assert data["undistorted_coordinates"] is False
```

and `test_intrinsics_path_sets_undistorted_flag` exercises the other branch
end-to-end with zero-distortion intrinsics, asserting the flag is `True` and
recovery still succeeds through the pass-through.

**Why this works.** The root cause was a contract field asserted as a constant:
the author wrote the *intended* contract into the artifact while the code had
an optional branch the intention did not cover. Once the flag is computed from
the same predicate that selects the branch, the metadata cannot drift from the
behaviour — there is a single source of truth for both. The self-consistent
anchors close the loop: a consumer (or a test, or a human with the JSON and
three lines of NumPy) can verify the artifact without trusting the flag — the
artifact carries its own evidence.

The generalised rules:

1. **Never assert artifact metadata as constants** — derive every
   describes-what-happened field from the code path actually taken.
2. **Make serialised artifacts self-consistent** — stored inputs must reproduce
   stored outputs under the stored transform, so a lying flag becomes
   detectable rather than silent.
3. **Test the metadata contract for every branch that changes behaviour** — one
   test with the flag off, one with it on, each asserting the *value*. A
   key-presence check is not a contract test.
4. **Frozen cross-ticket contracts deserve an adversarial review lens** — when
   the consumer does not exist yet, ask "could a consumer following this
   metadata to the letter be misled?"; the producer's tests cannot catch lies
   the consumer will believe.

## Trade-offs

- Deriving the flag makes the artifact honest but does not make the default
  path *good*: a homography fitted on distorted pixels is still lower-quality
  calibration. The honest `false` value pushes that decision to the consumer
  (or operator) instead of hiding it — an alternative considered was making
  `--intrinsics` mandatory, deferred until the bench workflow settles.
- Storing post-undistortion anchor pixels means the raw detections are not in
  the artifact; if raw-pixel provenance is ever needed, it would have to be
  added as a separate field rather than reinterpreting the existing one.

## Source links

- Fix: `tracking-core/tools/calibrate_extrinsics.py` (`run()` flag derivation, `write_json` payload, anchors comprehension) — commit `c42da16`
- Regression tests: `tracking-core/tests/python_integration/test_calibrate_extrinsics.py` (`test_recovers_homography`, `test_intrinsics_path_sets_undistorted_flag`)
- Contracts: ADR-006 (image → FloorPlane2D homography), ADR-007 (`safe_for_control` authority); consumer ticket TRK-017
- How it was caught: [multi-agent code review workflow](../workflow/2026-06-14-multi-agent-code-review-workflow.md) — adversarial reviewer, P1
- Same-batch sibling lesson: [composite test targets must model occlusion](../cpp/2026-07-18-composite-test-targets-model-occlusion.md) — both are "artifacts must reflect actual behaviour" failures caught by review
