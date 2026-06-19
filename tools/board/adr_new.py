#!/usr/bin/env python3
"""Scaffold a new Architecture Decision Record and update the ADR index.

Usage:
    python tools/board/adr_new.py "Short Title"
    python tools/board/adr_new.py 011 "Short Title" [--status Accepted]

The script writes to docs/adr/ (the canonical ADR directory).
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _lib import REPO_ROOT, slugify, today  # noqa: E402

ADR_FILE_RE = re.compile(r"^ADR-(\d{3})-.+\.md$")
VALID_STATUSES = ("Proposed", "Accepted")


def adr_dir() -> Path:
    return REPO_ROOT / "docs" / "adr"


def used_numbers(directory: Path) -> set[int]:
    numbers: set[int] = set()
    for path in directory.glob("ADR-*.md"):
        match = ADR_FILE_RE.match(path.name)
        if match:
            numbers.add(int(match.group(1)))
    return numbers


def parse_number(value: str) -> int:
    cleaned = value.upper().removeprefix("ADR-")
    if not cleaned.isdigit():
        raise ValueError(f"invalid ADR number {value!r}; expected NNN or ADR-NNN")
    number = int(cleaned)
    if number <= 0 or number > 999:
        raise ValueError(f"invalid ADR number {value!r}; expected 001..999")
    return number


def split_number_and_title(args: argparse.Namespace, directory: Path) -> tuple[int, str]:
    if args.title is None:
        title = args.number_or_title
        numbers = used_numbers(directory)
        number = max(numbers, default=0) + 1
        return number, title
    return parse_number(args.number_or_title), args.title


def render_adr(number: int, title: str, status: str) -> str:
    return f"""# ADR-{number:03d}: {title}

## Status
{status} ({today()})

## Context

What is the issue motivating this decision? What forces are at play — technical, hardware, environmental, contractual? Cite related ADRs where relevant.

## Decision

We will ...

## Consequences

**Positive:**

- What becomes easier.

**Negative:**

- What becomes harder.

**Risks:**

- Specific failure modes introduced by this decision, with mitigation strategies where known.

## Alternatives Considered

- Alternative considered and why it was rejected.

## Related ADRs

- ADR-NNN
"""


def append_index_row(readme_path: Path, filename: str, number: int, title: str, status: str) -> None:
    if not readme_path.exists():
        raise FileNotFoundError(f"ADR index missing: {readme_path}")
    text = readme_path.read_text(encoding="utf-8")
    row = f"| [ADR-{number:03d}]({filename}) | {title} | {status} | {today()} |"
    if row in text:
        return
    lines = text.splitlines()
    insert_at: int | None = None
    for index, line in enumerate(lines):
        if line.startswith("| [ADR-"):
            insert_at = index + 1
    if insert_at is None:
        raise ValueError(f"cannot locate ADR index table in {readme_path}")
    lines.insert(insert_at, row)
    readme_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("number_or_title", help="ADR number (NNN) or title when auto-numbering")
    parser.add_argument("title", nargs="?", help="Human-readable title when the number is provided")
    parser.add_argument("--status", choices=VALID_STATUSES, default="Proposed")
    args = parser.parse_args()

    directory = adr_dir()
    directory.mkdir(parents=True, exist_ok=True)
    number, title = split_number_and_title(args, directory)
    slug = slugify(title)
    if not slug:
        print(f"error: title produced an empty slug: {title!r}", file=sys.stderr)
        return 1
    numbers = used_numbers(directory)
    if number in numbers:
        print(f"error: ADR-{number:03d} already exists in {directory}", file=sys.stderr)
        return 1

    filename = f"ADR-{number:03d}-{slug}.md"
    path = directory / filename
    if path.exists():
        print(f"error: ADR file already exists: {path}", file=sys.stderr)
        return 1

    path.write_text(render_adr(number, title, args.status), encoding="utf-8")
    append_index_row(directory / "README.md", filename, number, title, args.status)

    print(f"created: {path.relative_to(REPO_ROOT).as_posix()}")
    print(f"updated: {(directory / 'README.md').relative_to(REPO_ROOT).as_posix()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())