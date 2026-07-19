---
title: "Replay-gate premises are scene assertions, not code invariants"
captured: 2026-07-19
applies_to: ["*"]
tags: [replay-gate, test-premise, scene-truth, false-positive, safety-gate]
problem_type: pattern
source: internal
applies_when: "A recorded-fixture gate fails (or has been suspiciously green); before treating it as a code regression, verify what was physically in the scene when the clips were captured"
---

# A recorded-fixture gate is only as true as its scene premise

## Summary

A test gate built on recorded footage encodes a physical-scene assertion — "this
clip contains no ball" — that no amount of code review can keep true, because
the truth lives in the room, not the repository. Worse, such a gate can stay
green for the wrong reason: a detector too blind to see the object keeps a
zero-false-positive gate passing even when the premise is false. When a zero-X
gate fails, verify the premise first (is the scene actually X-free?) with
spatial and content analysis before treating the failure as a code regression;
and record scene truth at recording time so the premise is a checkable fact
rather than a comment. Reach for this whenever a gate's assertion depends on
what was physically in front of the camera.

## Context

The tracking-core replay suite includes
`ReplayIntegration.NormalClipNoBallFalsePositives`, a gate asserting the
shipped ball detector fires **zero** times over the recorded `normal.avi`. The
premise is stated in the test's own comment: the detector "must not fire on a
ball-free, marker-free room scene". This sits in the spirit of the ADR-007
replay gate — zero false positives on the chain that feeds `safe_for_control`
is the cardinal-sin condition, so an exact-zero assertion on a supposedly
ball-free clip is exactly the kind of gate the project wants. The clip library
was recorded on 2026-07-17.

## What happened

On 2026-07-19 a measured threshold revision — `ball.brightness_threshold`
lowered from 200 to 130, the first value with real-footage provenance — made
the gate fail: 4 ball false positives in 64 quality-passed frames of the
"ball-free" clip.

The investigation (via a probe binary running the production detector; written
up separately) examined every flagged detection across the entire clip library:
91 in total, 87 on the new ball-in-scene clips and 4 on the "ball-free" clip.
All 91 landed within 40 px of one location — the physical ball at roughly
(490, 205) — with zero detections anywhere else in ~1,850 admitted frames. A
photograph of the scene confirmed the ball sits exactly there today; the
inference is that it was already in scene on 2026-07-17 when the "ball-free"
clips were recorded. So the gate's premise was false from the day the fixtures
were made — invisible at threshold 200, because the ball's peak brightness is
191 grey and the detector was simply blind to it, and exposed the moment the
detector could actually see.

The subtle part is that the premise rotted silently. The gate stayed green for
two days not because the scene was ball-free but because the detector could not
see the ball. A green gate is evidence about the premise only if the detector
is capable of falsifying it — here it was not, so the green run verified
nothing. The premise was never independently checked; the test comment asserted
it and everything downstream trusted the comment.

## What didn't catch it

- **Recording-session tooling.** It doesn't — and can't — verify scene
  contents. Nothing in the capture pipeline knows what "ball-free" means.
- **The gate itself.** Its comment *stated* the premise, but nothing *checked*
  it. A stated premise is documentation, not verification.
- **The recording script's framing.** The original clips were recorded for
  exposure scenarios; the script's own comment — "the scenarios differ by
  exposure, not content" — licensed content-blindness. Nobody was looking at
  what was in frame because the recordings were never about what was in frame.
- **The blind-detector interaction.** Any capability check on the detector
  (does it actually fire on a visible ball at this threshold?) would have
  revealed that a green zero-FP run at threshold 200 was uninformative. The
  True-Positive Counterweight concept existed in `CONCEPTS.md` but was not
  paired with this gate.

## Approach / Resolution

The resolution was explicitly user-approved on 2026-07-19, because it rewrites
a safety gate's assertion — premise corrections to safety gates are user
decisions, not something an agent self-serves.

The gate was re-scoped to the claim that is spatially true: **zero detections
away from the known scene-ball neighbourhood**. Constants `kSceneBallX`,
`kSceneBallY`, and `kSceneBallRadiusPx` pin the neighbourhood, with a
scene-truth comment dating the verification (spatial analysis + photograph,
2026-07-19). Detections at the ball's location are recorded via
`RecordProperty` as informational true positives rather than failures. The
exact-zero gate returns once the ball is physically removed and the library
re-recorded.

