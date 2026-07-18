"""VIEW-002: stream client — socket handling, validation, connection state.

Pure logic (no pygame) so the state machine and validation path are unit
testable. Staleness derives from LOCAL receive time plus in-message relative
ages, never cross-host wall clocks (plan R19 — the Pi has no NTP).
"""

from __future__ import annotations

import enum
from dataclasses import dataclass, field

import zmq
from pydantic import ValidationError

from schema import TrackingMessage


class ConnectionState(enum.Enum):
    DISCONNECTED = "DISCONNECTED"  # no message yet / socket-level loss
    CONNECTED = "CONNECTED"
    STALLED = "STALLED"  # no message for longer than stale_ms


@dataclass
class ConnectionTracker:
    """Connection state machine (plan R19).

    ``update`` is fed (message_received, now_ms) every UI frame and returns the
    current state; ``reconnected`` flags a DISCONNECTED/STALLED -> CONNECTED
    transition so the caller can clear per-connection artefacts (trails).
    """

    stale_ms: float
    last_message_ms: float | None = None
    reconnected: bool = field(default=False, init=False)

    def update(self, message_received: bool, now_ms: float) -> ConnectionState:
        self.reconnected = False
        if message_received:
            was_healthy = (
                self.last_message_ms is not None
                and now_ms - self.last_message_ms <= self.stale_ms
            )
            if not was_healthy:
                self.reconnected = True
            self.last_message_ms = now_ms
            return ConnectionState.CONNECTED
        if self.last_message_ms is None:
            return ConnectionState.DISCONNECTED
        if now_ms - self.last_message_ms > self.stale_ms:
            return ConnectionState.STALLED
        return ConnectionState.CONNECTED


class StreamClient:
    """SUB socket per ADR-002 (RCVHWM=1, CONFLATE=1, empty filter) plus
    validation. Schema violations are counted and surfaced, never silently
    discarded (plan R21)."""

    def __init__(self, endpoint: str) -> None:
        self._context = zmq.Context.instance()
        self.socket = self._context.socket(zmq.SUB)
        self.socket.setsockopt(zmq.RCVHWM, 1)
        self.socket.setsockopt(zmq.CONFLATE, 1)
        self.socket.setsockopt_string(zmq.SUBSCRIBE, "")  # single-part schema
        self.socket.connect(endpoint)
        self.violation_count = 0

    def poll_message(self) -> TrackingMessage | None:
        """Non-blocking receive + validate; None when nothing valid arrived."""
        try:
            raw = self.socket.recv(flags=zmq.NOBLOCK)
        except zmq.Again:
            return None
        return self.validate(raw)

    def validate(self, raw: bytes) -> TrackingMessage | None:
        try:
            return TrackingMessage.model_validate_json(raw)
        except ValidationError:
            # Counted and displayed; the caller marks the last-valid render
            # stale so an outdated SAFE can never look current (plan R21/U15).
            self.violation_count += 1
            return None

    def close(self) -> None:
        self.socket.close(linger=0)
