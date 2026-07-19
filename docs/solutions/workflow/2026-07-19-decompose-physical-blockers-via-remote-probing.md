---
title: "Decompose \"requires physical hardware\" blockers via remote probing"
captured: 2026-07-19
applies_to: ["*"]
tags: [remote-development, hardware, feasibility, probing, replay-verification]
problem_type: pattern
source: internal
applies_when: "A task is marked blocked on physical hardware access while working remotely; before accepting the blocker, split it into information acquisition (usually remote) and matter rearrangement (truly physical)"
---

# "Physically blocked" tasks decompose: probe for information remotely, bound the true physical core

## Summary

A task labelled "physically blocked" is almost never one blocker; it is a
bundle of *information acquisition* (usually automatable over the network) and
*matter rearrangement* (truly physical). Do that split explicitly — with cheap
probes, before accepting the label — and most of the bundle dissolves. On this
project, "camera calibration session, live 60 fps verification, and the
ship-gate recording library all need someone in the room" collapsed, over one
remote session, to a precisely bounded physical residue: turn a focus screw,
wave a calibration board through out-of-plane poses, tape and measure floor
markers, move a ball, plug in one HDMI cable. Everything else — a 5 h 18 m
60 fps soak with clean SIGINT shutdown, cross-host ZMQ schema validation, a
3×120 s ship-gate recording library, and loudly-labelled synthetic calibration
artifacts to unblock the pipeline — was driven entirely over Tailscale and SSH.
Reach for this pattern whenever a task is parked as "needs hands on hardware",
"needs someone on site", or "waiting for physical access": run the probing
sequence first and see how small the physical core actually is.

## Context

The v0.3 slice of tracking-core was complete except for three items the plan
had labelled operator-physical: a camera calibration session, live-camera
60 fps verification with a SIGINT smoke test, and the ADR-007 ship-gate
recording library. The session constraint was hard: remote only, via Tailscale
to the dev box (Z2) and the Pi 5, with the camera on a Pi 3B. The naive
reading — "all three wait until the operator is at the bench" — would have
stalled the slice. The actual reading, established by four probes costing
minutes each, was that nearly all of the work was information acquisition in
disguise.

## Approach / Pattern

The method is a probing sequence. Each probe is cheap (minutes), evidence-based
(a command output or a photograph, not an assumption), and either unlocks
remote work or kills an idea before it consumes hours. Run them in roughly
this order.

**Probe 1 — mine your own tooling for topology.** The repo's operational
scripts encode network facts nobody wrote down as documentation. Reading
`tools/record-scenarios-pi3b.sh` revealed both the Pi 3B's address and the
relay pattern: the dev box holds SSH keys for both Pis, so the Pis need no
keys for each other. The camera — the supposed heart of the physical blocker —
was fully drivable remotely all along. Before assuming a device is
unreachable, grep the scripts that already talk to it.

**Probe 2 — look at the scene.** One `rpicam-still` capture, pulled back and
visually inspected, established scene truth in a single step: the tracking
ball *is* in frame; there are no ArUco markers anywhere; the floor is not in
view (camera at desk level); focus is soft. That single photograph redirected
the whole plan — it promoted ball-in-scene recordings to "do now, remotely"
and demoted extrinsics to "physical, later". One photograph of the actual
scene beats any amount of assumed scene state. Capture and *look*.

**Probe 3 — enumerate device capabilities.** `rpicam-hello --list-cameras`
showed the ov5647's 640×480 mode runs at 62.5 fps — above the 60 fps target.
The live-verification goal was therefore plausible on this sensor, a fact
worth one command rather than a site visit. Enumerate what the hardware can
do before declaring a target unreachable.

**Probe 4 — kill clever ideas cheaply, with evidence.** The tempting idea:
display ArUco markers on a monitor, machine-measure their positions from the
screen's pixel pitch via `xrandr`'s physical-mm output — extrinsics with no
tape measure. Two probes killed it: Z2's `xrandr` showed all four outputs
disconnected; the Pi 5's DRM connectors likewise. No reachable machine drives
a visible screen. The idea was correctly abandoned in minutes, not after a
half-built implementation. A negative connector scan is a *result*: it
converts "maybe" into "no, and here is why", and it recorded the exact revival
condition (one HDMI cable).

**Then execute the unblocked remainder.** With the split made, everything on
the information side ran remotely:

- `--replay` mode on the real threaded binary: a 5 h 18 m soak at a steady
  60–61 fps on the Pi (18,868 one-second reports against a 30 s requirement),
  a clean 200 ms SIGINT shutdown, and the ZMQ stream validated cross-host at
  ~60 Hz with zero schema violations.
- Full recording sessions driven over SSH: the 3×120 s ball-in-scene
  ship-gate library.
- Synthetic bench calibration artifacts to satisfy the fail-fast loader for
  pipeline runs — loudly self-labelling (`camera_id: "bench-synthetic"`) so
  they can never masquerade as real calibration.

