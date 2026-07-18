"""VIEW-002/003 tests: pydantic schema strictness, connection state machine,
floor transform round-trip, clause rows, timeline session reset, and a
mock-publisher integration run through the real StreamClient (plan U15/U16)."""

from __future__ import annotations

import sys
import time
from pathlib import Path

import pytest
import zmq

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "src" / "viewer"))

import pygame  # noqa: E402
import pydantic  # noqa: E402

from client import ConnectionState, ConnectionTracker, StreamClient  # noqa: E402
from renderer import FloorView  # noqa: E402
from safety_panel import SafetyPanel, clause_rows  # noqa: E402
from schema import TrackingMessage  # noqa: E402


def golden_message() -> dict:
    """A valid v0.3 message mirroring zmq_publisher.hpp's schema block."""
    return {
        "schema_version": 1,
        "message_id": 5,
        "session_id": 1752849000000,
        "publish_timestamp_ms": 1752849000123,
        "frame_capture_timestamp_ms": 1752849000110,
        "system_health": {
            "tracker_state": "RUNNING",
            "calibration_status": "VALID",
            "ball_radius_m": 0.03,
            "cpu_temp_c": 55.5,
            "frames_processed_count": 42,
            "frames_rejected_count": 3,
        },
        "thresholds": {
            "age_max_ms": 50.0,
            "laser_settled_speed_m_per_s": 0.05,
            "alignment_tolerance_m": 0.02,
            "min_unsafe_dwell_ms": 200.0,
        },
        "objects": [
            {
                "object_type": "ball",
                "track_id": 7,
                "track_state": "Confirmed",
                "position": {
                    "coordinate_space": "Plane2D_World",
                    "x_m": 0.1,
                    "y_m": 2.0,
                    "z_m": 0.03,
                    "uncertainty_m": 0.004,
                },
                "pixel": {"x_px": 320.0, "y_px": 200.0},
                "age_ms": 12.5,
                "speed_m_per_s": 0.2,
                "speed_valid": True,
                "safe_for_control": False,
                "unsafe_reasons": ["LaserNotConfirmed"],
            }
        ],
        "safety": {
            "safe": False,
            "unsafe_reasons": ["LaserNotConfirmed"],
            "hysteresis_remaining_ms": 200,
            "distance_valid": False,
        },
    }


class TestSchema:
    def test_golden_message_validates(self) -> None:
        message = TrackingMessage.model_validate(golden_message())
        assert message.objects[0].position.coordinate_space == "Plane2D_World"
        assert message.safety.laser_ball_distance_m is None

    def test_missing_coordinate_space_rejected(self) -> None:
        bad = golden_message()
        del bad["objects"][0]["position"]["coordinate_space"]
        with pytest.raises(pydantic.ValidationError):
            TrackingMessage.model_validate(bad)

    def test_non_finite_float_rejected(self) -> None:
        bad = golden_message()
        bad["objects"][0]["position"]["x_m"] = float("nan")
        with pytest.raises(pydantic.ValidationError):
            TrackingMessage.model_validate(bad)

    def test_unknown_schema_version_rejected(self) -> None:
        bad = golden_message()
        bad["schema_version"] = 2
        with pytest.raises(pydantic.ValidationError):
            TrackingMessage.model_validate(bad)

    def test_wrong_type_rejected(self) -> None:
        bad = golden_message()
        bad["safety"]["safe"] = "yes"
        with pytest.raises(pydantic.ValidationError):
            TrackingMessage.model_validate(bad)

    def test_unknown_extra_field_rejected(self) -> None:
        # extra="forbid": producer drift is a loud violation, not silent skew.
        bad = golden_message()
        bad["surprise"] = 1
        with pytest.raises(pydantic.ValidationError):
            TrackingMessage.model_validate(bad)


class TestConnectionTracker:
    def test_state_machine_transitions(self) -> None:
        tracker = ConnectionTracker(stale_ms=1000.0)
        assert tracker.update(False, 0.0) is ConnectionState.DISCONNECTED
        assert tracker.update(True, 100.0) is ConnectionState.CONNECTED
        assert tracker.reconnected  # first message ever counts as reconnect
        assert tracker.update(False, 500.0) is ConnectionState.CONNECTED
        assert tracker.update(False, 1200.0) is ConnectionState.STALLED
        assert tracker.update(True, 1300.0) is ConnectionState.CONNECTED
        assert tracker.reconnected  # STALLED -> CONNECTED clears trails
        tracker.update(True, 1400.0)
        assert not tracker.reconnected  # healthy stream: no spurious clears


class TestFloorView:
    def test_world_screen_round_trip(self) -> None:
        view = FloorView(pygame.Rect(0, 0, 960, 680), 120.0, 40)
        for x_m, y_m in [(0.0, 0.0), (1.5, 2.0), (-2.0, 0.5)]:
            sx, sy = view.world_to_screen(x_m, y_m)
            rx, ry = view.screen_to_world(sx, sy)
            assert abs(rx - x_m) < 0.01
            assert abs(ry - y_m) < 0.01


class TestSafetyPanel:
    def test_clause_rows_reflect_reasons_and_values(self) -> None:
        message = TrackingMessage.model_validate(golden_message())
        rows = clause_rows(message)
        assert len(rows) == 8
        labels = [row[0] for row in rows]
        assert labels[0].startswith("1 tracker")
        assert rows[0][1] is True  # RUNNING passes
        assert rows[2][1] is False  # LaserNotConfirmed
        assert rows[3][2] == "Confirmed"  # ball state shown
        assert "12" in rows[5][2]  # ball age value rendered
        assert rows[7][2] == "distance undefined"  # no distance published

    def test_timeline_resets_on_session_change(self) -> None:
        panel = SafetyPanel(font=None)  # observe() never touches the font
        message = TrackingMessage.model_validate(golden_message())
        panel.observe(message, 1.0)
        panel.observe(message, 2.0)
        assert len(panel.timeline) == 2

        restarted = golden_message()
        restarted["session_id"] += 1  # producer restart
        panel.observe(TrackingMessage.model_validate(restarted), 3.0)
        assert len(panel.timeline) == 1


class TestStreamClientIntegration:
    def test_valid_and_invalid_messages_over_zmq(self) -> None:
        # Mock publisher: single-part messages per the schema contract.
        context = zmq.Context.instance()
        pub = context.socket(zmq.PUB)
        pub.bind("tcp://127.0.0.1:25549")
        client = StreamClient("tcp://127.0.0.1:25549")
        time.sleep(0.3)  # slow-joiner settle

        valid = TrackingMessage.model_validate(golden_message())
        pub.send_string(valid.model_dump_json(exclude_none=True))
        received = None
        for _ in range(50):
            received = client.poll_message()
            if received is not None:
                break
            time.sleep(0.02)
        assert received is not None
        assert received.message_id == 5
        assert client.violation_count == 0

        pub.send_string('{"schema_version": 99}')  # violation, not a crash
        for _ in range(50):
            if client.poll_message() is not None:
                break
            if client.violation_count:
                break
            time.sleep(0.02)
        assert client.violation_count == 1

        # Stream keeps working after a violation (never blind — plan R21).
        followup = golden_message()
        followup["message_id"] = 6
        pub.send_string(
            TrackingMessage.model_validate(followup).model_dump_json(exclude_none=True)
        )
        received = None
        for _ in range(50):
            received = client.poll_message()
            if received is not None:
                break
            time.sleep(0.02)
        assert received is not None
        assert received.message_id == 6
        pub.close(linger=0)
        client.close()
