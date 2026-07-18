# CAM-002 PoC — Pi 3B → Pi 5 camera streaming with web viewer

Proof-of-concept slice (ticket CAM-002): the Pi 3B streams its CSI camera as
MJPEG over TCP; the Pi 5 relays it to a browser. De-risks CAM-001 and produces
transport evidence for the CAM-001a decision. **Not** the production streaming
node — no ZMQ, no capture timestamps, no auth.

```
Pi 3B (192.168.2.73)                Pi 5 (192.168.2.106)              Browser
rpicam-vid --codec mjpeg  --TCP-->  mjpeg_web_relay.py       --HTTP-->  <img>
--listen tcp://0.0.0.0:8554         latest-frame-wins relay :8080
```

## Run

**Pi 3B** (no installs needed — uses the stock `rpicam-vid`):

```bash
./start_stream_pi3b.sh                 # 1280x720 @ 15 fps on :8554
./start_stream_pi3b.sh 640 480 25      # lighter, if Wi-Fi chokes
```

**Pi 5** (stdlib Python only — no pip installs):

```bash
python3 mjpeg_web_relay.py             # source 192.168.2.73:8554, serves :8080
```

Then open **http://192.168.2.106:8080/**.

## Verify (scriptable)

```bash
curl -s http://192.168.2.106:8080/healthz          # {"connected": true, "fps": ...}
curl -s http://192.168.2.106:8080/snapshot.jpg | file -   # "JPEG image data"
```

Resilience: kill `start_stream_pi3b.sh`, watch `connected` go false, restart it —
the relay reconnects (1→10 s backoff) without a restart; the page recovers.

## Test without hardware

```bash
python3 -m unittest test_mjpeg_parser    # frame parser: chunk boundaries, resync
```

## Notes for CAM-001a (transport decision)

- MJPEG-over-TCP baseline, **measured 2026-07-15** (ov5647, 720p15 q80, real
  room scene): ~55 KB/frame × 15 fps ≈ **6.6 Mbit/s**; relay held 15–16.7 fps
  with frames < 50 ms fresh. Comfortable on wired LAN and typical Wi-Fi.
- Source-kill recovery: relay reconnects and the feed resumes in < 10 s with no
  relay restart (measured live).
- `rpicam-vid --listen` accepts one client and exits on disconnect; the start
  script loops to re-listen. Single-consumer only — the real node wants ZMQ PUB
  (ADR-002) so N consumers can subscribe.
- Latest-frame-wins in the relay mirrors the project's staleness philosophy
  (ADR-002 `ZMQ_CONFLATE=1`): a slow browser never builds a frame queue.
- No capture timestamps in this slice — CAM-001c/d territory.
