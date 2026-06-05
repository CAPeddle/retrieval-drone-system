# Session Decision Log — 2026-05-31 / 2026-06-04

**Session ID:** `f5e37142-9fa3-4d53-82b5-92b6213deb8c`
**Dates:** 2026-05-31 (planning) → 2026-06-04 (implementation + commit)
**Scope:** Everything from the first message through the commit run.

---

## How to use this template

> Copy this file for each new session. Fill in the header, then record each decision point as it occurs.
> Use the table for quick scanning; the detail section for full context.
> Capture questions regardless of channel — VS Code question tool, in-prose checkpoints, or user pivots.

---

## Quick-reference table

| # | Date | Channel | Question / Decision point | Answer / Outcome |
|---|------|---------|---------------------------|------------------|
| D-01 | 2026-05-31 | VS Code question tool | Planning scope for this session | Everything — cleanup through future phases |
| D-02 | 2026-05-31 | VS Code question tool | Skip existing cleanup tickets? | Yes — focus on v0.3 and beyond |
| D-03 | 2026-05-31 | VS Code question tool | v0.3 ticket granularity | Fine (20+ tickets, each a few hours) |
| D-04 | 2026-05-31 | VS Code question tool | Phase 3+ planning depth | Full stories where possible |
| D-05 | 2026-05-31 | VS Code question tool | Dependency ordering strategy | Parallel-ready (explicit deps, work multiple fronts) |
| D-06 | 2026-05-31 | VS Code question tool | v0.3 breakdown grouping: accept or adjust? | Adjust — user selected "Adjust grouping" |
| D-07 | 2026-05-31 | VS Code question tool | Free-text grouping feedback | Move calibration tools before coordinate mapping layer |
| D-08 | 2026-05-31 | VS Code question tool | Confirm reordered sequence | Move calibration tools earlier — confirmed |
| D-09 | 2026-05-31 | VS Code question tool | Phase 3+ scope: comprehensive enough? | Looks comprehensive — proceed |
| D-10 | 2026-05-31 | VS Code question tool | Robustness improvements: which to adopt? | All four proposed improvements |
| D-11 | 2026-06-01 | User prompt (no answer to prior) | TRK-001 identified as blocking TRK-002 | No explicit confirmation — agent proceeded; user asked "progress?" |
| D-12 | 2026-06-01 | In-prose checkpoint (not question tool) | TRK-002 complete — review or continue? | No direct answer — user pivoted: "Update documentation, run compound flow" |
| D-13 | 2026-06-04 | User directive | Commit all files in conventional commits | Directive accepted; agent produced 6-commit stack |
| D-14 | 2026-06-04 | VS Code question tool | Session decision log format (4 sub-questions) | Plain markdown, full session scope, paraphrased, reusable template |

---

## Detail entries

---

### D-01 — Planning scope

**Date:** 2026-05-31
**Channel:** VS Code question tool
**Question:** What's the primary focus for this planning session?
**Options presented:**
- v0.3 Implementation Roadmap
- Full Horizon (v0.3 + Phase 3+ skeleton)
- Everything — cleanup through future phases *(recommended)*

**Answer:** Everything — cleanup through future phases

**Outcome:** Agent proceeded to plan all phases (v0.3, Phase 3/4/5) including operational tickets.

---

### D-02 — Skip existing cleanup tickets

**Date:** 2026-05-31
**Channel:** VS Code question tool
**Question:** The 3 "Next" cleanup tickets (DOCS-001, DOCS-002, TRK-001) already have plans. Should planning effort focus on what comes after them?
**Options presented:**
- Yes — skip cleanup, plan from v0.3 implementation onward *(recommended)*
- No — review and adjust cleanup plans too

**Answer:** Yes — skip cleanup, plan from v0.3 implementation onward

**Outcome:** Cleanup tickets left as-is; agent focused story-writing effort on TRK-002 and beyond.

---

### D-03 — v0.3 ticket granularity

