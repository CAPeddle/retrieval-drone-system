#!/usr/bin/env python3
"""MJPEG web relay — CAM-002 proof of concept.

Runs on the Pi 5. Connects as a TCP client to the Pi 3B's `rpicam-vid` MJPEG
stream (see start_stream_pi3b.sh), keeps the latest frame (stale frames are
dropped, never queued), and serves:

    /              viewer page (browser)
    /stream        multipart/x-mixed-replace MJPEG for the <img> tag
    /snapshot.jpg  single latest frame (curl-friendly verification)
    /healthz       JSON: connected, frames_total, fps, last_frame_age_s

Stdlib only — no frameworks (python rule: a small tool does not need Flask).
PoC scope: no auth, no TLS, single source. Not the CAM-001 production daemon.

Usage:
    python3 mjpeg_web_relay.py [--source-host 192.168.2.73] [--source-port 8554] [--port 8080]
"""

import argparse
import json
import socket
import threading
import time
from collections import deque
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

SOI = b"\xff\xd8"  # JPEG start-of-image
EOI = b"\xff\xd9"  # JPEG end-of-image
MAX_BUFFER = 8 * 1024 * 1024  # drop a runaway buffer (corrupt stream) rather than grow forever


class MJPEGFrameParser:
    """Incremental JPEG frame extractor over a byte stream.

    Feed arbitrary chunks; get back complete JPEG frames (SOI..EOI). Within
    JPEG entropy-coded data 0xFF is byte-stuffed (FF 00) and restart markers
    are FF D0-D7, so scanning for FF D9 as the frame end is sound for MJPEG.
    """

    def __init__(self):
        self._buf = bytearray()

    def feed(self, chunk: bytes):
        """Consume a chunk, return a list of complete JPEG frames (possibly empty)."""
        self._buf.extend(chunk)
        frames = []
        while True:
            start = self._buf.find(SOI)
            if start < 0:
                # No frame start in buffer: keep only a trailing byte in case
                # the next chunk begins with the second half of a split SOI.
                del self._buf[:-1]
                break
            if start > 0:
                del self._buf[:start]  # discard garbage before SOI
            end = self._buf.find(EOI, 2)
            if end < 0:
                if len(self._buf) > MAX_BUFFER:
                    del self._buf[:]  # corrupt/oversized frame: resync
                break
            frames.append(bytes(self._buf[: end + 2]))
            del self._buf[: end + 2]
        return frames


class LatestFrame:
    """Latest-wins frame slot. Readers wait on a version bump; no queueing."""

    def __init__(self):
        self._cond = threading.Condition()
        self._frame = None
        self._version = 0
        self._frame_time = 0.0
        self._times = deque(maxlen=60)  # recent frame arrival times, for fps
        self.frames_total = 0
        self.connected = False

    def publish(self, frame: bytes):
        with self._cond:
            self._frame = frame
            self._version += 1
            self._frame_time = time.monotonic()
            self._times.append(self._frame_time)
            self.frames_total += 1
            self._cond.notify_all()

    def wait_next(self, last_version: int, timeout: float = 5.0):
        """Block until a frame newer than last_version exists (or timeout)."""
        with self._cond:
            self._cond.wait_for(lambda: self._version != last_version, timeout=timeout)
            return self._frame, self._version

    def snapshot(self):
        with self._cond:
            return self._frame

    def stats(self):
        with self._cond:
            now = time.monotonic()
            window = [t for t in self._times if now - t <= 3.0]
            fps = len(window) / 3.0
            age = (now - self._frame_time) if self._frame is not None else None
            return {
                "connected": self.connected,
                "frames_total": self.frames_total,
                "fps": round(fps, 1),
                "last_frame_age_s": round(age, 2) if age is not None else None,
            }


