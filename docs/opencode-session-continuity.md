# OpenCode Session Continuity Scaffold

`.remember/` is local scratch space and is ignored by git. Keep it that way: it can hold machine-specific notes, but committed OpenCode startup must not depend on files that will be absent on the next machine.

Use this committed scaffold to recreate local continuity notes after cloning the project elsewhere.

## Local Files

Create these files under `.remember/` when useful:

```text
.remember/
  now.md                  Current in-progress handoff, usually short-lived
  recent.md               Rolling cross-session notes worth seeing first
  today-YYYY-MM-DD.md     Timestamped notes for the current workday
```

## `now.md` Template

```markdown
# Now

## Current Goal

- 

## Active Ticket

- ID: 
- Status: 
- Last completed U-ID: 

## Next Action

- 

## Validation State

- 
```

## `recent.md` Template

```markdown
# Recent

## YYYY-MM-DD

- Decision or workflow note that should survive a session restart.
- Link to committed source when possible: ticket, ADR, solution, or startup doc.
```

## Daily Note Template

```markdown
## HH:MM | branch-name

One-line note: what changed, what was decided, or what remains blocked.
```

## Use In OpenCode

- Start with committed state first: `AGENTS.md`, `docs/opencode-startup.md`, `BOARD.md`, and story files.
- Read `.remember/` only as local context. Treat it as helpful scratch, never as source of truth.
- Promote anything durable into a committed artifact: a story-file log entry, an ADR, a solution doc, or this startup documentation.