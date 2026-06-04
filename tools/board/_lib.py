"""Shared helpers for board scripts.

Stdlib only. No pip install required. Python 3.11+.
"""
from __future__ import annotations

import re
from dataclasses import dataclass, field
from datetime import date
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
BOARD_PATH = REPO_ROOT / "BOARD.md"
TICKETS_DIR = REPO_ROOT / "docs" / "tickets"
ARCHIVE_PATH = REPO_ROOT / "docs" / "board-archive.md"

PREFIX_TO_SUBSYSTEM = {
    "TRK": "tracking-core",
    "VIEW": "viewer",
    "CAM": "camera-node",
    "LASER": "laser-controller",
    "MAV": "mavlink-adapter",
    "DRONE": "drone",
    "DOCS": "docs",
    "META": "meta",
}

# Order matters when rendering BOARD.md (columns appear top to bottom in this order).
COLUMN_NAMES = ["Next", "In Progress", "Blocked", "Done (recent)", "Backlog"]
COLUMN_TO_STATUS = {
    "Next": "next",
    "In Progress": "in-progress",
    "Blocked": "blocked",
    "Done (recent)": "done",
    "Backlog": "backlog",
}
STATUS_TO_COLUMN = {v: k for k, v in COLUMN_TO_STATUS.items()}

# Columns where a story file is required (rather than optional).
STATUSES_REQUIRING_STORY = {"next", "in-progress", "blocked"}

TICKET_ID_RE = re.compile(r"^([A-Z]+)-(\d+)$")
TICKET_LINE_RE = re.compile(
    r"^- \[([ x])\] ([A-Z]+-\d+) — (.+?)"
    r"(?:\s+→\s+\[story\]\((.+?)\))?"
    r"(\s+\(.+?\))?\s*$"
)
COLUMN_HEADER_RE = re.compile(r"^##\s+(.+?)\s*$")
SUBHEADER_RE = re.compile(r"^###\s+(.+?)\s*$")


@dataclass
class TicketLine:
    raw: str
    checked: bool
    ticket_id: str
    title: str
    story_link: str | None
    suffix: str | None
    column: str
    subheader: str | None = None


@dataclass
class Frontmatter:
    id: str | None = None
    status: str | None = None
    subsystem: str | None = None
    tier: str | None = None
    created: str | None = None
    updated: str | None = None
    spec: str | None = None
    plan: str | None = None
    blockers: list[str] = field(default_factory=list)


def today() -> str:
    return date.today().isoformat()


def slugify(title: str) -> str:
    s = re.sub(r"[^A-Za-z0-9\s-]", "", title).strip().lower()
    s = re.sub(r"\s+", "-", s)
    s = re.sub(r"-+", "-", s)
    return s


def parse_frontmatter(text: str) -> tuple[Frontmatter, str]:
    """Parse the fixed-schema YAML frontmatter we use in story files.

    Returns (Frontmatter, body_text). Body includes the leading newline after `---`.
    """
    if not text.startswith("---\n"):
        raise ValueError("missing frontmatter opener `---`")
    end = text.find("\n---\n", 4)
    if end == -1:
        # Last-line `---` with no trailing newline
        end = text.find("\n---", 4)
        if end == -1 or end + 4 != len(text.rstrip()):
            raise ValueError("missing frontmatter terminator `---`")
        body = ""
    else:
        body = text[end + 5:]
    fm_text = text[4:end]
    raw: dict[str, object] = {}
    for line in fm_text.splitlines():
        line = line.rstrip()
        if not line or line.lstrip().startswith("#"):
            continue
        if ":" not in line:
            continue
        key, _, value = line.partition(":")
        key = key.strip()
        value = value.strip()
        if value == "null" or value == "":
            raw[key] = None
        elif value.startswith("[") and value.endswith("]"):
            inner = value[1:-1].strip()
            raw[key] = [x.strip() for x in inner.split(",") if x.strip()] if inner else []
        else:
            raw[key] = value
    fm = Frontmatter(
        id=raw.get("id"),
        status=raw.get("status"),
        subsystem=raw.get("subsystem"),
        tier=raw.get("tier"),
        created=raw.get("created"),
        updated=raw.get("updated"),
        spec=raw.get("spec"),
        plan=raw.get("plan"),
        blockers=raw.get("blockers") or [],
    )
    return fm, body


def render_frontmatter(fm: Frontmatter) -> str:
    """Render a Frontmatter back to YAML in the canonical field order."""
    def render(value: object) -> str:
        if value is None:
            return "null"
        if isinstance(value, list):
            return "[" + ", ".join(str(x) for x in value) + "]"
        return str(value)

    lines = ["---"]
    for key in ("id", "status", "subsystem", "tier", "created", "updated", "spec", "plan", "blockers"):
        lines.append(f"{key}: {render(getattr(fm, key))}")
    lines.append("---")
    return "\n".join(lines) + "\n"


def find_story_file(ticket_id: str) -> Path:
    """Locate the unique story file for a ticket ID. Raises if zero or multiple."""
    matches = sorted(TICKETS_DIR.glob(f"{ticket_id}-*.md"))
    if not matches:
        raise FileNotFoundError(f"no story file for {ticket_id} in {TICKETS_DIR}")
    if len(matches) > 1:
        raise RuntimeError(f"multiple story files for {ticket_id}: {[p.name for p in matches]}")
    return matches[0]


def parse_board(text: str) -> list[TicketLine]:
    """Parse BOARD.md into a list of TicketLine, each tagged with its column."""
    tickets: list[TicketLine] = []
    current_column: str | None = None
    current_sub: str | None = None
    for line in text.splitlines():
        col_match = COLUMN_HEADER_RE.match(line)
        if col_match:
            heading = col_match.group(1).strip()
            current_column = heading if heading in COLUMN_TO_STATUS else None
            current_sub = None
            continue
        sub_match = SUBHEADER_RE.match(line)
        if sub_match and current_column:
            current_sub = sub_match.group(1).strip()
            continue
        if current_column:
            m = TICKET_LINE_RE.match(line)
            if m:
                tickets.append(
                    TicketLine(
                        raw=line,
                        checked=m.group(1) == "x",
                        ticket_id=m.group(2),
                        title=m.group(3).strip(),
                        story_link=m.group(4),
                        suffix=m.group(5).strip() if m.group(5) else None,
                        column=current_column,
                        subheader=current_sub,
                    )
                )
    return tickets