**Date:** 2026-05-31
**Channel:** VS Code question tool
**Question:** For v0.3 implementation tickets, how granular should stories be?
**Options presented:**
- Coarse (5–8 major milestones)
- Medium (10–15 tickets, 1–2 day scope) *(recommended)*
- Fine (20+ tickets, each a few hours)

**Answer:** Fine (20+ tickets, each a few hours)

**Outcome:** Agent created 27 v0.3 story files with detailed U-ID plans. This was the maximum token spend per the user's stated objective ("maximise useful output before context fills").

---

### D-04 — Phase 3+ planning depth

**Date:** 2026-05-31
**Channel:** VS Code question tool
**Question:** How detailed should the future-phase planning be?
**Options presented:**
- Milestone sketch only
- Ticket outlines (acceptance criteria, no implementation plans) *(recommended)*
- Full stories where possible

**Answer:** Full stories where possible

**Outcome:** Agent wrote full story files (Context + Acceptance + Plan) for all Phase 3/4/5 tickets.

---

### D-05 — Dependency ordering strategy

**Date:** 2026-05-31
**Channel:** VS Code question tool
**Question:** Should dependency order assume serial execution (one dev) or parallel-ready (explicit deps, multi-front)?
**Options presented:**
- Serial — strict linear order, single developer *(recommended)*
- Parallel-ready — explicit deps, work multiple fronts

**Answer:** Parallel-ready — explicit deps, work multiple fronts

**Outcome:** `depends_on:` frontmatter populated on all 46 story files with explicit dependency IDs; dependency graph created in `docs/dependency-graph.md`.

---

### D-06 — v0.3 grouping: accept or adjust

**Date:** 2026-05-31
**Channel:** VS Code question tool
**Question:** Agent presented a 7-layer grouping of ~25 v0.3 tickets. Accept or adjust?
**Preview shown:**
- Foundation (4), Ingestion (2), Detection (4), Tracking (3), Coordinate Mapping (3), State Export (4), Calibration & Testing (5), Viewer (2)

**Options presented:**
- Looks good — proceed to writing stories *(recommended)*
- Adjust grouping — let me give feedback first

**Answer:** Adjust grouping — let me give feedback first

**Outcome:** Agent asked follow-up question D-07 before writing any stories.

---

### D-07 — Free-text grouping feedback

**Date:** 2026-05-31
**Channel:** VS Code question tool
**Question:** What would you like adjusted in the v0.3 breakdown?
**Format:** Free text (no options)

**Answer:** User requested calibration tools be moved earlier in the sequence — before the coordinate mapping layer, not after. Rationale: the coordinate mapper depends on calibration output files, so the tools that produce those files should come first.

**Outcome:** Agent reordered the sequence to place TRK-012/013 (intrinsic/extrinsic calibration tools) after detection but before homography loader and Z compensation.

---

### D-08 — Confirm reordered sequence

**Date:** 2026-05-31
**Channel:** VS Code question tool
**Question:** Does this revised serial sequence work, or should anything else be reordered?
**Options presented:**
- Proceed with this order *(recommended)*
- Move calibration tools earlier (before coord mapping)
- Other — type your adjustment

**Answer:** Move calibration tools earlier (before coord mapping) — this was selected confirming the feedback from D-07.

**Outcome:** Sequence locked. Agent wrote all story files in this order.

---

### D-09 — Phase 3+ scope

**Date:** 2026-05-31
**Channel:** VS Code question tool
**Question:** How does the Phase 3+ scope look?
**Preview shown:** Phase 3 (4 tickets), Phase 4 (4 tickets), Phase 5 (2 tickets), Operational (2 tickets)
**Options presented:**
- Looks comprehensive — proceed *(recommended)*
- Add more Phase 3+ items — I have ideas
- Slim down — fewer future tickets, more detail on near-term

**Answer:** Looks comprehensive — proceed

**Outcome:** Agent wrote all 12 Phase 3–5 + operational story files as scoped.

---

### D-10 — Robustness improvements

