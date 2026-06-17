---
description: Show available OpenCode commands for this repo
agent: build
---

Available slash commands:

| Command | Use when | Purpose |
|---|---|---|
| `/board` | You need current kanban state | Run `board_check.py`, show BOARD.md, and summarise active work. |
| `/next-ticket` | You are ready to pick work | Find the next ready ticket whose `depends_on:` entries are done. |
| `/start-ticket ID` | You are about to execute a ticket | Tier-gate, run `@explore`, move the ticket to `in-progress`, then pause for go-ahead. |
| `/finish-ticket ID` | Implementation is complete | Verify, review, move the ticket to `done`, and lint the board. |
| `/ticket-archive` | Done (recent) is getting noisy | Run `ticket_archive.py --keep 10` and summarise archived tickets. |
| `/review` | You want a first-pass diff review | Dispatch `@reviewer`, and `@safety-reviewer` when safety-critical areas are touched. |
| `/agents` | You want the subagent menu | List `@explore`, `@researcher`, `@reviewer`, and `@safety-reviewer`. |
| `/help` | You are cold-starting in OpenCode | Show this command index. |

Workflow source of truth: AGENTS.md "Workflow at a glance". Story files remain the ticket source of truth; BOARD.md is only the index.