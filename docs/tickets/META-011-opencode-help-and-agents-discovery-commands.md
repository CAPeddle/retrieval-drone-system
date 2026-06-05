---
id: META-011
status: backlog
subsystem: meta
tier: small
created: 2026-06-05
updated: 2026-06-05
depends_on: []
spec: null
plan: null
blockers: []
---
## Context

The agent-native audit scored capability discovery 5/7: the OpenCode scaffold is well-documented across [docs/opencode-startup.md](../opencode-startup.md) and [AGENTS.md](../../AGENTS.md), but it is not self-advertising. A user dropped into the OpenCode TUI cold sees no hint of `/board`, `/next-ticket`, `/start-ticket`, `/finish-ticket`, `/review`, or the four subagents. Two small command files make the existing capabilities discoverable from inside the tool without reading external docs.

## Acceptance

- `.opencode/commands/help.md` exists and lists every slash command (`/board`, `/next-ticket`, `/start-ticket`, `/finish-ticket`, `/review`, `/help`, `/agents`) with a one-line purpose and when to use it, plus a pointer to AGENTS.md "Workflow at a glance".
- `.opencode/commands/agents.md` exists and describes the four subagents (`@explore`, `@researcher`, `@reviewer`, `@safety-reviewer`): role, read-only/edit scope, and when to dispatch each.
- Both files carry a `description:` frontmatter field consistent with the existing command files.
- The command list in both files matches the actual files in `.opencode/commands/` (no drift) at time of writing.

## Plan

U1. Enumerate current `.opencode/commands/*.md` and `.opencode/agents/*.md` to source accurate names and descriptions.
U2. Write `.opencode/commands/help.md`: frontmatter + table of commands with purpose and trigger, ending with the AGENTS.md pointer.
U3. Write `.opencode/commands/agents.md`: frontmatter + table of subagents with scope and dispatch guidance, citing the relevant CLAUDE.md sections (§2 Review mode, §7 hot-path).
U4. Cross-check both against the live files so the listings do not drift.

## Log

- 2026-06-05: created. Status: backlog. Source: agent-native audit capability-discovery finding (5/7).
