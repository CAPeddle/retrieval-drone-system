#!/usr/bin/env bash
# pi-power-check.sh — verify a Raspberry Pi's power supply is adequate under
# camera load, by reading the firmware throttle flags before and after a camera
# run.
#
# `vcgencmd get_throttled` flags are LATCHED until reboot, so a meaningful test
# must: (1) reboot to clear history, (2) read a clean idle baseline, then (3)
# load the camera and watch for FRESH under-voltage. This script does all three
# over SSH. Key-based auth is required (BatchMode — it never prompts for a
# password); set it up once with `ssh-copy-id <user@host>`.
#
# Usage:
#   tools/pi-power-check.sh [user@host] [options]
#   tools/pi-power-check.sh drone@192.168.2.73
#
# Options:
#   --no-reboot        Skip the reboot (baseline flags may be stale/latched)
#   --no-camera        Baseline only; skip the camera load test
#   --camera-secs N    Camera run duration, seconds (default 8)
#   --camera-cmd CMD   Camera binary (default: rpicam-hello, else libcamera-hello)
#   -h | --help        Show this header
#
# Exit: 0 = healthy, 1 = under-voltage detected, 2 = could not reach/run.
#
# NOTE: safe by design — it only reboots and briefly opens the camera. It does
# NOT actuate the laser, drone, or servos (no real-hardware-actuation involved).

set -uo pipefail   # deliberately NOT -e: (( )) returning 0 must not exit.

HOST="drone@192.168.2.73"
DO_REBOOT=1
DO_CAMERA=1
CAM_SECS=8
CAM_CMD=""
SSH_OPTS=(-o BatchMode=yes -o ConnectTimeout=8 -o StrictHostKeyChecking=accept-new)

while [ $# -gt 0 ]; do
  case "$1" in
    --no-reboot)   DO_REBOOT=0 ;;
    --no-camera)   DO_CAMERA=0 ;;
    --camera-secs) CAM_SECS="${2:?}"; shift ;;
    --camera-cmd)  CAM_CMD="${2:?}"; shift ;;
    -h|--help)     sed -n '2,26p' "$0"; exit 0 ;;
    -*)            echo "unknown option: $1" >&2; exit 2 ;;
    *)             HOST="$1" ;;
  esac
  shift
done

say() { printf '%s\n' "$*"; }
hr()  { printf -- '------------------------------------------------------------\n'; }
run() { ssh "${SSH_OPTS[@]}" "$HOST" "$@"; }

# Decode a 0x-prefixed throttle bitmask into now/past human-readable flags.
decode_throttled() {
  local v=$(( ${1:-0} )) now="" past=""
  (( v & 0x1 ))     && now+=" under-voltage"
  (( v & 0x2 ))     && now+=" arm-freq-capped"
  (( v & 0x4 ))     && now+=" throttled"
  (( v & 0x8 ))     && now+=" soft-temp-limit"
  (( v & 0x10000 )) && past+=" under-voltage"
  (( v & 0x20000 )) && past+=" arm-freq-capped"
  (( v & 0x40000 )) && past+=" throttled"
  (( v & 0x80000 )) && past+=" soft-temp-limit"
  printf '  now : %s\n' "${now:- none}"
  printf '  past: %s\n' "${past:- none}"
}

say "Pi power check -> $HOST"
if ! run true 2>/dev/null; then
  say "ERROR: cannot SSH to $HOST (needs key auth + LAN reachability)."
  say "  Fix: 'ssh-copy-id $HOST' so 'ssh $HOST' works with no password, and"
  say "  run this from a machine on the same network as the Pi."
  exit 2
fi
say "Model: $(run 'tr -d "\0" </proc/device-tree/model 2>/dev/null' || echo unknown)"

REBOOTED=0
if [ "$DO_REBOOT" = 1 ]; then
  hr; say "Rebooting to clear latched throttle flags..."
  if run 'sudo -n reboot' 2>/dev/null; then
    REBOOTED=1
    say "  waiting for the Pi to come back..."
    sleep 8
    deadline=$(( SECONDS + 150 ))
    until run true 2>/dev/null; do
      [ "$SECONDS" -ge "$deadline" ] && { say "ERROR: Pi did not return within 150s."; exit 2; }
      sleep 4
    done
    say "  back up ($(run 'uptime -p' 2>/dev/null))"
  else
    say "  could not reboot non-interactively (sudo needs a password?)."
    say "  Reboot the Pi manually then re-run, or use --no-reboot (baseline may be latched)."
  fi
