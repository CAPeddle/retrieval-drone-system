# ADR-011: Primary Camera Hardware and libcamera Frame Acquisition

## Status
Proposed (2026-07-26) — acceptance gates in the "Acceptance gates" section below. This ADR is written before the hardware is purchased so the purchase is made against a recorded rationale; it becomes `Accepted` only when the gates are measured on target.

## Context

The Pi 5 has never had a camera attached. Every frame the project has captured came from the 5 MP OV5647 NoIR module on the **Pi 3B**, driven by `rpicam-vid` and consumed either as a recorded clip (TRK-031) or over the CAM-002 MJPEG relay. The tracking core has therefore never ingested a live frame on the machine it is meant to run on.

Four forces bear on the choice of a camera and the path that gets its frames into the core.

**1. The core cannot talk to a native CSI camera today.** `CameraSource` (`tracking-core/src/core/camera_source.cpp:8-12`) opens `cv::VideoCapture` with `CAP_V4L2`, falling back to `CAP_ANY`. On 64-bit Raspberry Pi OS a Bayer CSI sensor exposes no processed BGR `/dev/video0` — `/dev/video0` carries the raw sensor stream with no auto-exposure, black-level correction, or lens shading applied. Producing usable frames requires libcamera. `CameraSource` works for UVC devices and on the development machine, and that is the only reason it has ever worked.

**2. The pipeline is monochrome end to end.** `BallDetector::detect` (`tracking/ball_detector.cpp:35`), `CalibrationMarkerDetector` (`tracking/calibration_marker_detector.cpp:65,84`) and `FrameQuality::assess` (`frame_quality.cpp:42`) each begin with `cv::cvtColor(frame, gray_, cv::COLOR_BGR2GRAY)`. Nothing downstream reads a colour channel, and ADR-005 already rejects colour-based laser detection on the grounds that the NoIR sensor lacks usable colour information. The system currently pays for a debayer and then three greyscale conversions per frame, and consumes only the luminance.

**3. ADR-005 prefers an infrared laser, and names a shutter risk.** ADR-005 prefers IR over visible to eliminate the perceptible 15 Hz flicker, which makes an IR-transmitting sensor a hard requirement rather than a preference. ADR-005 also records the unresolved rolling-shutter interaction (risk **R-03**): at a 16.6 ms scan time and 15 Hz modulation, the top and bottom of one frame can capture different modulation states, degrading correlation near frame edges. ADR-005 leaves the resolution to detector implementation, naming per-row temporal alignment as one option. Neither wavelength (850 nm vs 940 nm) nor a specific laser part has ever been recorded.

**4. Deployment geometry is already fixed by ADR-010.** ADR-010 states the v0.3 camera is "tabletop-mounted, pointed downwards at floor", at roughly 70–100 cm height and 30–60° tilt, with the ball at up to ~2 m horizontal distance. This is an oblique view across a floor patch, not a nadir view, and it sets the field-of-view requirement. The current rig fails this outright — a 2026-07-19 probe found "the floor is not in view (camera at desk level); focus is soft".

One further constraint that this ADR corrects rather than accepts: `docs/tickets/CAM-001-pi3b-camera-streaming.md` justifies the Pi 3B sensor node with "CSI cable reach (15cm) prevents multi-camera on a single Pi 5". Raspberry Pi sells shielded 22-pin camera cables in 200, 300 and 500 mm lengths, and CSI-over-HDMI extenders reach several metres. 15 cm is the length of the cable in the box, not a property of CSI. The Pi 3B node is not required by cable reach.

## Decision

### D1 — Primary camera

The primary v0.3 tracking camera is the **Raspberry Pi Camera Module 3 Wide NoIR** (SC0875, Sony IMX708), mounted on the Pi 5 `CAM0` port via a 22-pin-to-15-pin Raspberry Pi camera cable. The Pi 5 CSI connector is 22-pin; the 15-pin cable supplied with the module does not fit it, so the cable is part of the purchase, not an accessory.

Wide (102° horizontal, 67° vertical) is chosen over Standard (66° horizontal) because the Standard variant cannot cover the ADR-010 deployment geometry.

### D2 — The Pi 3B is a recording rig, not a tracking source

The Pi 3B and its OV5647 are reclassified as a **recording and replay rig** supporting TRK-031. They are not a frame source for the tracking hot path. The measured CAM-002 baseline is 720p at 15 fps with frames under 50 ms fresh, against a ship gate of 60 fps and a 30 ms median end-to-end budget; a networked node cannot serve the hot path at v0.3 scope. CAM-001 remains a Phase 3 ticket for coverage extension, on its own merits.

### D3 — Frame acquisition via libcamera

