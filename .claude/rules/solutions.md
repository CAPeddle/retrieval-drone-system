---
description: Schema and conventions for docs/solutions/ library entries (patterns, recipes, reviews, recovery procedures). Applies when creating or editing solution files.
paths:
  - "docs/solutions/**/*.md"
---

# Solutions Library

Each file in `docs/solutions/` captures a reusable pattern, recipe, review, or
recovery procedure so a future agent can find prior work via grep on
`applies_to`, `tags`, or `problem_type` rather than re-deriving it.

## Frontmatter (required)

```yaml
---
title: Short Human Title              # Required.
captured: 2026-05-31                  # Required. ISO date the entry was added.
applies_to: [tracking-core]           # Required. Subsystems (matches README), or ["*"] for cross-cutting.
tags: [agent-workflow, kanban]        # Required. 2–5 free-form tags for grep/filter.
problem_type: review                  # Required. pattern | recipe | review | recovery | reference
source: https://github.com/...        # Required. Deep URL if external, "internal" if from our own work.
---
```

## Body

Free-form markdown. Suggested sections: **Summary** (what this captures, when to
reach for it), **Context** (what triggered it), **Approach/Pattern** (the
substance), **Trade-offs**, **Worked example**, **Source links** (deep URLs to
file + section, not landing pages).

## File naming & categories

`docs/solutions/<category>/<YYYY-MM-DD>-<slug>.md`. Categories: `workflow/`,
`cpp/`, `python/`, `calibration/`, `external-reviews/`. If a solution fits none,
add a new category folder rather than dumping at the root.

## Index & distinction from ADRs

When adding a solution, append a row to the index table in
`docs/solutions/README.md`, sorted by date descending.

ADRs (`docs/adr/`) record architectural *decisions* (WHY, with Alternatives
Considered). Solutions record *patterns and learnings* (HOW / what we observed).
When in doubt: if it would be wrong for the next developer to choose differently
→ ADR; if it's "here's a clean way to do X" or "here's what we learned" →
solution.
