---
id: CAM-002
status: done
subsystem: camera-node
tier: small
created: 2026-07-15
updated: 2026-07-15
spec: null
plan: null
blockers: []
---
## Context

Proof-of-concept vertical slice: prove that the Pi 3B (192.168.2.73) can stream
its CSI camera to the Pi 5 (192.168.2.106) over the LAN, with the Pi 5 hosting a
web page that shows the live feed in a browser. This de-risks CAM-001 (the real
Phase 3 streaming node) and produces concrete transport evidence for the
CAM-001a decision (measured bandwidth/latency of MJPEG-over-TCP as the
baseline). Deliberately minimal: `rpicam-vid` MJPEG over TCP on the 3B (no new
installs on the constrained node), a stdlib-only Python relay + HTTP server on
the Pi 5 (per the python rule: no frameworks for small tools). Not production
CAM-001 — no ZMQ, no timestamps, no fusion.

## Acceptance

- A browser pointed at `http://192.168.2.106:8080/` shows live video from the
  Pi 3B's camera.
- `curl http://192.168.2.106:8080/snapshot.jpg` returns a valid JPEG (SOI/EOI
  markers) captured within the last second.
- `curl http://192.168.2.106:8080/healthz` reports `connected: true` and a
  non-zero frame rate.
- Relay survives the source dropping: kill the Pi 3B stream, restart it, and the
  page recovers without restarting the relay.
- Unit test covers the MJPEG frame parser (frame extraction across chunk
  boundaries) and passes locally without hardware.

## Plan

U1. `camera-node/poc/start_stream_pi3b.sh` — launch `rpicam-vid` MJPEG TCP
    listener on the 3B (configurable resolution/fps/port; PoC defaults).
U2. `camera-node/poc/mjpeg_web_relay.py` — stdlib Python: TCP client to the 3B
    with reconnect/backoff, MJPEG frame parser (latest-frame-wins), HTTP server
    with `/` (viewer page), `/stream` (multipart), `/snapshot.jpg`, `/healthz`.
U3. `camera-node/poc/test_mjpeg_parser.py` — unit test for the frame parser
    (chunk-boundary handling, garbage before SOI, truncated frames).
U4. `camera-node/poc/README.md` — how to run both ends + verification commands.
U5. Deploy: start stream on 3B, relay on Pi 5; verify snapshot/healthz/browser
    end-to-end; note observed bandwidth and latency for CAM-001a.

## Hardware gates (external)

- Pi 3B camera currently undetected (`libcamera interfaces=0`, no sensor in
  dmesg; worked 2026-07-15 pre-reboot) — likely unseated CSI ribbon after the
  PSU swap; needs physical reseat + reboot.
- Pi 5 SSH access not yet provisioned. Probing shows sshd offers **publickey
  only** (no password auth), so `ssh-copy-id` cannot bootstrap access. The dev
  machine's `~/.ssh/id_ed25519.pub` must be appended to
  `~/.ssh/authorized_keys` on the Pi 5 via existing access (console/monitor or
  an already-authorized machine). Tried users drone/pi/cpeddle/chris, direct
  and via the Pi 3B hop — all denied. Port 22 is open; 80/8080 closed.

## Log

- 2026-07-15: created. Status: backlog.
- 2026-07-15: backlog → in-progress. Starting PoC slice: rpicam-vid MJPEG over TCP from Pi3B, stdlib Python web relay on Pi5.
- 2026-07-15: U1–U4 complete and verified locally: parser unit tests 7/7; end-to-end smoke against a synthetic MJPEG TCP source — healthz connected/fps ≈ source rate, snapshot has valid SOI/EOI, /stream ≈ 20 parts/s, relay auto-reconnects after source kill/restart. U5 (hardware deploy) blocked on two external gates: Pi 3B camera undetected (suspect unseated CSI ribbon post-PSU-swap; needs physical reseat + reboot — remote sudo unavailable) and no SSH credentials for Pi 5 (publickey denied; needs ssh-copy-id from operator machine).
- 2026-07-15: in-progress → blocked. U1-U4 done + verified locally; deploy script ready and fail-fast tested. Blocked on operator: (1) reseat Pi3B CSI ribbon (camera undetected since post-PSU-swap reboot), (2) authorize the dev machine's id_ed25519 key on the Pi5 (sshd is publickey-only, so ssh-copy-id/password paths don't exist).
- 2026-07-15: blocked → done. PoC live and verified E2E: http://192.168.2.106:8080/ — 720p15 MJPEG, healthz 15-16.7 fps, frames <50ms fresh, /stream 33 parts/2s, snapshot 55KB real JPEG, source-kill recovery in <10s without relay restart. Measured ~6.6 Mbit/s (15fps x ~55KB) at 720p q80 — CAM-001a transport evidence. Camera gate resolved by reboot alone (boot-time detection glitch, ribbon fine); Pi5 access via authorized id_ed25519 for cpeddle@.