A new `LibcameraSource` implements the existing `FrameSource` interface (`include/frame_source.hpp`) alongside `CameraSource` and `ReplaySource`. The `grab(cv::Mat&)` seam does not change. `CameraSource` is retained for UVC devices and for development machines without libcamera; the source is selected by a `camera.source_type` config field, and the libcamera dependency is optional at build time so the development-machine build stays green.

`LibcameraSource` configures, at startup and never thereafter:
- manual exposure and manual analogue gain, from config, per ADR-004's locked-exposure requirement;
- `AfMode = Manual` with a fixed `LensPosition` from config;
- an explicitly named sensor mode (see D4).

### D4 — Sensor mode 1536×864, delivered at 60 fps

The core selects the IMX708 **1536×864** sensor mode — the mode capable of 120 fps — and requests a **60 fps** frame delivery rate by extending vertical blanking.

This is deliberate and is the main reason for naming a mode rather than a resolution. Frame *delivery* stays at 60 fps, preserving the ship-gate rate and its 16.6 ms per-frame CPU budget, while the sensor's *active readout* still completes in roughly 8.3 ms. Rolling-shutter row skew is a property of readout time, not of frame period, so selecting the fast mode roughly halves the R-03 skew at no CPU cost and with no code.

120 fps delivery is a documented future promotion, permitted only once thermal and CPU headroom are measured, and it is the enabling condition for ADR-005's 22 Hz modulation promotion.

### D5 — Frames are single-channel greyscale

The `FrameSource` contract changes: frames are **`CV_8UC1` greyscale**, not `CV_8UC3` BGR. `LibcameraSource` requests a YUV420 stream and passes the **Y plane** through directly, without a colour conversion.

Consequently `BallDetector`, `CalibrationMarkerDetector` and `FrameQuality` drop their `cvtColor` call and their `gray_` scratch buffer and consume the input frame directly; `FrameRingBuffer` pre-allocates `CV_8UC1` slots; `ReplaySource` decodes recorded clips to greyscale so that replay and live capture present identical data to the detectors.

### D6 — Infrared wavelength is 850 nm, and the sensor choice leads it

The camera is the leading decision; the laser wavelength follows it. The IR laser referred to by ADR-005 and ADR-008 is specified as **850 nm**.

**Why infrared is effectively required, not merely preferred.** ADR-005 records the IR preference as a comfort argument ("15 Hz visible flicker is perceptible"). It is stronger than that, and the constraint is structural. ADR-005:36-42 caps modulation at `f_capture / 4` — 15 Hz at 60 fps, 30 Hz at the D4 promotion to 120 fps. Critical flicker fusion for a bright point source is roughly 50–60 Hz, and higher for a *moving* point in peripheral vision. Clearing fusion would require ≥240 fps capture, which this platform will not reach. **A visible modulated laser therefore cannot be made non-flickering on this hardware at any achievable setting.** Worse, 15 Hz sits near the peak of the photosensitive-epilepsy provocation band (roughly 3–60 Hz, peaking around 15–25 Hz). A 1 mW dot subtends a small visual angle so the absolute risk is far below full-field flashing, but with children present this is the specific hazard the IR preference exists to avoid. Visible-spectrum modulation is ruled out, not merely disfavoured.

**Why 850 nm rather than 940 nm.** The usual argument for 940 nm is the solar water-absorption dip: less ambient background. That benefit is only collectable behind a narrowband filter, and this ADR fits none (below). Without one, a broadband NoIR sensor integrates the same NIR background whatever the laser emits — **background is common-mode between the two candidates**, and the choice collapses to signal, i.e. sensor quantum efficiency, which favours 850 nm in silicon by a wide margin. Two factors point the other way and are accepted as outweighed: IEC 60825-1's wavelength correction factor `C₄ = 10^(0.002(λ−700))` puts the Class 1 limit roughly 1.5× higher at 940 nm, so more power is permissible there; and 940 nm modules are less commonly available as collimated laser diodes than as LED illuminators.

**Not a safety feature.** An earlier draft of this ADR argued that 850 nm's faint visible glow at the aperture is a safety benefit. That is withdrawn. Neither candidate wavelength triggers a blink reflex — which is exactly what makes IR more hazardous than visible light at equal power — so the glow protects nobody. It tells the *operator* the emitter is energised, which is an operability benefit only, and it may attract a child's eye toward the aperture.

**No IR band-pass filter is fitted.** The same camera must detect the ball in ambient light and the laser spot in IR; a band-pass filter would optimise the second at the cost of the first. If bring-up shows laser detection is background-limited rather than signal-limited, the remedy is not a different wavelength — it is a filtered second camera dedicated to the laser, which reopens this clause and pulls ADR-006's multi-camera scaffolding into scope. That is a significantly larger change than a wavelength swap and must not be made silently.

