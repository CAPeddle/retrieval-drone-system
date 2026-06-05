#!/usr/bin/env python3
"""Scaffold a solution-library entry and update docs/solutions/README.md.

Usage:
    python tools/board/solution_new.py "Short Title" --category workflow --applies-to tracking-core --tags workflow,handoff
    python tools/board/solution_new.py --title "Short Title" --category external-reviews --problem-type review --source https://example.test
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from _lib import REPO_ROOT, slugify, today  # noqa: E402

SOLUTIONS_DIR = REPO_ROOT / "docs" / "solutions"
VALID_PROBLEM_TYPES = ("pattern", "recipe", "review", "recovery", "reference")


def csv_list(value: str) -> list[str]:
    return [item.strip() for item in value.split(",") if item.strip()]


def yaml_list(values: list[str]) -> str:
    return "[" + ", ".join(values) + "]"


def render_solution(title: str, applies_to: list[str], tags: list[str], problem_type: str, source: str) -> str:
    return f"""---
title: {title}
captured: {today()}
applies_to: {yaml_list(applies_to)}
tags: {yaml_list(tags)}
problem_type: {problem_type}
source: {source}
---

## Summary

One paragraph: what this captures, and when a future agent should reach for it.

## Context

What triggered the capture. What problem motivated it.

## Approach

The pattern, recipe, review, or recovery procedure.

## Trade-offs

What this is good for, and what it is not.

## Source Links

- Specific deep URL or internal file reference.
"""


def append_index_row(readme_path: Path, relative_file: str, problem_type: str, applies_to: list[str], tags: list[str]) -> None:
    if not readme_path.exists():
        raise FileNotFoundError(f"solutions index missing: {readme_path}")
    text = readme_path.read_text(encoding="utf-8")
    display = relative_file.removeprefix("docs/solutions/")
    row = (
        f"| {today()} | [{display}]({display}) | {problem_type} | "
        f"{', '.join(applies_to)} | {', '.join(tags)} |"
    )
    if row in text:
        return
    marker = "|---|---|---|---|---|"
    if marker not in text:
        raise ValueError(f"cannot locate solutions index table in {readme_path}")
    text = text.replace(marker, marker + "\n" + row, 1)
    readme_path.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("positional_title", nargs="?", help="Human-readable title")
    parser.add_argument("--title", dest="flag_title", help="Human-readable title")
    parser.add_argument("--category", required=True, help="Existing docs/solutions category")
    parser.add_argument("--applies-to", default="*", help="Comma-separated subsystem list, default '*' ")
    parser.add_argument("--tags", required=True, help="Comma-separated tags, usually 2-5")
    parser.add_argument("--problem-type", choices=VALID_PROBLEM_TYPES, default="recipe")
    parser.add_argument("--source", default="internal", help="Deep URL or 'internal'")
    args = parser.parse_args()

    title = args.flag_title or args.positional_title
    if not title:
        print("error: provide a title as a positional argument or --title", file=sys.stderr)
        return 1
    if args.flag_title and args.positional_title:
        print("error: provide the title only once", file=sys.stderr)
        return 1

    category_dir = SOLUTIONS_DIR / args.category
    if not category_dir.is_dir():
        print(f"error: solution category does not exist: {category_dir}", file=sys.stderr)
        return 1

    applies_to = csv_list(args.applies_to)
    tags = csv_list(args.tags)
    if not applies_to:
        print("error: --applies-to produced an empty list", file=sys.stderr)
        return 1
    if not tags:
        print("error: --tags produced an empty list", file=sys.stderr)
        return 1

    slug = slugify(title)
    if not slug:
        print(f"error: title produced an empty slug: {title!r}", file=sys.stderr)
        return 1

    path = category_dir / f"{today()}-{slug}.md"
    if path.exists():
        print(f"error: solution file already exists: {path}", file=sys.stderr)
        return 1

    path.write_text(
        render_solution(title, applies_to, tags, args.problem_type, args.source),
        encoding="utf-8",
    )
    relative_file = path.relative_to(SOLUTIONS_DIR).as_posix()
    append_index_row(SOLUTIONS_DIR / "README.md", relative_file, args.problem_type, applies_to, tags)

    print(f"created: {path.relative_to(REPO_ROOT).as_posix()}")
    print("updated: docs/solutions/README.md")
    return 0


if __name__ == "__main__":
    sys.exit(main())