#!/usr/bin/env python3
"""Trim Done (recent) when it exceeds N items.

Oldest items (by trailing (YYYY-MM-DD) date suffix) are moved to docs/board-archive.md,
grouped under ## YYYY-MM headers.

Usage:
    python tools/board/ticket_archive.py [--keep 10]
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _lib import ARCHIVE_PATH, BOARD_PATH  # noqa: E402

DONE_HEADER = "## Done (recent)"
DATE_SUFFIX_RE = re.compile(r"\((\d{4})-(\d{2})-\d{2}\)\s*$")
ARCHIVE_HEADER = (
    "# Board Archive\n\n"
    "Completed tickets, grouped by month. Newest months first. Appended by "
    "`tools/board/ticket_archive.py`.\n\n"
)


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--keep",
        type=int,
        default=10,
        help="Items to keep on the board (default 10)",
    )
    args = ap.parse_args()

    board_text = BOARD_PATH.read_text(encoding="utf-8")
    lines = board_text.splitlines(keepends=True)

    done_start: int | None = None
    done_end: int | None = None
    for i, line in enumerate(lines):
        if line.startswith(DONE_HEADER):
            done_start = i + 1
            continue
        if done_start is not None and line.startswith("## "):
            done_end = i
            break
    if done_start is None:
        print(f"error: BOARD.md missing {DONE_HEADER!r}", file=sys.stderr)
        return 1
    if done_end is None:
        done_end = len(lines)

    done_indexed: list[tuple[int, str]] = [
        (i, lines[i]) for i in range(done_start, done_end) if lines[i].startswith("- ")
    ]
    if len(done_indexed) <= args.keep:
        print(
            f"ticket_archive: {len(done_indexed)} item(s) in Done (recent); "
            f"nothing to archive (keep={args.keep})"
        )
        return 0

    def date_key(item: tuple[int, str]) -> str:
        m = DATE_SUFFIX_RE.search(item[1])
        return f"{m.group(1)}-{m.group(2)}" if m else "0000-00"

    done_indexed.sort(key=date_key)
    cutoff = len(done_indexed) - args.keep
    to_archive = done_indexed[:cutoff]

    by_month: dict[str, list[str]] = {}
    for _, line in to_archive:
        m = DATE_SUFFIX_RE.search(line)
        month = f"{m.group(1)}-{m.group(2)}" if m else "unknown"
        by_month.setdefault(month, []).append(line)

    archive_text = ARCHIVE_PATH.read_text(encoding="utf-8") if ARCHIVE_PATH.exists() else ARCHIVE_HEADER
    for month in sorted(by_month.keys(), reverse=True):
        header = f"## {month}\n"
        block = "".join(by_month[month])
        if header in archive_text:
            archive_text = archive_text.replace(header, header + block, 1)
        else:
            if not archive_text.endswith("\n\n"):
                archive_text = archive_text.rstrip("\n") + "\n\n"
            archive_text += f"{header}\n{block}\n"
    ARCHIVE_PATH.parent.mkdir(parents=True, exist_ok=True)
    ARCHIVE_PATH.write_text(archive_text, encoding="utf-8")

    archive_indices = {i for i, _ in to_archive}
    kept_lines = [line for i, line in enumerate(lines) if i not in archive_indices]
    BOARD_PATH.write_text("".join(kept_lines), encoding="utf-8")

    print(f"ticket_archive: moved {len(to_archive)} item(s) to {ARCHIVE_PATH.relative_to(BOARD_PATH.parent).as_posix()}")
    for _, line in to_archive:
        print(f"  - {line.strip()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
