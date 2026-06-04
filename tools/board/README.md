# Board Scripts

Python helpers for the kanban workflow defined in [README → Workflow](../../README.md#workflow). **Stdlib only** — no `pip install` required. Python 3.11+.

Both Claude Code and GitHub Copilot can invoke these via shell.

## Scripts

| Script | Purpose |
|---|---|
| `ticket_new.py PREFIX-NNN "Title" [--tier mechanical\|small\|design]` | Scaffold story file + append Backlog line to BOARD.md |
| `ticket_move.py PREFIX-NNN <status> [--note "reason"]` | Move ticket: update story `status:` + `updated:`, append Log, move BOARD line |
| `board_check.py` | Lint: BOARD lines ↔ story files agree on status. Exit 1 on mismatch. |
| `ticket_archive.py [--keep 10]` | Trim Done (recent) > N to `docs/board-archive.md`, grouped by year-month |

## Valid statuses

`backlog`, `next`, `in-progress`, `blocked`, `done`

## Valid prefixes (per subsystem)

| Prefix | Subsystem |
|---|---|
| `TRK` | tracking-core |
| `VIEW` | viewer |
| `CAM` | camera-node |
| `LASER` | laser-controller |
| `MAV` | mavlink-adapter |
| `DRONE` | drone |
| `DOCS` | docs (cross-cutting) |
| `META` | meta (cross-cutting workflow) |

## Examples

```bash
# Add a new ticket to the Backlog
python tools/board/ticket_new.py TRK-005 "Implement modulation detector" --tier design

# Move a ticket from Next to In Progress
python tools/board/ticket_move.py DOCS-001 in-progress

# Block a ticket with a reason
python tools/board/ticket_move.py LASER-001 blocked --note "waiting on MCU pinout"

# Mark Done
python tools/board/ticket_move.py DOCS-001 done

# Lint the board
python tools/board/board_check.py

# Archive old Done items (keep newest 10)
python tools/board/ticket_archive.py --keep 10
```

## Design notes

- Stdlib-only (`pathlib`, `argparse`, `re`, `dataclasses`, `datetime`). Adding PyYAML was rejected to keep install friction at zero on Pi 5 / Windows / any agent's shell.
- Frontmatter parsing is a hand-rolled mini-YAML reader targeting our fixed 9-field schema. Robust for that schema; will not handle arbitrary YAML.
- BOARD.md is mutated by string operations, not regenerated wholesale. This preserves any hand-edits, comments, or subheadings between automated changes.
- All scripts use repo-relative paths derived from the script location, so they work regardless of the caller's CWD.

## Known limitations

- **`_None._` placeholder management is manual.** When a column becomes empty after `ticket_move.py`, the placeholder isn't auto-added; when a ticket moves *into* an empty column, the placeholder isn't auto-removed. Tidy in BOARD.md by hand if desired.
- **Reason-suffix updates on Blocked tickets are manual.** `ticket_move.py` writes the suffix on the move *into* Blocked; subsequent reason changes require editing BOARD.md directly.
- **No concurrent-write protection.** Two parallel invocations targeting the same file race the filesystem. Mitigate by running sequentially. The planned [META-005 — Parallel Safety Check](../../docs/tickets/META-005-adopt-parallel-safety-check.md) protocol covers the broader parallel-session story.
- **U-IDs in `## Plan`** are not yet emitted by `ticket_new.py`. The scaffolded plan uses bare `1.`, `2.`. Update when [META-004](../../docs/tickets/META-004-adopt-u-ids-in-plans.md) lands.