Generalised rules:

1. **Decompose before accepting the blocker.** Split every "physically
   blocked" task into information acquisition and matter rearrangement,
   explicitly, before parking it.
2. **The repo's operational tooling encodes topology.** Hosts, keys, and
   relay patterns live in the scripts that already do the job — read them
   before assuming unreachability.
3. **One photograph beats assumed scene state.** Capture and look; do not
   plan against a remembered or imagined scene.
4. **Enumerate capabilities before declaring targets unreachable** — the
   62.5 fps mode was one `--list-cameras` away.
5. **Spend probes to kill clever ideas early.** A minutes-long negative scan
   beats an abandoned half-build, and it documents the revival condition.
6. **Synthetic stand-ins must be loudly self-labelling.** If a fake artifact
   unblocks a pipeline, its identity must scream fake at every layer that
   might later trust it.

## What stayed physical — and why

The residue is small and each item has a specific physical reason — worth
recording so nobody re-probes them:

- **Aiming and focusing the camera.** The ov5647's focus is a physical screw;
  no software path exists. (The soft focus from Probe 2 sits behind this.)
- **Per-unit intrinsics.** Calibration requires varied *out-of-plane* board
  poses. A monitor is degenerate here even if one were connected: every
  on-screen view is coplanar, leaving focal length unobservable — the exact
  degeneracy the repo's own calibration fixtures document. This one is
  physics, not connectivity.
- **Placing and hand-measuring floor markers** for deployment extrinsics.
  Markers are matter; positioning and measuring them rearranges the room.
- **Moving the ball.** Without hands in the scene, all footage is
  static-ball-only, which caps the variety of the recording library.
- **One HDMI cable.** The single item that would shrink the residue further:
  plugging any reachable machine into a monitor revives the machine-measured
  on-screen-marker extrinsics trick (extrinsics only — the coplanarity
  degeneracy still rules it out for intrinsics).

The point of the bounded list: the next "operator at the bench" session has a
complete, minimal shopping list, and everything not on it should never again
be deferred to physical presence.

## Trade-offs

- **Probing costs session time up front** against a task the plan says is
  blocked anyway. The costs here were minutes per probe against hours of
  unlocked remote work; the ratio will not always be that favourable, but the
  probe cost is bounded and the blocker cost is not.
- **Synthetic calibration artifacts create a contamination risk.** A
  bench-synthetic file that leaks into a real deployment would corrupt every
  downstream coordinate. The mitigation is the loud self-labelling plus the
  fail-fast loader — the stand-in is safe *because* it cannot be mistaken for
  real data, not because anyone promises to be careful.
- **Remote-driven recordings are static-ball-only**, so the ship-gate library
  built this way under-covers motion scenarios. It satisfies the letter of
  the gate (≥3 scenarios, ≥5 min) but the motion-rich recordings still land
  on the physical list.
- **The killed monitor-extrinsics idea is conditionally dead, not wrong.**
  Recording the revival condition (one HDMI cable) preserves the option
  without leaving a zombie task open.

## Source links

- Topology discovery: `tools/record-scenarios-pi3b.sh` (Pi 3B address; dev-box-as-relay SSH key pattern)
- Scene truth: single `rpicam-still` capture from the Pi 3B, visually inspected (ball in frame, no markers, no floor, soft focus)
- Capability enumeration: `rpicam-hello --list-cameras` on the Pi 3B (ov5647 640×480 @ 62.5 fps)
- Idea-kill evidence: `xrandr` on Z2 (all four outputs disconnected); Pi 5 DRM connector status (likewise)
- Remote execution results: `--replay` soak on the Pi 5 — 5 h 18 m at 60–61 fps, 18,868 one-second reports, 200 ms SIGINT shutdown; cross-host ZMQ validation at ~60 Hz, zero schema violations; 3×120 s ship-gate recordings over SSH
- Synthetic stand-in: bench calibration artifacts with `camera_id: "bench-synthetic"` satisfying the fail-fast loader
- Coplanarity degeneracy for intrinsics: documented in the repo's own calibration fixtures (out-of-plane pose requirement)
- Related: [stacked-PR merge-order recovery](2026-07-18-stacked-pr-merge-order-recovery.md) — another case of converting an assumed dead-end into a bounded, evidence-based procedure
- Related: [Pi 3B camera node under-voltage recovery](../hardware/pi3b-camera-node-undervoltage.md) — the same remote-probing posture applied to a hardware fault
- Related: [tracking-core rename/build-overhaul handoff](2026-06-04-tracking-core-rename-and-build-overhaul-handoff.md) — the blocked-with-precise-blocker discipline this pattern extends
- Related: [composite test targets must model occlusion](../cpp/2026-07-18-composite-test-targets-model-occlusion.md) — why the On-Target Run remains the evidence anchor the remote work targets