**Bench lasers are not the production laser.** Two visible 650 nm modules are owned and are **bench and alignment equipment only** — useful precisely because the beam is visible during aiming, servo characterisation (LASER-004) and debugging. They are not a fallback for the production laser and must not be modulated in an occupied room. See the reference file's hardware section for their handling constraints.

**Ball colour constraint.** The ball is non-red. This is recorded because it is load-bearing under one contingency: if the laser ever becomes visible-spectrum, red-channel chroma is the natural discriminator, and a red or orange ball would collide with it in exactly that channel. A non-red ball keeps that option open at zero cost.

### D7 — What this ADR does not decide

- **Multi-camera remains out of scope.** ADR-006 continues to govern; camera count in v0.3 is one. This ADR does not add a second camera and does not alter `FrameMetadata::camera_id` semantics.
- **R-03 is reduced, not retired.** D4 halves the row skew; the residual must be closed in software by per-row temporal alignment in the modulation detector (TRK-009), which is the option ADR-005 names. This ADR does not claim to close R-03.

## Acceptance gates

This ADR moves from `Proposed` to `Accepted` when all of the following are measured on the Pi 5 with the purchased hardware. Each is falsifiable; none may be satisfied by inspection or assumption.

| Gate | Criterion |
|---|---|
| G1 | `rpicam-hello --list-cameras` on the Pi 5 reports IMX708 and lists a ~1536×864 mode; its reported frame-duration limits admit both 60 fps and 120 fps. |
| G2 | At the ADR-010 mount pose (70–100 cm height, 30–60° tilt), the intended floor working area is fully within frame, verified by photograph with a measured reference on the floor — not by trigonometry. |
| G3 | Sustained ≥60 fps end to end with frame drop ≤0.5 %, median latency ≤30 ms, p99 ≤50 ms, core CPU ≤60 % of one core (CLAUDE.md §5). |
| G4 | Measured active readout time, and an R-03 position-dependence test: modulation correlation strength with the laser near the top of frame versus near the bottom, before and after per-row alignment. |
| G5 | `frame_quality` thresholds re-derived on IMX708 footage (the current values are measured OV5647 values, and `tracking_core.yaml` says to re-calibrate on camera change). |
| G6 | `tools/pi-power-check.sh` reports `get_throttled=0x0` on the Pi 5 with the camera streaming under load. |

If G4 shows a position-dependent correlation gap that per-row alignment does not close, the escalation is a global-shutter sensor — see Alternatives.

## Consequences

**Positive**
- The Pi 5 gains a working camera and the "Pi 5 + NoIR CSI camera" architecture stops being aspirational.
- D5 removes a debayer and three per-frame `cvtColor` calls from the hot path. The frame also carries one third of the bytes, which reduces ring-buffer footprint and memory bandwidth — both scarce on a Pi 5 sharing CPU with the flight-controller stack.
- D4 halves rolling-shutter row skew for free, and opens ADR-005's 22 Hz modulation promotion as a later option without another hardware purchase.
- Software-locked focus (`AfMode=Manual` + fixed `LensPosition`) is deterministic across reboots and immune to physical knocks. This is a genuine improvement on R-05's current mitigation, which is to physically lock a focus ring that a child can twist.
- NoIR is native. No irreversible modification to the camera is required to satisfy ADR-005's IR preference.
- libcamera restores access to auto-exposure metadata, lens shading and black-level correction that the raw V4L2 path discards — relevant to the ADR-004 calibration lifecycle.

**Negative**
- A new hot-path module (`LibcameraSource`) with its own buffer lifecycle and failure semantics, and a new build dependency that must stay optional so the development build survives without it.
- D5 is a breaking contract change across `FrameSource`, the ring buffer, all three detectors and the replay source. It must land as one coherent change, not incrementally.
- Every measured photometric threshold in `tracking_core.yaml` is an OV5647 value and must be re-derived. Existing OV5647 recordings remain valid as regression history but cannot satisfy the ADR-007 replay gate for the new sensor — the scenarios must be re-recorded.
- Per ADR-004 and ADR-010, per-unit intrinsics are required; debt D-03 (no per-camera intrinsics generated) must be discharged for this unit before the core can be trusted in coordinate mapping.
- The Wide lens is not free: at 102° horizontal, far-field pixels subtend more floor area than near-field pixels, so ADR-010's uncertainty propagation will report materially larger `uncertainty_m` at the far edge of frame than a Standard lens would. This is correct behaviour, not a defect, but it shrinks the usable working area at a given uncertainty budget.