def source_reader(host: str, port: int, latest: LatestFrame, stop: threading.Event):
    """Connect to the rpicam-vid TCP listener; reconnect with backoff forever."""
    backoff = 1.0
    while not stop.is_set():
        parser = MJPEGFrameParser()
        try:
            with socket.create_connection((host, port), timeout=5.0) as sock:
                sock.settimeout(5.0)
                latest.connected = True
                backoff = 1.0
                print(f"[relay] connected to source {host}:{port}")
                while not stop.is_set():
                    chunk = sock.recv(65536)
                    if not chunk:
                        raise ConnectionError("source closed the stream")
                    for frame in parser.feed(chunk):
                        latest.publish(frame)
        except OSError as e:
            latest.connected = False
            print(f"[relay] source unavailable ({e}); retrying in {backoff:.0f}s")
            stop.wait(backoff)
            backoff = min(backoff * 2, 10.0)


INDEX_HTML = """<!doctype html>
<title>CAM-002 PoC — Pi3B camera via Pi5 relay</title>
<style>body{font-family:sans-serif;background:#111;color:#ddd;text-align:center}
img{max-width:95vw;border:1px solid #444}#h{color:#8a8}</style>
<h2>Pi 3B camera &mdash; relayed by Pi 5 (CAM-002 PoC)</h2>
<img src="/stream" alt="live stream">
<p id="h">connecting&hellip;</p>
<script>
setInterval(async()=>{try{const r=await fetch('/healthz');const j=await r.json();
document.getElementById('h').textContent=
 `source connected: ${j.connected} | fps: ${j.fps} | frames: ${j.frames_total}`;
}catch(e){document.getElementById('h').textContent='relay unreachable';}},2000);
</script>
"""

BOUNDARY = "cam002frame"


def make_handler(latest: LatestFrame):
    class Handler(BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, fmt, *args):  # quiet per-request logging
            pass

        def _send(self, code, ctype, body):
            self.send_response(code)
            self.send_header("Content-Type", ctype)
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def do_GET(self):
            if self.path == "/":
                self._send(200, "text/html; charset=utf-8", INDEX_HTML.encode())
            elif self.path == "/healthz":
                self._send(200, "application/json", json.dumps(latest.stats()).encode())
            elif self.path == "/snapshot.jpg":
                frame = latest.snapshot()
                if frame is None:
                    self._send(503, "text/plain", b"no frame yet\n")
                else:
                    self._send(200, "image/jpeg", frame)
            elif self.path == "/stream":
                self.send_response(200)
                self.send_header(
                    "Content-Type", f"multipart/x-mixed-replace; boundary={BOUNDARY}"
                )
                self.end_headers()
                version = 0
                try:
                    while True:
                        frame, version = latest.wait_next(version)
                        if frame is None:
                            continue
                        self.wfile.write(
                            b"--" + BOUNDARY.encode() + b"\r\n"
                            b"Content-Type: image/jpeg\r\n"
                            b"Content-Length: " + str(len(frame)).encode() + b"\r\n\r\n"
                        )
                        self.wfile.write(frame)
                        self.wfile.write(b"\r\n")
                except (BrokenPipeError, ConnectionResetError):
                    pass  # viewer left
            else:
                self._send(404, "text/plain", b"not found\n")

    return Handler


def main():
    ap = argparse.ArgumentParser(description="CAM-002 MJPEG web relay (Pi 5)")
    ap.add_argument("--source-host", default="192.168.2.73")
    ap.add_argument("--source-port", type=int, default=8554)
    ap.add_argument("--port", type=int, default=8080, help="HTTP listen port")
    args = ap.parse_args()

    latest = LatestFrame()
    stop = threading.Event()
    reader = threading.Thread(
        target=source_reader,
        args=(args.source_host, args.source_port, latest, stop),
        daemon=True,
    )
    reader.start()

    server = ThreadingHTTPServer(("0.0.0.0", args.port), make_handler(latest))
    print(f"[relay] serving http://0.0.0.0:{args.port}/ "
          f"(source {args.source_host}:{args.source_port})")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        stop.set()
        server.shutdown()


if __name__ == "__main__":
    main()
