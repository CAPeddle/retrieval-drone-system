---
id: META-010
status: done
subsystem: meta
tier: small
created: 2026-06-05
updated: 2026-06-05
spec: null
plan: null
blockers: []
---
## Context

The agent-native audit found that tickets and the board have Create tooling (`ticket_new.py`) but the other frequently-authored Markdown entities do not. ADRs and solution-library docs are created by hand against a documented template, which invites numbering mistakes, missing required sections, and stale index tables. This is the highest-value gap the audit surfaced that is not an intentional design choice: agents capture decisions and learnings often, and hand-formatting the templates is real friction. Two stdlib-only scripts that mirror the existing board-script pattern close it.

## Acceptance

- `tools/board/adr_new.py NNN "Title"` (or auto-selecting the next free number) creates an `ADR-NNN-title-with-hyphens.md` file in the canonical ADR directory with the full Nygard template from CLAUDE.md §8.2 (Status, Context, Decision, Consequences, Alternatives Considered, Related ADRs).
- `adr_new.py` refuses to reuse or overwrite an existing ADR number and appends a row to the ADR `README.md` index table.
- `tools/board/solution_new.py` creates a solution doc under `docs/solutions/<category>/` with the frontmatter schema from `.github/instructions/solutions.instructions.md` and appends a row to `docs/solutions/README.md`.
- Both scripts are stdlib-only (no `pip install`), use repo-relative paths derived from the script location, and run on Windows and Pi 5.
- `tools/board/README.md` documents both scripts in the Scripts table with usage examples.

## Plan

U1. Read CLAUDE.md §8.2/§8.4 (ADR template, numbering rules) and `.github/instructions/solutions.instructions.md` (solution frontmatter schema) to pin the exact output formats.
U2. Write `tools/board/adr_new.py`: resolve canonical ADR dir (`docs/adr/`), compute next free number, render template, write file, append README index row. Block on collision.
U3. Write `tools/board/solution_new.py`: accept `--title`, `--category`, `--applies-to`, `--tags`; render frontmatter + section skeleton; write to dated slug path; append README index row. Validate category folder exists.
U4. Add both scripts to the Scripts table and Examples in `tools/board/README.md`.
U5. Manual smoke test: generate one throwaway ADR and one throwaway solution doc, confirm index rows and section completeness, then delete the throwaways.

## Log

- 2026-06-05: created. Status: backlog. Source: agent-native audit CRUD-completeness finding (highest-ROI gap).
- 2026-06-05: backlog → done. ADR and solution scaffolding scripts added and documented.