**Risks**
- **Wide-angle distortion exceeds the calibration model.** At 102° horizontal, the standard five-parameter OpenCV distortion model may not fit adequately. *Mitigation:* TRK-012 must report reprojection error for this unit and the model be escalated to a rational or fisheye model if the residual is unacceptable. Do not assume the existing tool is adequate — measure it.
- **Frame-rate versus mode assumption is unverified.** D4 assumes IMX708 permits the 1536×864 mode at an extended frame duration while retaining its short readout. This is standard sensor behaviour but is asserted, not measured. *Mitigation:* gate G1, then G4.
- **Autofocus not actually disabled.** If `AfMode=Manual` is not applied on every start, a silent refocus invalidates intrinsics exactly as a bumped focus ring would (R-05). *Mitigation:* `LibcameraSource` must read back and log the applied lens position at startup, and the value belongs in `system_health` so the operator can compare it against the calibrated value.
- **850 nm choice is reasoned, not measured.** No IMX708 quantum-efficiency curve has been consulted at either candidate wavelength. *Mitigation:* the first bring-up recording with the laser fitted should compare spot signal-to-noise against background; if 850 nm underperforms, the wavelength decision is revisited before the modulation detector is tuned.
- **Ambient IR contamination.** With no IR-cut filter and no band-pass filter, sunlight and incandescent sources are strong broadband IR emitters and will appear as bright regions. This is precisely the failure mode CLAUDE.md warns about — "the brightest pixel is not the laser" — and it is why the ADR-005 modulation-correlation window is load-bearing. *Mitigation:* none new; this ADR must not be read as weakening ADR-005.

## Alternatives Considered

- **Raspberry Pi Global Shutter Camera (IMX296, 1456×1088 @ 60 fps, C/CS mount).** The technically strongest option: a global shutter retires R-03 outright rather than reducing it, and short exposures give strong ambient rejection that would also help R-04. Rejected as the first purchase because its Hoya CM500 IR filter can only be made IR-transmitting by permanent, warranty-voiding removal — an irreversible modification made before any IR signal has been measured; because it caps at 60 fps, foreclosing ADR-005's 22 Hz promotion; because it needs a ~2.8–3.6 mm CS lens bought separately to reach the required field of view, at roughly 40 % higher total cost. **Retained as the named escalation if gate G4 fails.**
- **Camera Module 3 Standard NoIR (66° horizontal).** Rejected on coverage: it cannot see the ADR-010 working area from the stated mount pose, which is the specific failure the current rig already exhibits.
- **Monochrome global-shutter sensor read as raw greyscale over V4L2 (e.g. OV9281).** Attractive: native mono matches D5 exactly, needs no ISP, has no IR filter to remove, and would have worked with the existing `CameraSource` unchanged. Rejected because 1280×800 at a 1/4" format is a small sensor for an oblique wide view, because the raw V4L2 path forfeits the lens-shading and black-level correction that ADR-004 calibration benefits from, and because it requires `media-ctl` pipeline configuration whose behaviour varies across kernel versions — an unattractive dependency for a system that must come up unattended.
- **A USB UVC global-shutter webcam.** Would work with the existing code with no driver work at all. Rejected because USB adds roughly 10–20 ms of latency against a 30 ms median budget, and forfeits the timing determinism of CSI that `CaptureThread`'s SCHED_FIFO design is built around.
- **Keep the OV5647 on the Pi 3B and stream to the Pi 5.** Rejected on measured evidence: the CAM-002 PoC reached 720p at 15 fps with ~50 ms frame freshness, against a 60 fps and 30 ms median requirement. It is also single-consumer, since `rpicam-vid --listen` accepts one client and exits on disconnect. Viable for coverage in Phase 3; not viable for the hot path.
- **Build no libcamera source and buy a UVC camera instead.** This is the same trade as the USB option and was rejected for the same reason, but it is recorded separately because it was the cheapest path to a working Pi 5 camera and its rejection is a deliberate acceptance of implementation cost in exchange for latency determinism.

## Related ADRs
- ADR-001 (Hybrid C++ Core with Python Tooling) — the 60–90 fps ingestion commitment this ADR must satisfy.
- ADR-004 (Hybrid Calibration Lifecycle) — locked manual exposure; per-unit intrinsics required for the new sensor.
- ADR-005 (Active Laser Modulation) — the IR preference D6 makes concrete, and the R-03 rolling-shutter risk D4 reduces.
- ADR-006 (FloorPlane2D World Frame) — unchanged; camera count in v0.3 remains one.
- ADR-010 (Object-Class Z Compensation) — supplies the deployment geometry that sets the field-of-view requirement, and the uncertainty propagation that the Wide lens makes more position-dependent.