Two alternatives were rejected:

- **Reverting the threshold to 200** — restores green by restoring detector
  blindness, hiding the premise rot rather than fixing it.
- **Skipping the gate** — stops guarding the actual failure mode (spurious
  detections in empty regions) entirely.

**House-rule interplay.** The project holds both "never relax an assertion to
pass" and "when a test passes but reality is wrong, the test is wrong". The
distinction that resolves the tension: the assertion's *premise* (scene
contents) was factually false, established by independent evidence — spatial
clustering of all 91 detections plus a photograph — not by the failing test
alone; and the re-scoped assertion is *stronger* about the actual failure mode
(it still demands zero spurious detections, now with a detector that can see)
while being honest about the scene. That is a premise correction, not an
assertion relaxation. The correction is documented where the assertion lives
(the test comment) and in the commit message, so the next reader inherits the
evidence, not just the change.

The generalised rules:

1. **Treat clip premises as facts with a shelf life.** A recorded-fixture gate
   encodes a physical-scene assertion that no code review can keep true; the
   scene can drift (or, as here, have been wrong from day one) with zero
   repository diff.
2. **When a zero-X gate fails, verify the premise first.** Before treating the
   failure as a code regression, establish whether the scene is actually
   X-free — spatial clustering of the detections and a look at the frames are
   cheap and decisive.
3. **Pair every zero-FP gate with a capability check.** A blind detector keeps
   any false-positive gate green; the gate is only evidence if the detector
   could have falsified the premise. This is the True-Positive Counterweight
   already named in `CONCEPTS.md` — apply it, don't just cite it.
4. **Record scene truth at recording time.** A one-line manifest per clip —
   what is in frame and where — would have made this a non-event: the gate
   could have asserted against recorded fact instead of remembered intent.
5. **Premise changes to safety gates go through the user**, documented where
   the assertion lives and in the commit message.

## Trade-offs

- The re-scoped gate is weaker than exact-zero in one specific way: a genuine
  false positive that happens to land within 40 px of the scene ball would be
  recorded as informational rather than failing. The spatial analysis (all 91
  detections at one location, none elsewhere in ~1,850 frames) makes that
  corner small, and the exact-zero gate returns with the re-recorded library —
  the relaxation is scoped and temporary, with a stated exit condition.
- The `kSceneBall*` constants hard-code today's scene truth into the test. If
  the ball moves before re-recording, the constants silently mis-describe the
  scene again — the same class of rot, one level down. The dated scene-truth
  comment is the mitigation, not a fix; the real fix is re-recording with a
  manifest.
- Keeping the gate running (rather than skipping it) preserves guarding of the
  actual failure mode during the interim, at the cost of a more complicated
  assertion that future readers must understand before touching.

## Source links

- Gate: `ReplayIntegration.NormalClipNoBallFalsePositives` (tracking-core replay suite) — re-scoped assertion, `kSceneBallX`/`kSceneBallY`/`kSceneBallRadiusPx`, scene-truth comment dated 2026-07-19
- Trigger: `ball.brightness_threshold` 200 → 130, first real-footage-derived value
- Investigation: probe binary running the production detector over the full clip library (documented separately) — 91/91 detections within 40 px of (490, 205), zero elsewhere in ~1,850 admitted frames; scene photograph confirming ball position
- Contracts: ADR-007 (`safe_for_control` authority, zero-false-positive replay gate); CLAUDE.md §5 ("never relax assertions to pass" / "when a test passes but reality is wrong, the test is wrong")
- Vocabulary: True-Positive Counterweight — `CONCEPTS.md`
- Sibling in spirit: [artifact metadata must be derived, not asserted](../python/2026-07-18-artifact-metadata-derived-not-asserted.md) — both are stated-premise-vs-actual-truth failures; there the artifact lied about the code path, here the fixture lied about the scene
- Same incident, oracle-side lesson: [a detector replica is not the oracle](../cpp/2026-07-19-production-binary-oracle-for-threshold-provenance.md)
- Premise-honesty sibling: [serialised artifact metadata must be derived from behaviour](../python/2026-07-18-artifact-metadata-derived-not-asserted.md) — asserted-not-verified premises, producer side
- Test-realism sibling: [composite test targets must model occlusion](../cpp/2026-07-18-composite-test-targets-model-occlusion.md)
