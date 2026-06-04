---
applyTo: "BOARD.md"
description: "Rules for editing the project kanban board. Apply whenever BOARD.md is modified — moving tickets between columns, adding new tickets, archiving Done items."
---
# BOARD.md Editing Rules

The board is a thin index. Ticket detail lives in `docs/tickets/<ID>-<slug>.md`. See [tickets.instructions.md](tickets.instructions.md) for the story file schema.

**Story file requirement:**

- **Next, In Progress, Blocked** lines MUST have a corresponding story file whose frontmatter `status:` matches the column.
- **Backlog** lines MAY be one-line placeholders without a story file. Create the story file when promoting to Next.
- **Done (recent)** lines do not need a `→ [story]` link on the board (the date suffix `(YYYY-MM-DD)` takes its place). If a story file exists from when the ticket was active, leave it; its frontmatter `status:` should read `done`.

Run `python tools/board/board_check.py` to verify consistency.

## Prefer the scripts

[`tools/board/ticket_new.py`](../../tools/board/ticket_new.py), [`tools/board/ticket_move.py`](../../tools/board/ticket_move.py), and [`tools/board/ticket_archive.py`](../../tools/board/ticket_archive.py) perform the operations below atomically (story file + BOARD.md updated together, status front-matter and `## Log` entry kept in sync). **Hand-edit only when the change can't be expressed through them.** Full reference in [`tools/board/README.md`](../../tools/board/README.md).

## Allowed Edits

| Operation | What changes |
|---|---|
| Add ticket | One new line in Backlog. Also create the matching story file. |
| Move ticket | Cut one line, paste in target column. Also update the story file's `status:` front-matter and append a Log entry. |
| Update spec/plan link | Append/update `→ [story](...)` suffix on a single ticket line. |
| Archive Done | Cut oldest Done lines, append them to `docs/board-archive.md` with their original `(YYYY-MM-DD)` date suffix. |

## Forbidden Edits

- **No inline detail.** Acceptance criteria, notes, blockers, history — all go in the story file. The BOARD is one line per ticket.
- **No column reordering.** Columns are fixed in this order: Next, In Progress, Blocked, Done (recent), Backlog. Do not rename them.
- **No bulk edits.** One ticket transition per commit. Bulk edits break parallel-session merges and obscure history.
- **No status drift.** If BOARD.md shows a ticket as In Progress, the story file's `status:` must say `in-progress`. Run `tools/board/board_check.py` if it exists.
- **No deletion.** Tickets do not disappear. Completed tickets move through Done → archive. Cancelled tickets get `status: done` with a Log entry explaining the cancellation.

## Line Format

Active tickets (Next, In Progress, Backlog):

```
- [ ] PREFIX-NNN — Short title → [story](docs/tickets/PREFIX-NNN-slug.md)
```

Blocked tickets — append a parenthetical reason:

```
- [ ] PREFIX-NNN — Short title → [story](docs/tickets/PREFIX-NNN-slug.md) (waiting on X)
```

Done tickets — `[x]` with date suffix, no story link required on the line (it stays in the story file):

```
- [x] PREFIX-NNN — Short title (2026-05-31)
```

## Concurrent Edit Resolution

If BOARD.md has merge conflicts because two agents moved **different** tickets concurrently: accept both moves — they are additive. The board's column structure is intentionally line-oriented so this merge is mechanical.

If two agents moved the **same** ticket: the later commit wins on board position, and both agents must reconcile the corresponding story file's `## Log` so the history is complete.

## When in Doubt

If a change would require more than a single-line edit, stop. Either the change belongs in the story file, or it should be split into multiple commits.
