#!/usr/bin/env bash
# deploy_and_verify.sh — CAM-002 U5: one-shot deploy + end-to-end verification.
#
# Run from anywhere with key-auth SSH to both Pis:
#   camera-node/poc/deploy_and_verify.sh [pi5-user@host] [pi3b-user@host]
# Defaults: drone@192.168.2.106  drone@192.168.2.73
#
# Steps: check the 3B camera is detected -> start the 3B MJPEG listener ->
# copy + start the relay on the Pi 5 -> curl-verify healthz and snapshot ->
# print the viewer URL. Exit 0 only when the service is live and showing frames.

set -u
PI5="${1:-drone@192.168.2.106}"
PI3B="${2:-drone@192.168.2.73}"
PI5_HOST="${PI5#*@}"
PI3B_HOST="${PI3B#*@}"
STREAM_PORT=8554
WEB_PORT=8080
HERE="$(cd "$(dirname "$0")" && pwd)"
SSH=(ssh -o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=accept-new)

fail() { echo "FAIL: $*" >&2; exit 1; }

echo "[1/5] Pi 3B camera detected?"
"${SSH[@]}" "$PI3B" 'rpicam-hello --list-cameras 2>&1 | head -3' | tee /tmp/cam002_cams.txt
grep -q "No cameras available" /tmp/cam002_cams.txt && \
  fail "Pi 3B still reports no camera — reseat the CSI ribbon (both ends, power off first), reboot, re-run."

echo "[2/5] Start MJPEG listener on the Pi 3B (:${STREAM_PORT})"
scp -q "$HERE/start_stream_pi3b.sh" "$PI3B:/tmp/start_stream_pi3b.sh" || fail "scp to Pi 3B"
# pkill lives in its OWN ssh call: a pkill -f pattern must never share a command
# string with the bare name it targets, or it matches the remote shell itself.
"${SSH[@]}" "$PI3B" "pkill -f '[r]picam-vid' 2>/dev/null; pkill -f '[s]tart_stream' 2>/dev/null; true"
"${SSH[@]}" "$PI3B" "
  chmod +x /tmp/start_stream_pi3b.sh
  nohup /tmp/start_stream_pi3b.sh 1280 720 15 ${STREAM_PORT} >/tmp/cam002_stream.log 2>&1 &
  sleep 2; ss -ltn | grep -q ':${STREAM_PORT} ' && echo 'stream: listening' || { cat /tmp/cam002_stream.log; exit 1; }
" || fail "could not start rpicam-vid on the Pi 3B"

echo "[3/5] Deploy relay to the Pi 5"
"${SSH[@]}" "$PI5" 'mkdir -p ~/cam002' || fail "ssh to Pi 5 ($PI5) — is the key authorized?"
scp -q "$HERE/mjpeg_web_relay.py" "$PI5:~/cam002/" || fail "scp to Pi 5"

echo "[4/5] Start relay on the Pi 5 (:${WEB_PORT})"
# Same isolation rule as step 2: the pkill call must not contain the bare filename.
"${SSH[@]}" "$PI5" "pkill -f '[m]jpeg_web_relay' 2>/dev/null; sleep 0.5; true"
"${SSH[@]}" "$PI5" "
  nohup python3 ~/cam002/mjpeg_web_relay.py \
      --source-host ${PI3B_HOST} --source-port ${STREAM_PORT} --port ${WEB_PORT} \
      >~/cam002/relay.log 2>&1 &
  sleep 2; ss -ltn | grep -q ':${WEB_PORT} ' && echo 'relay: listening' || { cat ~/cam002/relay.log; exit 1; }
" || fail "could not start the relay on the Pi 5"

echo "[5/5] End-to-end verification"
sleep 4
HEALTH=$(curl -sf -m 5 "http://${PI5_HOST}:${WEB_PORT}/healthz") || fail "healthz unreachable"
echo "healthz: $HEALTH"
echo "$HEALTH" | grep -q '"connected": true' || fail "relay is up but not receiving from the Pi 3B (see ~/cam002/relay.log on the Pi 5)"
curl -sf -m 5 "http://${PI5_HOST}:${WEB_PORT}/snapshot.jpg" -o /tmp/cam002_snap.jpg || fail "snapshot fetch"
head -c2 /tmp/cam002_snap.jpg | od -An -tx1 | grep -q "ff d8" || fail "snapshot is not a JPEG"
echo
echo "PASS — live camera feed: http://${PI5_HOST}:${WEB_PORT}/"
echo "snapshot saved to /tmp/cam002_snap.jpg ($(stat -c%s /tmp/cam002_snap.jpg) bytes)"
