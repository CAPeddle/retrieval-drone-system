"""VIEW-002: floor-plane renderer. Layout per the v0.3 slice plan (U15): the
floor view fills the main/left area; spatial overlays (trails, uncertainty
rings, the VIEW-003 alignment circle) render inside it."""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, field

import pygame

from schema import TrackingMessage

# Colours (STALLED renders desaturated via `dim`).
BACKGROUND = (18, 18, 24)
GRID = (45, 45, 60)
BALL = (80, 180, 255)
LASER = (255, 80, 80)
UNCERTAINTY = (120, 120, 160)
ALIGNMENT = (240, 200, 60)
TEXT = (200, 200, 210)


def dim(colour: tuple[int, int, int], factor: float = 0.35) -> tuple[int, int, int]:
    """Desaturated variant for the STALLED visual treatment (plan U15)."""
    return (
        int(colour[0] * factor + 40),
        int(colour[1] * factor + 40),
        int(colour[2] * factor + 40),
    )


@dataclass
class FloorView:
    """Metres -> screen transform: origin at the bottom-centre of the view
    (the camera sits at floor (0,0) looking toward +y, which points up the
    screen)."""

    rect: pygame.Rect
    scale_px_per_m: float
    trail_length: int
    trails: dict[str, deque] = field(default_factory=dict)

    def world_to_screen(self, x_m: float, y_m: float) -> tuple[int, int]:
        sx = self.rect.centerx + x_m * self.scale_px_per_m
        sy = self.rect.bottom - 40 - y_m * self.scale_px_per_m
        return int(sx), int(sy)

    def screen_to_world(self, sx: float, sy: float) -> tuple[float, float]:
        x_m = (sx - self.rect.centerx) / self.scale_px_per_m
        y_m = (self.rect.bottom - 40 - sy) / self.scale_px_per_m
        return x_m, y_m

    def clear_trails(self) -> None:
        self.trails.clear()

    def draw(
        self, surface: pygame.Surface, message: TrackingMessage | None, stalled: bool
    ) -> None:
        surface.fill(BACKGROUND, self.rect)
        self._draw_grid(surface)
        if message is None:
            return
        factor = 0.35 if stalled else 1.0
        for obj in message.objects:
            colour = LASER if obj.object_type == "laser" else BALL
            if stalled:
                colour = dim(colour)
            pos = self.world_to_screen(obj.position.x_m, obj.position.y_m)
            trail = self.trails.setdefault(
                obj.object_type, deque(maxlen=self.trail_length)
            )
            if not stalled:
                trail.append(pos)
            for i, point in enumerate(trail):
                alpha = int(120 * (i + 1) / len(trail))
                pygame.draw.circle(
                    surface, tuple(c * alpha // 255 for c in colour), point, 2
                )
            if obj.object_type == "ball":
                radius_px = max(
                    3, int(message.system_health.ball_radius_m * self.scale_px_per_m)
                )
                pygame.draw.circle(surface, colour, pos, radius_px, 2)
                # VIEW-003 alignment envelope: radius + tolerance, in-world.
                envelope_px = int(
                    (
                        message.system_health.ball_radius_m
                        + message.thresholds.alignment_tolerance_m
                    )
                    * self.scale_px_per_m
                )
                pygame.draw.circle(
                    surface,
                    dim(ALIGNMENT) if stalled else ALIGNMENT,
                    pos,
                    max(envelope_px, radius_px + 2),
                    1,
                )
            else:
                pygame.draw.circle(surface, colour, pos, 4)
            uncertainty_px = int(obj.position.uncertainty_m * self.scale_px_per_m)
            if uncertainty_px > 2:
                pygame.draw.circle(
                    surface,
                    dim(UNCERTAINTY, factor) if stalled else UNCERTAINTY,
                    pos,
                    uncertainty_px,
                    1,
                )

    def _draw_grid(self, surface: pygame.Surface) -> None:
        # 1 m grid lines across the visible extent.
        metres_wide = int(self.rect.width / self.scale_px_per_m) + 2
        metres_deep = int(self.rect.height / self.scale_px_per_m) + 2
        for gx in range(-metres_wide // 2, metres_wide // 2 + 1):
            x0, y0 = self.world_to_screen(gx, 0)
            x1, y1 = self.world_to_screen(gx, metres_deep)
            pygame.draw.line(surface, GRID, (x0, y0), (x1, y1), 1)
        for gy in range(0, metres_deep + 1):
            x0, y0 = self.world_to_screen(-metres_wide / 2, gy)
            x1, y1 = self.world_to_screen(metres_wide / 2, gy)
            pygame.draw.line(surface, GRID, (x0, y0), (x1, y1), 1)
