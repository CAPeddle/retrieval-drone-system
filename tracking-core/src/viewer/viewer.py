"""VIEW-002/003: floor-frame viewer for the v0.3 tracking stream.

Layout (plan U15, shared with the safety panel): the floor-plane render fills
the main/left area; a fixed-width right sidebar stacks the safe_for_control
indicator (top, largest), the clause panel, and the health summary; the 30 s
safety timeline runs full-width along the bottom.

Staleness is LOCAL receive time (plan R19); schema violations are counted,
displayed, and force the STALLED visual so an outdated SAFE never looks
current (plan R21). Trails clear on session_id change and on reconnect.

Usage:
    python viewer.py [--endpoint tcp://localhost:5555] [--floor-scale 120]
                     [--trail-length 40] [--stale-ms 1000]
"""

from __future__ import annotations

import argparse
import time

import pygame

from client import ConnectionState, ConnectionTracker, StreamClient
from renderer import TEXT, FloorView
from safety_panel import SafetyPanel

SIDEBAR_WIDTH = 320
TIMELINE_HEIGHT = 40
WINDOW_SIZE = (1280, 720)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--endpoint", default="tcp://localhost:5555")
    parser.add_argument(
        "--floor-scale",
        type=float,
        default=120.0,
        help="pixels per metre in the floor view",
    )
    parser.add_argument("--trail-length", type=int, default=40)
    parser.add_argument(
        "--stale-ms",
        type=float,
        default=1000.0,
        help="no message for this long -> STALLED (plan R19)",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    pygame.init()
    screen = pygame.display.set_mode(WINDOW_SIZE)
    pygame.display.set_caption("tracking-core floor viewer (v0.3)")
    font = pygame.font.SysFont("monospace", 14)
    clock = pygame.time.Clock()

    floor_rect = pygame.Rect(
        0, 0, WINDOW_SIZE[0] - SIDEBAR_WIDTH, WINDOW_SIZE[1] - TIMELINE_HEIGHT
    )
    sidebar_rect = pygame.Rect(floor_rect.right, 0, SIDEBAR_WIDTH, floor_rect.height)
    timeline_rect = pygame.Rect(0, floor_rect.bottom, WINDOW_SIZE[0], TIMELINE_HEIGHT)

    view = FloorView(floor_rect, args.floor_scale, args.trail_length)
    panel = SafetyPanel(font)
    client = StreamClient(args.endpoint)
    tracker = ConnectionTracker(stale_ms=args.stale_ms)

    last_message = None
    last_session: int | None = None
    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        now_ms = time.monotonic() * 1000.0
        message = client.poll_message()
        state = tracker.update(message is not None, now_ms)
        if message is not None:
            if last_session is not None and message.session_id != last_session:
                view.clear_trails()  # producer restarted (plan R20/U15)
            last_session = message.session_id
            last_message = message
            panel.observe(message, now_ms / 1000.0)
        if tracker.reconnected:
            view.clear_trails()  # pre-disconnect positions are stale (plan U15)

        stalled = state is not ConnectionState.CONNECTED
        view.draw(screen, last_message, stalled)
        panel.draw_sidebar(screen, sidebar_rect, last_message, stalled)
        panel.draw_timeline(screen, timeline_rect, now_ms / 1000.0)

        status = state.value
        if client.violation_count:
            status += f"  schema violations: {client.violation_count}"
        screen.blit(font.render(status, True, TEXT), (8, 8))

        pygame.display.flip()
        clock.tick(60)

    client.close()
    pygame.quit()


if __name__ == "__main__":
    main()