**Date:** 2026-05-31
**Channel:** VS Code question tool
**Question:** Agent proposed four improvements to make the work durable without session memory. Which to adopt?
**Options proposed (all four):**
1. Populate `depends_on:` frontmatter on all story files
2. Create repo memory at `/memories/repo/conventions.md`
3. Create `docs/v0.3-execution-guide.md` (standalone execution guide)
4. Update `tickets.instructions.md` with U-ID and `depends_on` conventions

**Answer:** All four

**Outcome:** All four robustness improvements were executed in the same session; each is a committed file in the repository.

---

### D-11 — TRK-001 prerequisite decision

**Date:** 2026-06-01
**Channel:** Agent implicit + user prompt
**Question:** No explicit question asked. Agent identified TRK-001 (rename `tracking-system` → `tracking-core`) as a blocking prerequisite for TRK-002 and stated it would execute TRK-001 first.
**User turn:** User asked "progress?" — not an answer to a question, a status request.

**Answer:** No direct confirmation given. Agent explained it had moved TRK-001 to in-progress but not yet executed, then proceeded immediately.

**Outcome:** TRK-001 executed in full on the same turn. All path references updated across 11 files; `applyTo:` globs corrected; ticket marked done.

---

### D-12 — TRK-002 checkpoint

**Date:** 2026-06-01
**Channel:** In-prose checkpoint (not VS Code question tool)
**Question (paraphrased):** "TRK-002 complete — all acceptance criteria met (modulo the environment-blocked build test). Review the changes, or continue to next ticket?"

**Answer:** No direct answer given. User provided a different directive instead: "Update the relevant documentation for this work, run the compound learning/engineering flow."

**Note:** This is a **pivot** — the checkpoint question was bypassed entirely by a new instruction. The agent should treat this as implicit approval of TRK-002's implementation state while redirecting to documentation tasks.

**Outcome:** Agent ran compound documentation flow: TRK-002 moved to `blocked` (validation blocker), solutions library entry created, board and ticket story updated.

---

### D-13 — Commit directive

**Date:** 2026-06-04
**Channel:** User directive (no question asked by agent, no answer expected)
**Instruction:** "Commit all files, as best in conventional commits with appropriate messages where possible. You might have to delegate lower tier explore sub agents to investigate batches of files."

**Answer:** N/A — directive with no question preceding it.

**Outcome:** Agent dispatched Explore subagent to propose batching, then created 6 conventional commits:
1. `refactor(tracking-core): rename subsystem and modernise cmake scaffolding`
2. `docs(agent): add cross-agent operating contracts and copilot guidance`
3. `docs(instructions): add board/ticket/solution schemas and path updates`
4. `chore(board): add kanban index and automation scripts`
5. `docs(planning): add ticket backlog, execution guides, and solutions library`
6. `chore(archive): add synthesised adr bundle snapshot`

---

### D-14 — Session decision log format

**Date:** 2026-06-04
**Channel:** VS Code question tool (4 questions)
**Context:** User requested this document; agent asked clarifying questions before writing.

| Sub-question | Options | Answer |
|---|---|---|
| Schema | Solutions frontmatter / Plain markdown / Custom | Plain markdown, no frontmatter |
| Scope | Full session / Since TRK-001 / Today only | Everything from the very first message |
| Detail level | Verbatim / Paraphrased / Both | Paraphrased summaries |
| Reuse intent | One-off / Reusable template | Reusable template |

**Outcome:** This document.

---

## Template notes for future sessions

- **One entry per decision point.** Not one per message.
- **Channel matters.** Distinguish VS Code question tool from prose checkpoints from user pivots — they carry different confirmation weights.
- **Record pivots explicitly.** When a user provides a new directive instead of answering a checkpoint question (like D-12), that is a pivot. Note it as such. The checkpoint question was not answered; the implicit signal is "proceed, but redirect."
- **No answer ≠ rejection.** Silence or a new directive usually means implicit approval + redirect, not disagreement.
- **Copy the table row template:** `| D-NN | YYYY-MM-DD | [channel] | [question] | [answer / outcome] |`
