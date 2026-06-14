---
description: First-pass review of the current working-tree diff
agent: build
---

Diff under review:
!`git diff`

Dispatch `@reviewer` on the diff above. If it touches the `safe_for_control` predicate, ADR-007, coordinate mapping, detection, or the C++ hot path, also dispatch `@safety-reviewer`.

Consolidate the findings by severity (Blocker / Major / Minor), each with a `file:line` reference and a concrete fix.

ADR evidence rules:
- Cite ADRs by ID, and include the clause when the finding depends on a specific clause.
- Every ADR claim must name the evidence path used to verify it (ADR file, design doc, or story file).
- If you cannot support an ADR claim with a verified path, either omit it or mark the gap as a Major finding. Do not guess.

Context rules:
- Review from evidence pointers, not long pasted excerpts.
- Prefer the minimum file set needed to support each finding.
- If the diff is large, start with the files most likely to control behaviour rather than summarising everything.

End by stating whether this change should be escalated to a final Claude Code review and what specifically to re-check there. Do not edit anything — this is review only.
