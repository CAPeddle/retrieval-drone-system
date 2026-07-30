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
9. **U9** — **Spot-detection sanity check with the bench laser, before the IR laser is bought.** 650 nm is well within IMX708 response, so the owned LASERFUCHS module (1 mW, Class 2, collimated — *not* the KY-008) can validate the whole detection path on the new camera at zero cost and zero lead time: is the spot cleanly separable from background at the working distance and locked exposure? Static only — do not modulate a visible laser in an occupied room (ADR-011 D6).
10. **U10** — **Settle the 850 vs 940 nm question empirically**, discharging the ADR-011 D6 risk that the wavelength is reasoned rather than measured. An 850 nm and a 940 nm LED cost a few pounds between them and are sufficient; no laser purchase is needed to decide this. Fixed exposure, fixed gain, all auto disabled, same distance and geometry; metric is mean spot pixel value minus dark background. Radiometric power matching is the weak point — cheap-LED datasheets are good to perhaps ±30 % — so read the result as a band, not a number: a ratio ≥2× in favour of 850 nm survives that uncertainty and confirms D6, while anything under ~1.5× is inconclusive and means the wavelength barely matters, at which point pick on module availability instead.
11. **U11** — **Establish whether detection is signal-limited or background-limited**, under both realistic worst cases: daylight through a window, and evening room lighting. Incandescent and halogen lamps are strong NIR emitters; LED room lighting emits almost none, and that difference dwarfs 850-vs-940. Vary source power and check whether spot SNR scales or saturates. **If it saturates, this is an architecture finding, not a tuning result** — the remedy is a filtered laser-dedicated camera, which reopens ADR-011 D6 and pulls ADR-006 multi-camera scaffolding into scope. Stop and raise it rather than absorbing it.
12. **U12** — Write the measurements into ADR-011's gate table. If G1/G2/G5/G6 all pass, ADR-011 can move to `Accepted` once TRK-032 supplies G3 and TRK-009b supplies the second half of G4.

## Log

- 2026-07-26: created. Status: backlog. Blocked on hardware purchase (ADR-011 D1). Discharges ADR-011 gates G1, G2, G5, G6 and the first half of G4.
- 2026-07-30: **hardware acquired and fitted by the operator; U1 complete, U2 measured. The purchase blocker is discharged** (status left `backlog` — transition batched with the TRK-009 Phase C board close-out). Camera enumerates on the Pi 5 as `imx708_wide_noir` at `/base/axi/pcie@1000120000/rp1/i2c@88000/imx708@1a`, module ID `0x0382`, bound by `rp1-cfe` as `/dev/video0`. Sensor name confirms the NoIR **Wide** variant that ADR-011 D1 specifies.

  **U2 — `rpicam-hello --list-cameras`, verbatim:**
  ```
  0 : imx708_wide_noir [4608x2592 10-bit RGGB] (/base/axi/pcie@1000120000/rp1/i2c@88000/imx708@1a)
      Modes: 'SRGGB10_CSI2P' : 1536x864 [120.13 fps - (768, 432)/3072x1728 crop]
                               2304x1296 [56.03 fps - (0, 0)/4608x2592 crop]
                               4608x2592 [14.35 fps - (0, 0)/4608x2592 crop]
  ```

  **G1 verdict: PASSES on frame rate, but the measurement surfaces a conflict with ADR-011 D4 that G1's wording did not anticipate.** The 1536×864 mode exists and admits both 60 and 120 fps (120.13 fps ceiling) as G1 requires. However the mode is a **centre crop, not the full array**: its crop rectangle is `3072x1728` taken from the `4608x2592` array (offset `(768,432)`; 768+3072 = 3840 and 4608−3840 = 768, so exactly centred), i.e. **2/3 of the sensor linearly**. The 102° H / 67° V figures in ADR-011 D2 are full-array numbers.

  Derived — *not* measured, and superseded by the G2 photograph — the effective field of view in D4's chosen mode is `2·atan(0.6667·tan(51°)) ≈ 79° H` and `2·atan(0.6667·tan(33.5°)) ≈ 48° V`, against the 102°×67° D2 assumed. **D2 rejected CM3 Standard at 66° H on coverage; 79° sits nearer that rejected figure than the assumed 102°.** Only `2304x1296` reads the full array, and it caps at **56.03 fps — below the v0.3 ≥60 fps ship gate**. No mode offers full field of view at 60 fps. Raised for decision rather than resolved here; see the ADR-011 D4 note. **U3 (the G2 coverage photograph) is now the gating measurement** and must be shot in the 1536×864 mode, since that is the mode whose coverage is in doubt.
