---
title: Pi 3B Camera Node Under-Voltage on a Marginal Power Supply
captured: 2026-07-15
applies_to: [camera-node]
tags: [raspberry-pi, power, under-voltage, camera, hardware]
problem_type: recovery
source: internal
---

# Pi 3B Camera Node Under-Voltage on a Marginal Power Supply

## Problem

The Pi 3B intended as the CAM-001 camera streaming node showed "power problems
when the camera is attached" — instability that appeared once the CSI camera was
connected. Root cause: the 5 V rail transiently sagged below the Pi's
under-voltage threshold (~4.63 V) under load. The camera's init/stream current
(~200–260 mA on top of the Pi's ~0.7–1 A) was the last straw on a marginal
supply — the camera was the trigger, not the fault.

## Symptoms

- Instability / brown-outs / resets specifically after attaching the camera.
- Red PWR LED blinking (solid = healthy).
- `vcgencmd get_throttled` returns non-zero. Observed `throttled=0x50000` here.
- `dmesg` shows cycling `Undervoltage detected!` / `Voltage normalised`.

## Diagnosis

`vcgencmd get_throttled` is the definitive signal — a hex bitmask. **The flags
are latched until reboot**, so read them, reboot to clear, then re-test under
load. Bits:

| Bit | Value | Meaning |
|-----|-------|---------|
| 0 | `0x1` | under-voltage **now** |
| 2 | `0x4` | currently throttled |
| 16 | `0x10000` | under-voltage **has occurred** (since boot) |
| 18 | `0x40000` | throttling has occurred (since boot) |

`0x0` = healthy. `0x50000` (bits 16+18) = it under-volted and throttled at some
point since boot — the confirming reading here. Corroborate with
`dmesg | grep -i voltage` (look for `Undervoltage detected!`).

The live test that pins the camera as the trigger: on the Pi run
`watch -n1 vcgencmd get_throttled` (or `sudo dmesg -wH | grep -i volt`), then
start the camera (`rpicam-hello -t 0`). If the bits light up the instant the
camera streams, it is supply, not software.

## Solution

Replace the power delivery, in priority order:

1. **Power supply** — a genuine **5 V / 2.5 A** (Pi 4/5: 3 A / USB-C PD). Phone
   chargers and 5 V/2 A bricks are the usual offenders.
2. **Cable** — the single most common cause: a thin or long micro-USB/USB-C cable
   drops 0.3–0.5 V under load. Use a **short, thick** one. This alone fixes most
   cases even with an adequate PSU.
3. **Shed load** if the supply is fixed/marginal: power peripherals from a
   powered hub, not the Pi; for a headless camera node, `vcgencmd display_power 0`
   and disable Bluetooth to cut baseline draw.

In this case swapping the PSU resolved it — verified `throttled=0x0` with **no**
new under-voltage under a 10 s camera run (see below).

## Verify / Prevent

Use the repo's diagnostic script — it reboots (to clear latched flags), reads a
clean baseline, runs the camera while sampling the throttle flags, and prints a
PASS/FAIL verdict with a clean exit code:

```bash
tools/pi-power-check.sh drone@192.168.2.73            # full: reboot + baseline + camera test
tools/pi-power-check.sh drone@192.168.2.73 --no-reboot  # test on the current boot only
```

Exit `0` = healthy, `1` = under-voltage, `2` = unreachable. Requires SSH key auth
(`ssh-copy-id` once). Good candidate for a periodic health check on the node.

Prevention notes:
- Spec the camera node with a **2.5 A+ supply and a short cable** from the start.
- Never assume a "working" Pi supply is fine once you add a camera or other draw —
  re-verify under the new load.
- A one-off clean `get_throttled` is not proof: read it *after* a reboot and
  *under* the real workload, because the flags are latched and the sag is transient.

## References

- Ticket: CAM-001 (Pi 3B camera streaming node) — power requirement now noted there.
- Script: `tools/pi-power-check.sh`.
- Hardware context: `docs/agent-reference/agent-reference.md` (Hardware & environment).
