#!/usr/bin/env bash
# record-scenarios-pi3b.sh — capture the TRK-031 replay scenario library.
#
# Records three 5 s clips (normal / underexposed / overexposed) from the
# Pi 3B's ov5647 with rpicam-vid, transcodes them to .avi containers on the
# Pi 5 (cv::VideoCapture-friendly), and stores them at
# ~/tracking-core-ci/recordings/ on the Pi 5 for the env-gated replay
# integration tests. Re-runnable; overwrites previous clips.
#
# Usage: tools/record-scenarios-pi3b.sh [pi3b-user@host] [pi5-user@host]
set -u
PI3B="${1:-drone@192.168.2.73}"
PI5="${2:-cpeddle@192.168.2.106}"
SSH=(ssh -o BatchMode=yes -o ConnectTimeout=8)

fail() { echo "FAIL: $*" >&2; exit 1; }

echo "[1/3] Record on the Pi 3B (3 x 5 s, 640x480@15)"
"${SSH[@]}" "$PI3B" '
set -u
rpicam-hello --list-cameras >/dev/null 2>&1 || { echo "no camera"; exit 1; }
mkdir -p /tmp/trk031
# Scene note: whatever the room looks like — the scenarios differ by exposure,
# not content. Shutter values chosen to force the quality-gate verdicts.
rpicam-vid -t 5000 -n --codec mjpeg --width 640 --height 480 --framerate 15 \
    -o /tmp/trk031/normal.mjpeg 2>/dev/null || exit 1
rpicam-vid -t 5000 -n --codec mjpeg --width 640 --height 480 --framerate 15 \
    --shutter 100 --gain 1 -o /tmp/trk031/underexposed.mjpeg 2>/dev/null || exit 1
rpicam-vid -t 5000 -n --codec mjpeg --width 640 --height 480 --framerate 15 \
    --shutter 60000 --gain 10 -o /tmp/trk031/overexposed.mjpeg 2>/dev/null || exit 1
ls -la /tmp/trk031/
' || fail "recording on $PI3B"

echo "[2/3] Transfer 3B -> (this box) -> Pi 5 and transcode to .avi"
# Relay via this machine: it holds keys for both Pis; the Pis need no keys
# for each other.
"${SSH[@]}" "$PI5" 'command -v ffmpeg >/dev/null || sudo -n apt-get install -y -qq ffmpeg >/dev/null 2>&1; command -v ffmpeg >/dev/null' \
    || fail "ffmpeg unavailable on $PI5 and could not be installed"
"${SSH[@]}" "$PI5" "mkdir -p ~/tracking-core-ci/recordings" || fail "mkdir on $PI5"
STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
scp -o BatchMode=yes -q "$PI3B":/tmp/trk031/'*.mjpeg' "$STAGE/" || fail "scp from $PI3B"
scp -o BatchMode=yes -q "$STAGE"/*.mjpeg "$PI5":/tmp/ || fail "scp to $PI5"
"${SSH[@]}" "$PI5" '
set -u
for clip in normal underexposed overexposed; do
    ffmpeg -y -loglevel error -r 15 -i /tmp/$clip.mjpeg -c copy \
        ~/tracking-core-ci/recordings/$clip.avi || exit 1
    rm -f /tmp/$clip.mjpeg
done
ls -la ~/tracking-core-ci/recordings/
' || fail "transcode on $PI5"

echo "[3/3] Done — recordings at $PI5:~/tracking-core-ci/recordings/"
echo "tools/pi5-remote-test.sh will now include the replay integration tests."
