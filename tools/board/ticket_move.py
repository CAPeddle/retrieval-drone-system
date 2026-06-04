#!/usr/bin/env python3
"""Move a ticket: update story status, append Log entry, move BOARD line.

Usage:
    python tools/board/ticket_move.py PREFIX-NNN <status> [--note "reason"]

<status> is one of: backlog, next, in-progress, blocked, done.
Use --note to add a one-sentence Log reason and (on blocked) a (reason) suffix.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _lib import (  # noqa: E402
    BOARD_PATH,
    COLUMN_TO_STATUS,
    STATUS_TO_COLUMN,
    find_story_file,
    parse_board,
    parse_frontmatter,
    render_frontmatter,
    today,
)

VALID_STATUSES = sorted(set(COLUMN_TO_STATUS.values()))


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("ticket_id", help="Ticket ID, e.g. TRK-001")
    ap.add_argument("target_status", choices=VALID_STATUSES, help="Target status")
    ap.add_argument("--note", default="", help="Optional one-sentence note for Log / Blocked suffix")
    args = ap.parse_args()

    try:
        story_path = find_story_file(args.ticket_id)
    except (FileNotFoundError, RuntimeError) as e:
        print(f"error: {e}", file=sys.stderr)
        return 1

    story_text = story_path.read_text(encoding="utf-8")
    try:
        fm, body = parse_frontmatter(story_text)
    except ValueError as e:
        print(f"error: cannot parse frontmatter in {story_path.name}: {e}", file=sys.stderr)
        return 1
    if fm.id != args.ticket_id:
        print(
            f"error: story file {story_path.name} has id={fm.id!r}, expected {args.ticket_id!r}",
            file=sys.stderr,
        )
        return 1

    old_status = fm.status
    if old_status == args.target_status:
        print(f"warning: {args.ticket_id} already at status {old_status!r}; no change")
        return 0

    fm.status = args.target_status
    fm.updated = today()
    new_frontmatter = render_frontmatter(fm)

    if "## Log" not in body:
        print(f"error: story file {story_path.name} missing '## Log' section", file=sys.stderr)
        return 1
    log_entry = f"- {today()}: {old_status} → {args.target_status}."
    if args.note:
        log_entry += f" {args.note.rstrip('.')}."
    new_body = body.rstrip() + "\n" + log_entry + "\n"
    story_path.write_text(new_frontmatter + new_body, encoding="utf-8")

    board_text = BOARD_PATH.read_text(encoding="utf-8")
    tickets = parse_board(board_text)
    target = next((t for t in tickets if t.ticket_id == args.ticket_id), None)
    if target is None:
        print(
            f"warning: ticket {args.ticket_id} not on BOARD.md; story updated, board unchanged",
        )
        return 0

    target_column = STATUS_TO_COLUMN[args.target_status]
    if target.column == target_column:
        print(f"warning: ticket {args.ticket_id} already in column {target_column!r}")
        return 0

    # Remove the existing line. Replace with newline-anchored match so we don't
    # leave a dangling blank line.
    if (target.raw + "\n") in board_text:
        board_text = board_text.replace(target.raw + "\n", "", 1)
    else:
        board_text = board_text.replace(target.raw, "", 1)

    relative_path = story_path.relative_to(BOARD_PATH.parent).as_posix()
    story_link = f"[story]({relative_path})"
    if args.target_status == "done":
        new_line = f"- [x] {args.ticket_id} — {target.title} ({today()})"
    elif args.target_status == "blocked":
        suffix = f" ({args.note})" if args.note else " (blocked)"
        new_line = f"- [ ] {args.ticket_id} — {target.title} → {story_link}{suffix}"
    else:
        new_line = f"- [ ] {args.ticket_id} — {target.title} → {story_link}"

    header_marker = f"## {target_column}"
    header_pos = board_text.find(header_marker)
    if header_pos == -1:
        print(f"error: BOARD.md missing column '{target_column}'", file=sys.stderr)
        return 1
    section_end = board_text.find("\n## ", header_pos + 1)
    if section_end == -1:
        if not board_text.endswith("\n"):
            board_text += "\n"
        board_text += new_line + "\n"
    else:
        # Walk back across trailing blank lines so the new line sits flush with the section content.
        insert_pos = section_end
        while insert_pos > header_pos and board_text[insert_pos - 1] == "\n":
            insert_pos -= 1
        board_text = board_text[:insert_pos] + "\n" + new_line + board_text[insert_pos:]

    BOARD_PATH.write_text(board_text, encoding="utf-8")
    print(f"moved {args.ticket_id}: {old_status} → {args.target_status}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
