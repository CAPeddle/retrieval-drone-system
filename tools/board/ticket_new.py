#!/usr/bin/env python3
"""Scaffold a new ticket story file and add a Backlog line to BOARD.md.

Usage:
    python tools/board/ticket_new.py PREFIX-NNN "Title in human form" [--tier=mechanical]

The prefix must be one of: TRK, VIEW, CAM, LASER, MAV, DRONE, DOCS, META.
Tier defaults to 'mechanical'. Use 'small' or 'design' as appropriate.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _lib import (  # noqa: E402
    BOARD_PATH,
    PREFIX_TO_SUBSYSTEM,
    TICKET_ID_RE,
    TICKETS_DIR,
    Frontmatter,
    render_frontmatter,
    slugify,
    today,
)

VALID_TIERS = ("mechanical", "small", "design")

STORY_TEMPLATE = """{frontmatter}
## Context

(Why this work matters. One paragraph.)

## Acceptance

- (Falsifiable criterion 1.)
- (Falsifiable criterion 2.)

## Plan

1. (First step.)
2. (Second step.)

## Log

- {today}: created. Status: backlog.
"""


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("ticket_id", help="Ticket ID, e.g. TRK-002")
    ap.add_argument("title", help="Human-readable title")
    ap.add_argument("--tier", default="mechanical", choices=VALID_TIERS)
    args = ap.parse_args()

    m = TICKET_ID_RE.match(args.ticket_id)
    if not m:
        print(f"error: invalid ticket ID format: {args.ticket_id!r} (expected PREFIX-NNN)", file=sys.stderr)
        return 1
    prefix = m.group(1)
    if prefix not in PREFIX_TO_SUBSYSTEM:
        print(f"error: unknown prefix {prefix!r}. Known: {sorted(PREFIX_TO_SUBSYSTEM)}", file=sys.stderr)
        return 1
    subsystem = PREFIX_TO_SUBSYSTEM[prefix]
    slug = slugify(args.title)
    if not slug:
        print(f"error: title produced an empty slug: {args.title!r}", file=sys.stderr)
        return 1
    story_path = TICKETS_DIR / f"{args.ticket_id}-{slug}.md"
    if story_path.exists():
        print(f"error: story file already exists: {story_path}", file=sys.stderr)
        return 1

    fm = Frontmatter(
        id=args.ticket_id,
        status="backlog",
        subsystem=subsystem,
        tier=args.tier,
        created=today(),
        updated=today(),
        spec=None,
        plan=None,
        blockers=[],
    )
    story_path.parent.mkdir(parents=True, exist_ok=True)
    story_path.write_text(
        STORY_TEMPLATE.format(frontmatter=render_frontmatter(fm).rstrip(), today=today()),
        encoding="utf-8",
    )

    board_text = BOARD_PATH.read_text(encoding="utf-8")
    if "## Backlog" not in board_text:
        print(f"error: BOARD.md missing '## Backlog' section", file=sys.stderr)
        return 1
    if not board_text.endswith("\n"):
        board_text += "\n"
    relative_path = story_path.relative_to(BOARD_PATH.parent).as_posix()
    new_line = f"- [ ] {args.ticket_id} — {args.title} → [story]({relative_path})"
    board_text += new_line + "\n"
    BOARD_PATH.write_text(board_text, encoding="utf-8")

    print(f"created: {story_path}")
    print(f"appended to BOARD.md Backlog: {new_line}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
