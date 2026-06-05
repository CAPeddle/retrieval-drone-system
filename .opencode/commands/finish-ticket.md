---
description: Finish a ticket — verify, review, lint, mark done
agent: build
---

Ticket: $1

Do these in order, and never claim success without showing the command output that proves it (CLAUDE.md verification discipline):

1. **Verify.** Build and run the relevant tests (`cmake --build tracking-core/build` and/or `pytest`). Paste the output.
2. **Review.** Dispatch `@reviewer` on the diff. If the change touches the `safe_for_control` predicate, ADR-007, coordinate mapping (ADR-006/010), detection (ADR-005), or the C++ hot path, ALSO dispatch `@safety-reviewer`. Address Blocker and Major findings before continuing.
3. **Escalation note.** If this ticket is `design` tier or safety-critical, remind me that final sign-off belongs in **Claude Code** — list exactly what the human reviewer should re-check.
4. **Mark done** only once every acceptance criterion is met: `python tools/board/ticket_move.py $1 done --note "Acceptance criteria met"`
5. **Lint.** Run `python tools/board/board_check.py` and confirm it is clean.

Do not commit unless I explicitly ask.
