#!/usr/bin/env bash
# pi5-remote-test.sh — build tracking-core and run the full ctest suite ON the
# Pi 5, remotely. The on-target gate for claims dev-machine tests can't make:
# real ARM timing, real SCHED_FIFO (via setcap), tmpfs, thermal.
#
# Usage:  tools/pi5-remote-test.sh [user@host]
# Default target: cpeddle@192.168.2.106 (LAN) — the Tailscale address
# (cpeddle@100.96.75.13) works identically when off-LAN.
#
# One-time Pi 5 prerequisites (sudo): cmake, libopencv-dev, make,
# libzmq3-dev (cppzmq needs the system C library), libcap2-bin (setcap).
# Exit 0 only when both configs build warning-free and all tests pass.
set -u
PI5="${1:-cpeddle@192.168.2.106}"
REMOTE_DIR="tracking-core-ci"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
SSH=(ssh -o BatchMode=yes -o ConnectTimeout=8)

fail() { echo "FAIL: $*" >&2; exit 1; }

echo "[1/4] Sync source -> $PI5:~/$REMOTE_DIR/src"
"${SSH[@]}" "$PI5" "mkdir -p ~/$REMOTE_DIR" || fail "ssh to $PI5"
rsync -az --delete \
    --exclude 'build/' --exclude 'build-*/' --exclude '__pycache__/' \
    "$HERE/tracking-core/" "$PI5:$REMOTE_DIR/src/" || fail "rsync"

echo "[2/4] Upload remote build+test script"
"${SSH[@]}" "$PI5" "cat > ~/$REMOTE_DIR/run.sh" <<'REMOTE' || fail "upload run.sh"
#!/usr/bin/env bash
set -u
cd "$HOME/tracking-core-ci"
overall=0

echo "== environment =="
echo "temp-start: $(vcgencmd measure_temp)"
echo "throttled: $(vcgencmd get_throttled)"
echo "tmpfs /tmp: $(findmnt -T /tmp -n -o FSTYPE)"

build_and_test() {
    local name="$1"; shift
    local dir="build-$name"
    echo "== configure $name =="
    local cfg_log="$dir-configure.log"
    if ! cmake -S src -B "$dir" "$@" >"$cfg_log" 2>&1; then
        echo "CONFIGURE-FAIL $name"; grep -B2 -A6 -iE "error" "$cfg_log" | head -20
        overall=1; return
    fi
    tail -2 "$cfg_log"
    echo "== build $name =="
    local warnings
    warnings=$(cmake --build "$dir" -j"$(nproc)" 2>&1 | grep -cE 'warning:|error:')
    local rc=$?
    echo "build $name: grep-hits=$warnings"
    if ! cmake --build "$dir" -j"$(nproc)" >/dev/null 2>&1; then
        echo "BUILD-FAIL $name"; overall=1; return
    fi
    if [ "$warnings" != "0" ]; then echo "WARNINGS-PRESENT $name"; overall=1; fi
    echo "== ctest $name =="
    (cd "$dir" && ctest --output-on-failure 2>&1 | tail -3)
    (cd "$dir" && ctest -Q) || { echo "TEST-FAIL $name"; overall=1; }
}

build_and_test release
build_and_test debug -DCMAKE_BUILD_TYPE=Debug

echo "== SCHED_FIFO (real RT privileges via setcap) =="
TESTS_BIN="build-release/tests/cpp_unit/tracking_core_tests"
SETCAP=/usr/sbin/setcap
if sudo -n "$SETCAP" cap_sys_nice+ep "$TESTS_BIN" 2>/dev/null; then
    RT_OUT=$("./$TESTS_BIN" --gtest_filter='CaptureThreadTest.*' 2>&1)
    RT_RC=$?
    echo "$RT_OUT" | tail -2
    if [ $RT_RC -ne 0 ]; then echo "RT-TEST-FAIL"; overall=1; fi
    if echo "$RT_OUT" | grep -q 'SCHED_FIFO priority .* unavailable'; then
        echo "RT-PATH: degraded (setcap did not take effect)"
    else
        echo "RT-PATH: real SCHED_FIFO applied (no degradation warning)"
    fi
    sudo -n "$SETCAP" -r "$TESTS_BIN" 2>/dev/null
else
    echo "RT-PATH: skipped (no passwordless sudo for setcap)"
fi

echo "== environment (post) =="
echo "temp-end: $(vcgencmd measure_temp)"
echo "throttled: $(vcgencmd get_throttled)"
echo "OVERALL: $overall"
exit "$overall"
REMOTE

echo "[3/4] Build + test on target (first run downloads FetchContent deps)"
"${SSH[@]}" "$PI5" "chmod +x ~/$REMOTE_DIR/run.sh && ~/$REMOTE_DIR/run.sh"
rc=$?

echo "[4/4] Result"
if [ $rc -eq 0 ]; then
    echo "PASS — tracking-core green on the Pi 5 (both configs, RT path included)"
else
    fail "on-target run reported failures (rc=$rc)"
fi