fi

hr; say "Baseline (idle):"
BASE=$(run 'vcgencmd get_throttled' 2>/dev/null | grep -o '0x[0-9a-fA-F]*'); BASE="${BASE:-0x0}"
say "throttled=$BASE"; decode_throttled "$BASE"
run 'vcgencmd measure_volts; vcgencmd measure_temp' 2>/dev/null | sed 's/^/  /'

CAM_FLAG=0
if [ "$DO_CAMERA" = 1 ]; then
  hr; say "Camera load test (${CAM_SECS}s)..."
  # Run the whole test remotely via a heredoc (positional args, no escaping).
  OUT=$(run bash -s -- "$CAM_SECS" "$CAM_CMD" <<'REMOTE'
secs="$1"; cam="$2"
if [ -z "$cam" ]; then
  if   command -v rpicam-hello    >/dev/null 2>&1; then cam="rpicam-hello"
  elif command -v libcamera-hello >/dev/null 2>&1; then cam="libcamera-hello"
  else echo "NOCAM"; exit 0; fi
fi
before=$(dmesg 2>/dev/null | grep -ic 'undervoltage detected')
"$cam" -t $(( secs * 1000 )) --nopreview >/dev/null 2>&1 &
pid=$!
worst=0
while kill -0 "$pid" 2>/dev/null; do
  cur=$(vcgencmd get_throttled | grep -o '0x[0-9a-fA-F]*')
  live=$(( ${cur:-0} & 0xF ))          # bits 0-3 = live under-voltage/throttle
  [ "$live" -gt "$worst" ] && worst=$live
  sleep 1
done
wait "$pid" 2>/dev/null
after=$(dmesg 2>/dev/null | grep -ic 'undervoltage detected')
echo "CAMERA=$cam"
echo "WORST_LIVE=$worst"
echo "NEW_UV_EVENTS=$(( after - before ))"
echo "FINAL=$(vcgencmd get_throttled | grep -o '0x[0-9a-fA-F]*')"
REMOTE
)
  say "$OUT" | sed 's/^/  /'
  if printf '%s' "$OUT" | grep -q '^NOCAM$'; then
    say "  (no rpicam-hello / libcamera-hello found — install it or pass --camera-cmd)"
  else
    worst=$(printf '%s\n' "$OUT" | sed -n 's/^WORST_LIVE=\([0-9]*\)$/\1/p')
    newuv=$(printf '%s\n' "$OUT" | sed -n 's/^NEW_UV_EVENTS=\(-*[0-9]*\)$/\1/p')
    FINAL=$(printf '%s\n' "$OUT" | sed -n 's/^FINAL=\(0x[0-9a-fA-F]*\)$/\1/p')
    [ -n "${FINAL:-}" ] && { say "  final throttled=$FINAL"; decode_throttled "$FINAL"; }
    { [ "${worst:-0}" -gt 0 ] || [ "${newuv:-0}" -gt 0 ]; } && CAM_FLAG=1
  fi
fi

hr
BASEV=$(( BASE ))
if [ "$CAM_FLAG" = 1 ]; then
  say "VERDICT: UNDER-VOLTAGE UNDER CAMERA LOAD  [FAIL]"
  say "  The supply still sags when the camera runs. Next step: swap to a SHORT, THICK"
  say "  power cable (the usual remaining cause), then re-run. If it persists on a"
  say "  known-good PSU + cable, suspect the CSI ribbon/camera or a damaged Pi input."
  exit 1
elif (( BASEV & 0x1 )) || { [ "$REBOOTED" = 1 ] && (( BASEV & 0x10000 )); }; then
  say "VERDICT: UNDER-VOLTAGE  [FAIL]  (seen at idle / since last boot)"
  exit 1
else
  say "VERDICT: HEALTHY  [PASS]  (throttled=$BASE, no under-voltage under camera load)"
  exit 0
fi
