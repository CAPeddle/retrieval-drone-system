---
applyTo: "docs/solutions/**/*.md"
description: "Schema and conventions for solution library entries — patterns, recipes, reviews, and recovery procedures. Apply when creating or editing any file under docs/solutions/."
---
# Solutions Library Entry Schema

Each file in `docs/solutions/` captures a reusable pattern, recipe, review, or recovery procedure. The goal: a future agent (or you) can find prior work via grep on `applies_to`, `tags`, or `problem_type` rather than re-deriving it from scratch.

## Frontmatter (required)

```yaml
---
title: Short Human Title              # Required.
captured: 2026-05-31                  # Required. ISO date when entry was added.
applies_to: [tracking-core]           # Required. List of subsystems (matches README.md table), or ["*"] for cross-cutting.
tags: [agent-workflow, kanban]        # Required. Free-form list for grep/filter. Aim for 2–5 tags.
problem_type: review                  # Required. pattern | recipe | review | recovery | reference
source: https://github.com/...        # Required. Deep URL if external, "internal" if from our own work.
---
```

## Body Structure

Free-form markdown — pick what fits the content. Suggested sections:

- **Summary** — One paragraph: what this captures, when a future agent should reach for it.
- **Context** — What triggered the capture. What problem motivated it.
- **Approach** / **Pattern** — The substance.
- **Trade-offs** — What this is good for, what it's not.
- **Worked example** — Concrete instance, where applicable.
- **Source links** — Specific deep URLs (file + section, not landing pages) so a fresh agent can verify against primary sources.

## File Naming

`docs/solutions/<category>/<YYYY-MM-DD>-<slug>.md`. Date prefix gives chronology when scanning; slug is kebab-case description.

## Categories

- `workflow/` — Agent coordination, kanban tweaks, planning patterns.
- `cpp/` — Tracking-core C++ patterns and pitfalls.
- `python/` — Viewer / tooling patterns.
- `calibration/` — Calibration-specific recovery and procedures.
- `external-reviews/` — Critical reviews of external workflows or tools considered for adoption.

If a solution doesn't fit any existing category, add a new category folder rather than dumping at the solutions root.

## Index Maintenance

When adding a new solution, append a row to the index table in [`docs/solutions/README.md`](../docs/solutions/README.md). Sort by date descending.

## Distinction from ADRs

ADRs (`docs/adr/`) record architectural *decisions* — WHY we chose what we chose, with Alternatives Considered. Solutions record *patterns and learnings* — HOW to do things or what we observed.

When in doubt:

- If it would be wrong for the next developer to choose differently → **ADR**.
- If it's "here's a clean way to do X if you need to" or "here's what we learned" → **solution**.
