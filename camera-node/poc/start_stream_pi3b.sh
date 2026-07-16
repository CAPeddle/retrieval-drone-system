#!/usr/bin/env bash
# start_stream_pi3b.sh — CAM-002 PoC: serve the Pi 3B CSI camera as MJPEG over TCP.
#
# Runs ON the Pi 3B. Uses rpicam-vid's built-in TCP listener; no extra installs.
# The Pi 5 relay (mjpeg_web_relay.py) connects as the TCP client. rpicam-vid
# exits when its client disconnects, so this loops to accept the next one.
#
# Usage:  ./start_stream_pi3b.sh [WIDTH] [HEIGHT] [FPS] [PORT]
# Defaults: 1280 720 15 8554  (720p15 MJPEG ~= 10-20 Mbit/s; fine on wired LAN,
# drop to 640x480 if the Wi-Fi path chokes).

set -u
WIDTH="${1:-1280}"
HEIGHT="${2:-720}"
FPS="${3:-15}"
PORT="${4:-8554}"

echo "[pi3b] MJPEG ${WIDTH}x${HEIGHT}@${FPS} on tcp://0.0.0.0:${PORT} (Ctrl-C to stop)"
while true; do
    rpicam-vid -t 0 -n --codec mjpeg \
        --width "$WIDTH" --height "$HEIGHT" --framerate "$FPS" --quality 80 \
        --listen -o "tcp://0.0.0.0:${PORT}"
    rc=$?
    echo "[pi3b] rpicam-vid exited (rc=$rc); restarting listener in 1s"
    sleep 1
done
