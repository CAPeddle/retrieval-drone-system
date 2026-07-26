---
id: TRK-034
status: backlog
subsystem: tracking-core
tier: small
created: 2026-07-26
updated: 2026-07-26
depends_on: []
spec: null
plan: null
blockers: []
---
## Context

First contact between the Pi 5 and a real camera. ADR-011 selects the Raspberry Pi Camera Module 3 Wide NoIR (IMX708) as the primary tracking camera, but the ADR is `Proposed` and stays that way until its gates G1–G6 are measured on target. This ticket is those measurements. It also re-derives every photometric threshold in `tracking_core.yaml`, all of which are OV5647 values captured on the Pi 3B rig — the config file itself says "Re-calibrate on camera/scene change". No C++ changes belong here; this is bring-up and measurement so that TRK-032 and TRK-033 are written against numbers rather than assumptions.

## Acceptance

- `rpicam-hello --list-cameras` on the Pi 5 reports IMX708 and lists a ~1536×864 mode; its reported frame-duration limits are recorded and admit both 60 fps and 120 fps (ADR-011 G1).
- Coverage verified **by photograph with a measured reference object on the floor**, at the ADR-010 mount pose (70–100 cm height, 30–60° tilt), showing the intended working area fully in frame. Trigonometry does not satisfy this criterion (ADR-011 G2).
- Active sensor readout time measured in the 1536×864 mode, and recorded against the ~8.3 ms figure ADR-011 D4 asserts (ADR-011 G4, first half).
- `frame_quality` thresholds re-derived on IMX708 footage and written back to `tracking_core.yaml` with a dated measurement comment matching the existing convention: `blur_threshold`, `underexposed_threshold`, `overexposed_threshold`, and the ball detector's `brightness_threshold` (ADR-011 G5).
- `tools/pi-power-check.sh` reports `get_throttled=0x0` on the Pi 5 with the camera streaming under load (ADR-011 G6).
- `AfMode=Manual` with a fixed `LensPosition` demonstrated to survive a reboot, and the chosen lens position recorded (R-05).
- At least one clip recorded into the replay library from the new camera, so TRK-032/033 have IMX708 material to develop against.

## Plan

1. **U1** — Fit the camera to Pi 5 `CAM0` using a 22-pin-to-15-pin cable. Confirm the 15-pin cable in the module box is *not* used; the Pi 5 port is 22-pin.
2. **U2** — `rpicam-hello --list-cameras`; record sensor name, every mode, and each mode's frame-duration limits verbatim into the Log.
3. **U3** — Mount at the ADR-010 pose. Photograph the working area with a tape measure or known-size marker laid on the floor. Record the covered area. If the working area does not fit, stop and report before continuing — this is the failure the current rig already exhibits.
4. **U4** — Set manual exposure, manual gain, `AfMode=Manual` and a fixed `LensPosition`. Reboot; confirm the settings are reapplied and the image is identically focused.
5. **U5** — Measure active readout time in the 1536×864 mode (frame-duration sweep, or sensor timing registers via `v4l2-ctl`). Record against ADR-011 D4's ~8.3 ms claim.
6. **U6** — Record clips into the replay library covering the frame_quality range: well-exposed, underexposed, overexposed, sharp, and defocused.
7. **U7** — Re-derive `blur_threshold`, `underexposed_threshold`, `overexposed_threshold`, `brightness_threshold` from U6 footage. Update `tracking_core.yaml` with dated measurement comments.
8. **U8** — `tools/pi-power-check.sh` under streaming load; record `get_throttled`.
9. **U9** — Write the measurements into ADR-011's gate table. If G1/G2/G5/G6 all pass, ADR-011 can move to `Accepted` once TRK-032 supplies G3 and TRK-009b supplies the second half of G4.

## Log

- 2026-07-26: created. Status: backlog. Blocked on hardware purchase (ADR-011 D1). Discharges ADR-011 gates G1, G2, G5, G6 and the first half of G4.
