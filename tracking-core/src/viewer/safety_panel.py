"""VIEW-003: clause-level safe_for_control visualisation — sidebar indicator,
8-row clause panel (value vs threshold), hysteresis countdown, and the 30 s
timeline strip. Timeline and per-session state reset on session_id change."""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field

import pygame

from renderer import dim
from schema import TrackingMessage

SAFE_GREEN = (60, 200, 90)
UNSAFE_RED = (220, 70, 60)
ROW_OK = (110, 200, 130)
ROW_FAIL = (235, 110, 100)
TEXT = (205, 205, 215)
PANEL_BG = (28, 28, 36)

TIMELINE_SECONDS = 30.0


def _object(message: TrackingMessage, object_type: str):
    for obj in message.objects:
        if obj.object_type == object_type:
            return obj
    return None


def clause_rows(message: TrackingMessage) -> list[tuple[str, bool, str]]:
    """(label, passing, value-vs-threshold text) for the 8 ADR-007 clauses."""
    reasons = set(message.safety.unsafe_reasons)
    thresholds = message.thresholds
    laser = _object(message, "laser")
    ball = _object(message, "ball")

    def age_text(obj) -> str:
        return (
            f"{obj.age_ms:.0f} / {thresholds.age_max_ms:.0f} ms" if obj else "no track"
        )

    rows = [
        (
            "1 tracker RUNNING",
            "TrackerNotRunning" not in reasons,
            message.system_health.tracker_state,
        ),
        (
            "2 calibration VALID",
            "CalibrationInvalid" not in reasons,
            message.system_health.calibration_status,
        ),
        (
            "3 laser Confirmed",
            "LaserNotConfirmed" not in reasons,
            laser.track_state if laser else "no track",
        ),
        (
            "4 ball Confirmed",
            "BallNotConfirmed" not in reasons,
            ball.track_state if ball else "no track",
        ),
        ("5 laser age", "AgeExceedsThreshold" not in reasons, age_text(laser)),
        ("6 ball age", "AgeExceedsThreshold" not in reasons, age_text(ball)),
        (
            "7 laser settled",
            "LaserInTransit" not in reasons,
            (
                f"{laser.speed_m_per_s:.3f} / {thresholds.laser_settled_speed_m_per_s:.3f} m/s"
                if laser and laser.speed_valid
                else "speed undefined"
            ),
        ),
        (
            "8 aligned",
            "LaserBallMisaligned" not in reasons,
            (
                f"{message.safety.laser_ball_distance_m:.3f} / "
                f"{message.system_health.ball_radius_m + thresholds.alignment_tolerance_m:.3f} m"
                if message.safety.distance_valid
                and message.safety.laser_ball_distance_m is not None
                else "distance undefined"
            ),
        ),
    ]
    return rows


@dataclass
class SafetyPanel:
    font: pygame.font.Font | None  # None in logic-only tests (no draw calls)
    timeline: deque = field(default_factory=deque)  # (t_seconds, safe: bool)
    session_id: int | None = None

    def observe(self, message: TrackingMessage, t_seconds: float) -> None:
        if self.session_id != message.session_id:
            # Producer restarted: history belongs to a different run (plan R20).
            self.timeline.clear()
            self.session_id = message.session_id
        self.timeline.append((t_seconds, message.safety.safe))
        while self.timeline and t_seconds - self.timeline[0][0] > TIMELINE_SECONDS:
            self.timeline.popleft()

    def draw_sidebar(
        self,
        surface: pygame.Surface,
        rect: pygame.Rect,
        message: TrackingMessage | None,
        stalled: bool,
    ) -> None:
        surface.fill(PANEL_BG, rect)
        y = rect.top + 8
        if message is None:
            return
        # Prominent indicator (largest element, top of the sidebar — plan U15).
        colour = SAFE_GREEN if message.safety.safe else UNSAFE_RED
        if stalled:
            colour = dim(colour)
        indicator = pygame.Rect(rect.left + 8, y, rect.width - 16, 56)
        pygame.draw.rect(surface, colour, indicator, border_radius=6)
        label = "SAFE" if message.safety.safe else "UNSAFE"
        if stalled:
            label += " (STALE)"
        text = self.font.render(label, True, (10, 10, 10))
        surface.blit(text, text.get_rect(center=indicator.center))
        y = indicator.bottom + 6

        # Hysteresis countdown (VIEW-003).
        if not message.safety.safe and message.safety.hysteresis_remaining_ms > 0:
            countdown = self.font.render(
                f"dwell {message.safety.hysteresis_remaining_ms} ms", True, TEXT
            )
            surface.blit(countdown, (rect.left + 10, y))
        y += 22

        for label, passing, value in clause_rows(message):
            row_colour = ROW_OK if passing else ROW_FAIL
            if stalled:
                row_colour = dim(row_colour)
            surface.blit(self.font.render(label, True, row_colour), (rect.left + 10, y))
            surface.blit(
                self.font.render(value, True, row_colour), (rect.left + 150, y)
            )
            y += 20

        y += 8
        health = message.system_health
        for line in (
            f"tracker {health.tracker_state}",
            f"temp {health.cpu_temp_c:.1f} C" if health.cpu_temp_c >= 0 else "temp n/a",
            f"frames {health.frames_processed_count} (+{health.frames_rejected_count} rej)",
        ):
            surface.blit(self.font.render(line, True, TEXT), (rect.left + 10, y))
            y += 18

    def draw_timeline(
        self, surface: pygame.Surface, rect: pygame.Rect, now_seconds: float
    ) -> None:
        surface.fill(PANEL_BG, rect)
        for t, safe in self.timeline:
            frac = 1.0 - (now_seconds - t) / TIMELINE_SECONDS
            if frac < 0.0:
                continue
            x = rect.left + int(frac * rect.width)
            colour = SAFE_GREEN if safe else UNSAFE_RED
            pygame.draw.line(
                surface, colour, (x, rect.top + 4), (x, rect.bottom - 4), 2
            )
