"""VIEW-002: pydantic models for the v0.3 tracking stream.

Mirror of the schema documented in ``src/core/include/zmq_publisher.hpp`` —
keep the two in lockstep. ``extra="forbid"`` plus finite-float validation make
producer drift and non-finite values loud schema violations (plan R21) instead
of silently skewed renders.
"""

from __future__ import annotations

import math
from typing import Literal

from pydantic import BaseModel, ConfigDict, field_validator

TrackStateName = Literal[
    "Provisional", "Confirmed", "Predicted", "Occluded", "Lost", "Retired"
]

UNSAFE_REASON_NAMES = (
    "TrackerNotRunning",
    "CalibrationInvalid",
    "LaserNotConfirmed",
    "BallNotConfirmed",
    "AgeExceedsThreshold",
    "LaserInTransit",
    "LaserBallMisaligned",
)


class _StrictModel(BaseModel):
    # strict=True: no lax coercion ("yes" must never become True on a safety
    # field); extra="forbid": producer drift is a loud violation.
    model_config = ConfigDict(extra="forbid", strict=True)

    @field_validator("*")
    @classmethod
    def _finite_floats(cls, value: object) -> object:
        if isinstance(value, float) and not math.isfinite(value):
            raise ValueError("non-finite float in tracking message")
        return value


class Position(_StrictModel):
    coordinate_space: Literal["Plane2D_World"]
    x_m: float
    y_m: float
    z_m: float
    uncertainty_m: float


class Pixel(_StrictModel):
    x_px: float
    y_px: float


class TrackedObject(_StrictModel):
    object_type: Literal["laser", "ball"]
    track_id: int
    track_state: TrackStateName
    position: Position
    pixel: Pixel
    age_ms: float
    speed_m_per_s: float
    speed_valid: bool
    safe_for_control: bool
    unsafe_reasons: list[str]


class SystemHealth(_StrictModel):
    tracker_state: Literal["INITIALISING", "RUNNING"]
    calibration_status: Literal["VALID"]
    ball_radius_m: float
    cpu_temp_c: float
    frames_processed_count: int
    frames_rejected_count: int


class Thresholds(_StrictModel):
    age_max_ms: float
    laser_settled_speed_m_per_s: float
    alignment_tolerance_m: float
    min_unsafe_dwell_ms: float


class Safety(_StrictModel):
    safe: bool
    unsafe_reasons: list[str]
    hysteresis_remaining_ms: int
    distance_valid: bool
    laser_ball_distance_m: float | None = None  # present only when distance_valid


class TrackingMessage(_StrictModel):
    schema_version: Literal[1]
    message_id: int
    session_id: int
    publish_timestamp_ms: int
    frame_capture_timestamp_ms: int
    system_health: SystemHealth
    thresholds: Thresholds
    objects: list[TrackedObject]
    safety: Safety
