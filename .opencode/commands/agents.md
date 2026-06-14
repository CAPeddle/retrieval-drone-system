---
description: Show available OpenCode subagents for this repo
agent: build
---

Available subagents:

| Agent | Scope | Dispatch when |
|---|---|---|
| `@explore` | Read-only, no shell, no web | Pre-flight a ticket: goal, acceptance criteria, U-ID plan steps, ADRs in play, files in scope, dependencies, pitfalls, and open questions. |
| `@researcher` | Read-only, web allowed, no shell | You need external API or best-practice grounding before choosing an implementation approach. |
| `@reviewer` | Read-only, no shell, no web | Review a diff for correctness, maintainability, tests, ADR conformance, and C++ hot-path discipline from CLAUDE.md §7. |
| `@safety-reviewer` | Read-only, no shell, no web | Review work touching ADR-005, ADR-006, ADR-007, ADR-010, calibration, detection, coordinate mapping, or physical-environment safety. |

Use `@reviewer` for routine first-pass review. Add `@safety-reviewer` whenever the cardinal failure mode is in play: a false positive on `safe_for_control` under ADR-007.