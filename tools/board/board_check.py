#!/usr/bin/env python3
"""Lint BOARD.md and docs/tickets/ for consistency.

Rules (matching .claude/rules/tickets.md):
  - Every BOARD line at status Next, In Progress, or Blocked MUST have a story file
    in docs/tickets/ whose frontmatter `status:` matches the column.
  - Every story file with status next | in-progress | blocked MUST have a BOARD line
    in the matching column.
  - Backlog and Done lines MAY exist without a story file (one-liner mode).
  - Where a story file exists for a Backlog or Done ticket, its `status:` must still match.

Exits 0 if clean, 1 if any issues. Intended for pre-commit / pre-PR use.

Usage:
    python tools/board/board_check.py
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _lib import (  # noqa: E402
    BOARD_PATH,
    COLUMN_TO_STATUS,
    STATUSES_REQUIRING_STORY,
    TICKETS_DIR,
    parse_board,
    parse_frontmatter,
)


def main() -> int:
    errors: list[str] = []
    warnings: list[str] = []

    if not BOARD_PATH.exists():
        print(f"error: {BOARD_PATH} does not exist", file=sys.stderr)
        return 1
    board_text = BOARD_PATH.read_text(encoding="utf-8")
    board_tickets = parse_board(board_text)
    board_ids: dict[str, str] = {t.ticket_id: COLUMN_TO_STATUS[t.column] for t in board_tickets}

    for t in board_tickets:
        expected_status = COLUMN_TO_STATUS.get(t.column)
        if expected_status is None:
            errors.append(f"{t.ticket_id}: unrecognised column {t.column!r}")
            continue
        story_files = sorted(TICKETS_DIR.glob(f"{t.ticket_id}-*.md"))
        if not story_files:
            if expected_status in STATUSES_REQUIRING_STORY:
                errors.append(
                    f"{t.ticket_id}: in column {t.column!r} (status={expected_status}) "
                    f"but no story file in {TICKETS_DIR}"
                )
            # Backlog/Done with no story file is OK by design.
            continue
        if len(story_files) > 1:
            errors.append(
                f"{t.ticket_id}: multiple story files: {[p.name for p in story_files]}"
            )
            continue
        story_path = story_files[0]
        try:
            fm, _ = parse_frontmatter(story_path.read_text(encoding="utf-8"))
        except ValueError as e:
            errors.append(f"{story_path.name}: cannot parse frontmatter: {e}")
            continue
        if fm.id != t.ticket_id:
            errors.append(f"{story_path.name}: id field {fm.id!r} does not match filename {t.ticket_id!r}")
        if fm.status != expected_status:
            errors.append(
                f"{t.ticket_id}: BOARD column {t.column!r} expects status={expected_status!r} "
                f"but story status is {fm.status!r}"
            )

    # 2. Every story file with active status (next/in-progress/blocked) has a BOARD line.
    for story_path in sorted(TICKETS_DIR.glob("*.md")):
        try:
            fm, _ = parse_frontmatter(story_path.read_text(encoding="utf-8"))
        except ValueError as e:
            errors.append(f"{story_path.name}: cannot parse frontmatter: {e}")
            continue
        if fm.id is None:
            errors.append(f"{story_path.name}: missing id field")
            continue
        if fm.status in STATUSES_REQUIRING_STORY and fm.id not in board_ids:
            errors.append(
                f"{fm.id}: story file at status={fm.status!r} but no matching BOARD line"
            )

    # Output
    if warnings:
        print(f"board_check: {len(warnings)} warning(s):")
        for w in warnings:
            print(f"  ! {w}")
    if errors:
        print(f"board_check: {len(errors)} issue(s) found:", file=sys.stderr)
        for e in errors:
            print(f"  - {e}", file=sys.stderr)
        return 1
    n_stories = len(list(TICKETS_DIR.glob("*.md"))) if TICKETS_DIR.exists() else 0
    print(f"board_check: OK ({len(board_tickets)} board tickets, {n_stories} story files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
