# CLAUDE.md Modularization Design

## Overview

Split the monolithic CLAUDE.md (~870 lines) into a Claude-native modular
structure enabling incremental discovery. The goal: reduce per-session context
overhead while preserving all guidance.

**Target structure** (consolidated per D10 — resolves the fan-out concern):
- `CLAUDE.md` (~210 lines): Always-read core — identity, safety, workflow, dispatch
- `.claude/rules/` (3 files): Path-scoped, auto-applied when editing matching files
- `.claude/skills/` (2 files): Invoked on demand or by model
- `docs/agent-reference/` (1 file): Detailed reference material, consulted on demand

## Design Decisions

### D1: Discovery Mechanism — Hybrid

Use Claude-native mechanisms matched to content type:

| Content Type | Mechanism | Rationale |
|--------------|-----------|-----------|
| Coding standards, architecture boundaries | `.claude/rules/` with `paths` | Auto-applies when editing relevant paths |
| Multi-step procedures | `.claude/skills/` | Explicit invocation for workflows |
| Reference material | `docs/agent-reference/` | Consulted on demand, not loaded by default |
| Safety contract, identity, dispatch | `CLAUDE.md` | Always-read, non-negotiable |

### D2: Single-System — Claude Code (supersedes the cross-vendor setup)

**Updated 2026-07-06.** The project is managed by Claude Code going forward.
`CLAUDE.md` + `.claude/rules/` + `.claude/skills/` are the single source of truth.
The former cross-vendor setup — GitHub Copilot (`.github/copilot-instructions.md`,
`.github/instructions/*.instructions.md`) and OpenCode (`opencode.json`,
`.opencode/`, `docs/opencode-startup.md`) — is **retired**. Its unique content (the
ticket/board and solutions schemas) was migrated into `.claude/rules/tickets.md`
and `.claude/rules/solutions.md`, and `AGENTS.md`'s cardinal rules (never-commit,
British English) were folded into CLAUDE.md.

This supersedes the original D2 ("preserve `.github/instructions/`; the two
systems coexist without overlap"), which a code review found unachievable — once
§7 C++ conventions moved into `.claude/rules/cpp.md` they necessarily duplicated
`.github/instructions/cpp-hot-path.instructions.md`. Consolidating on one system
removes the duplication rather than trying to keep two vendor trees in sync.

### D3: CLAUDE.md Remains Authoritative

CLAUDE.md contains content that must be read every session:
- Project identity and v0.3 scope
- Operating persona and modes
- Source-of-truth hierarchy
- Physical safety contract (non-negotiable)
- Evidence and validation expectations
- Dispatch table to modular files

Safety rules in CLAUDE.md cannot be overridden by rules or skills.

### D4: Skill Invocation Model

| Skill | Invocation | Rationale |
|-------|------------|-----------|
| adr-creation | Model | Routine procedure, no physical risk |
| experiment-design | Model | Planning, no actuation |
| hardware-change-assessment | Model | Assessment, not execution |
| tracking-review | Model | Review checklist, no changes |
| bench-validation | Model | Synthetic/recorded data only |
| real-hardware-actuation | **Explicit only** | Physical movement risk |

Only `real-hardware-actuation` requires explicit user invocation because it
commands real hardware (laser, drone, servos).

### D5: Rule File Path Patterns

| Rule | `paths` Pattern |
|------|-------------------|
| cpp.md | `tracking-core/**/*.cpp`, `tracking-core/**/*.hpp`, `tracking-core/**/*.h` |
| python.md | `tracking-core/src/viewer/**/*.py`, `tracking-core/tools/**/*.py`, `tracking-core/tests/python_integration/**/*.py` |
| project-docs.md | `docs/adr/*.md`, `docs/tickets/*.md`, `BOARD.md` |

Each pattern is a glob; a rule's `paths:` list is an OR of its globs. Patterns
include file extensions to avoid over-triggering on docs or unrelated files.
Globs target the current repository layout (`tracking-core/src/...`,
`tracking-core/tests/...`, `tracking-core/tools/...`), reusing the patterns
already encoded in `.github/instructions/`. Do not treat CLAUDE.md §8.7's
aspirational `services/`/`tools/`/`tests/` layout as authoritative; §8.7 is
currently inconsistent with the real `tracking-core/`-rooted tree.

The `actuator-safety` rule from earlier drafts is intentionally **not** created
now: its target dirs (`laser-controller/`, `mavlink-adapter/`, `drone/`) do not
exist until Phase 3+/4+, and the safety-critical actuation block lives in
always-read CLAUDE.md (D8) plus a PreToolUse hook for hard enforcement. Add the
rule when those subsystems land.

### D6: File Format Schema

All `.claude/rules/` and `.claude/skills/` files use YAML frontmatter:

**Rule file frontmatter:** (field name confirmed against Claude Code docs —
`paths:`, not `applyTo:`; a rule with no `paths:` field loads every session at
`.claude/CLAUDE.md` priority, a rule with `paths:` loads only when Claude reads
a matching file)
```yaml
---
description: "One-line description for discovery"
paths:
  - "path/pattern/*.ext"
  - "another/pattern/**"
---
```

**Skill file layout and frontmatter:**

Each skill is a directory `.claude/skills/<skill-name>/SKILL.md` (not a flat
`.md` file — flat files are the legacy `commands/` format and would not be
discovered as skills). Frontmatter:
```yaml
---
name: "skill-name"
description: "One-line description for model invocation"
---
```

The `name` + `description` fields enable model selection of relevant skills.
There is no `alwaysApply` field in Claude Code skills (that is a Cursor-rules
field). To make `real-hardware-actuation` explicit-invocation-only, set
`disable-model-invocation: true` in its frontmatter — the documented Claude Code
field that lets only the user invoke the skill (`/real-hardware-actuation`) and
prevents Claude from ever auto-loading it. Its description is then kept out of
context until the user invokes it.

### D7: Skill Invocation Triggers

Each model-invoked skill has explicit trigger conditions:

| Skill | Trigger | Example User Request |
|-------|---------|---------------------|
| adr-creation | Decision is architecturally significant; new contract, boundary, or trade-off | "Let's use ROS2 instead of ZMQ" |
| experiment-design | Empirical measurement needed before design commit | "How do we know this will work on Pi 5?" |
| hardware-change-assessment | Proposed change affects wiring, mount, MCU, camera, laser | "I want to move the camera" |
| tracking-review | User says "review", "look at", uploads code, or requests four-domain test | "Review this detector change" |
| bench-validation | Implementation ready for validation before real hardware | "Test this on recorded data" |
| real-hardware-actuation | **User explicitly invokes** — never auto-triggered | User: "/skill real-hardware-actuation" |

The `real-hardware-actuation` skill is NEVER model-invoked. If the model
believes hardware actuation is needed, it MUST ask the user to invoke the
skill explicitly.

### D8: Safety-Critical Blocking Rules

These rules are stated in CLAUDE.md and cannot be overridden:

1. **Hardware actuation requires explicit skill invocation.** Never fire the
   laser, arm the drone, or command servos without user invoking
   `real-hardware-actuation` skill first.

2. **`safe_for_control` predicate is authoritative.** ADR-007 defines the
   safety contract. Any change to predicate logic requires ADR update.

3. **Dry-run is the default.** All actuator code paths default to simulation.
   Real actuation requires explicit flag AND skill invocation.

4. **Recording requirements are mandatory.** Changes to calibration,
   coordinates, control logic, or safety predicates must be recorded.

**Implementation note:** Dry-run conventions appear in two places:
- `actuator-safety.md` (rule): Code-level convention that actuator code
  defaults to dry-run mode.
- `real-hardware-actuation.md` (skill): Workflow procedure for transitioning
  from dry-run to real actuation, including pre-flight checklist.

**Enforcement caveat (confirmed against Claude Code docs):** CLAUDE.md, rules,
and skills are loaded as *context, not enforced configuration* — "Claude treats
them as context... To block an action regardless of what Claude decides, use a
PreToolUse hook instead." So D3/D8's "cannot be overridden" is a behavioural
convention, not a hard guarantee. Any truly non-negotiable actuation block
(e.g. never fire the laser without the explicit skill + flag) must be backed by
a `PreToolUse` hook, not by rule/skill text alone.

### D10: Consolidated Split (resolves P4 — cap the fan-out)

The original per-topic granularity (8 rules + 6 skills + 8 reference = 22 files)
multiplies the single-maintainer sync surface. Consolidate to a smaller, more
general set — grouped by audience and trigger, not by source section — and make
ADR-derived content *pointers to the ADRs*, not synced copies, so there is one
source of truth:

| File | Trigger | Consolidates |
|------|---------|--------------|
| `.claude/rules/cpp.md` | `paths:` tracking-core C++ | §7.1 hot-path, §7.2 general C++, §7.4 deviations, §5 boundaries, §7/ADR-003/006/010 coordinate & Z, §8.6 code docs |
| `.claude/rules/python.md` | `paths:` tracking-core Python | §7.3 |
| `.claude/rules/project-docs.md` | `paths:` docs/adr, docs/tickets, BOARD.md | §8.3-8.5 ADR conventions, §11.7 ticket board |
| `.claude/skills/adr-creation/SKILL.md` | model-invoked | §8.1-8.2 |
| `.claude/skills/tracking-review/SKILL.md` | model-invoked | §10 pitfalls, §9.4 four-domain test |
| `docs/agent-reference/agent-reference.md` | on-demand read | §3 baseline (as ADR pointers), §4 hardware/environment, coordinate frames, §4/ADR-004 calibration, §13 risks, §8.7 layout, §9 testing detail, §12 glossary, §10 pitfalls detail, §11.2/11.3/11.6 workflow |

Total: **6 satellite files** (3 rules + 2 skills + 1 reference), down from 22,
plus always-read `CLAUDE.md`. `actuator-safety` is NOT created now — its target
dirs (`laser-controller/`, `mavlink-adapter/`, `drone/`) do not exist until
Phase 3+/4+; the safety-critical actuation block lives in always-read CLAUDE.md
(D8) and, for hard enforcement, a PreToolUse hook. Testing conventions (§9) move
to the reference file; the ADR-007 ship-gate summary stays always-read
(Evidence & Validation). The D5 and Content Allocation tables below are written
against this consolidated set.

## Directory Structure

```
.claude/
  rules/
    cpp.md                             # ~110 lines  (§7.1/7.2/7.4, §5, §7+ADR-003/006/010, §8.6)
    python.md                          # ~20 lines   (§7.3)
    project-docs.md                    # ~55 lines   (§8.3-8.5, §11.7)

  skills/
    adr-creation/SKILL.md              # ~70 lines
    tracking-review/SKILL.md           # ~75 lines
    # experiment-design, hardware-change-assessment, bench-validation,
    # real-hardware-actuation — deferred to a follow-up plan (see scope note)

docs/
  agent-reference/
    agent-reference.md                 # ~500 lines  (§3, §4 detail, coordinate frames,
                                       #   §4+ADR-004 calibration, §13, §8.7, §9 detail,
                                       #   §12 glossary, §10 pitfalls detail, §11.2/11.3/11.6)

CLAUDE.md                              # ~210 lines (down from ~870)
```

**Total modular content:** ~830 lines across 6 satellite files + ~210 line CLAUDE.md
**As-built (2026-07-06):** 684 satellite lines across 6 files (cpp.md 129, python.md 24, project-docs.md 61, adr-creation 72, tracking-review 61, agent-reference.md 337) + 202-line CLAUDE.md.
**Original:** ~870 lines in single file

The satellite files are grouped by trigger (path-scoped rules, model-invoked
skills, on-demand reference) rather than one-per-topic, so the always-read
surface drops from ~870 to ~210 lines while total authored content stays close
to the original — the reference file holds lookup detail and ADR pointers rather
than synced copies.

## Content Allocation

### Stays in CLAUDE.md

| Section | Lines | Original Source | Content |
|---------|-------|-----------------|---------|
| Project Identity | ~25 | §1 | What this is/isn't, v0.3 scope, user context |
| Operating Persona | ~20 | §2 | Four modes, mode-independent rules |
| Source of Truth | ~20 | §6 | 5-level hierarchy, critical rule |
| Physical Safety Contract | ~35 | §4 (subset) | See D9 below for exact content |
| Workflow Essentials | ~30 | §11.1, §11.4, §11.5 | Ask-vs-deliver (condensed), reasoning visibility, tooling habits |
| Evidence & Validation | ~20 | §9.2-9.3 (summary) | Ship gates, burden of proof |
| Dispatch Table | ~50 | (new) | Rules, skills, reference mapping (6 files) |
| What's Not Here | ~15 | (new) | Pointer to modular files, guidance on when to read them |

**Total: ~210 lines**

### D9: Physical Safety Contract Content (what stays in CLAUDE.md)

From original §4, these specific items stay in CLAUDE.md because they are
non-negotiable safety rules:

**Stays in CLAUDE.md:**
- "Never assume real hardware state" (§4 framing)
- "Default to dry-run / bench validation" (§4 framing)
- "Distinguish measured results vs. assumptions vs. proposals" (implicit)
- Recording requirements for calibration/coordinates/control/safety changes
- Children present assumption (§4.human factors)
- Thermal throttling behavior requirement (§4.hardware.thermal)
- IR laser preferred for eye safety (§4.hardware.laser)
- No internet dependency (§4.network)
- Laser specular-reflection ghost risk (R-04) and camera focus-drift invalidation (R-05)
- Floor non-planarity bias near rugs/thresholds (R-02)
- The "Z=0 is not universal" and "brightest pixel is not the laser" pitfalls (§10)

**Moves to hardware-environment.md:**
- Pi 5 specs (RAM, OS, GPU absence)
- Co-tenancy CPU budget details
- Camera specs (sensor, shutter, focus)
- CSI cable constraints
- Network bandwidth/latency details
- Power supply details
- Environmental conditions detail (lighting, reflections, floor)

### Moves to `.claude/rules/` (3 files, per D10)

| Original Section | Target Rule | Key Content |
|------------------|-------------|-------------|
| §7.1 + §7.2 + §7.4 C++ | cpp.md | Hot-path discipline, general conventions, deviation rules |
| §5 System Boundaries | cpp.md | Core responsibilities, adapters, ZMQ contract |
| §7 + ADR-003/006/010 | cpp.md | Z compensation, projection pipeline |
| §8.6 Code documentation | cpp.md | Doc-comment conventions (brief) |
| §7.3 Python conventions | python.md | Formatting, typing, framework avoidance |
| §8.3-8.5 ADR conventions | project-docs.md | Anti-patterns, file naming, design-doc sync |
| §11.7 Repo tooling | project-docs.md | Story schema, U-IDs, transitions |

`cpp.md` is the single rule that loads when editing tracking-core C++ — it
carries everything a C++ edit needs (hot-path discipline, boundaries, coordinate
mapping, code docs). The §5 + ADR-008 actuator-safety content is deferred with
the `actuator-safety` rule (see D5/D10). §9 testing detail moves to the
reference file; the ADR-007 ship-gate summary stays always-read.

### Moves to `.claude/skills/` (2 files, per D10)

| Original Section | Target Skill | Key Content |
|------------------|--------------|-------------|
| §8.1-8.2 When/how to write ADR | adr-creation/SKILL.md | Triggers, template, validation |
| §10 pitfalls + §9.4 | tracking-review/SKILL.md | Four-domain test, pitfall checklist |

**Scope note:** Only `adr-creation` (from §8) and `tracking-review` (from
§10/§9.4) are in scope — they reorganise existing CLAUDE.md content. The four
skills from earlier drafts (`experiment-design`, `hardware-change-assessment`,
`bench-validation`, `real-hardware-actuation`) author net-new workflow
procedures that do not exist in the source and are **deferred to a separate
follow-up plan** where each gets its own design review. Phases 3 and 6 build
only the two in-scope skills.

### Moves to `docs/agent-reference/agent-reference.md` (1 file, per D10)

A single on-demand reference file with one `##` section per topic below. ADR
summaries are **pointers to the ADR** (cite the ADR ID; do not restate its
rationale), so there is one source of truth and no synced copy to drift.

| Original Section | Section in agent-reference.md | Key Content |
|------------------|-------------------------------|-------------|
| §12 Glossary | Glossary | Terms, acronyms |
| §4 Physical Environment (detail) | Hardware & environment | Pi 5 specs, camera, network, thermal (see D9) |
| ADR-003/006/010 details | Coordinate frames | Tiers, FloorPlane2D, Z compensation (pointer to ADRs) |
| §4 + ADR-004 | Calibration lifecycle | Phases, health states, drift |
| §13 Risks & Debt | Risks & debt | R-01–R-07, D-01–D-06 |
| §3 Architectural Baseline | Architectural baseline | ADR index + locked/open decisions (pointers to ADRs) |
| §8.7 Directory layout | Architectural baseline | Real repo layout |
| §9 Testing (detail) | Testing | Categories, performance-budget detail (ship-gate summary stays in CLAUDE.md) |
| §10 Common Pitfalls (detail) | Common pitfalls | P-01–P-08 (safety subset also stays in CLAUDE.md) |
| §11.2, §11.3, §11.6 | Workflow conventions | Checkpoint workflow, file creation, session rituals |

**Safety-content retention (per D3):** the safety-load-bearing subset of §10
and §13 — the pitfalls and risks listed under D9 "Stays in CLAUDE.md" — is
summarised in always-read CLAUDE.md. The reference files hold the *detail and
lookup material*, not the always-needed corrections an unprimed agent won't
know to fetch. "Preserved in a file" is not "loaded when the reasoning
happens"; the always-read summary is what guarantees an agent reasoning about a
detector or laser change has the eye-safety, reflection, and floor-planarity
context even in a session that never opens a reference doc.

Note: §11.1 (ask-vs-deliver), §11.4 (reasoning visibility), and §11.5
(tooling habits) stay in CLAUDE.md because they affect every session.

## Alternatives Considered

Three lower-risk alternatives were available. This section records them so the
choice of the three-tier `.claude/` scheme is deliberate rather than
path-dependent (per the project's anti-Groundhog-Day standard).

- **(a) Trim the monolith in place.** Condense verbose prose to reach ~220
  lines in a single always-read file, with reference detail below a fold in the
  same file. Meets Success Criterion 1, preserves all content, and avoids the
  unverified `.claude/rules/` mechanism entirely. It does not enable
  path-scoped incremental discovery — but it is the cheapest path and remains
  the fallback if the discovery mechanism is unverified.
- **(b) Two files (core + reference).** Always-read `CLAUDE.md` plus a single
  `docs/agent-reference.md` for lookup detail. Captures most of the size
  reduction across 2 files instead of the original 22-file proposal, with a far
  smaller sync surface.
- **(c) Reuse the existing `.github/instructions/` mechanism.** The repo already
  has 8 path-scoped instruction files using `applyTo`. Path-scoped rules could
  route through that already-supported mechanism rather than inventing a
  parallel `.claude/rules/` tier.

The `.claude/rules/` mechanism is now verified (see Deferred / Open Questions —
RESOLVED). The three-tier scheme is retained but consolidated per D10 to **6
satellite files**, which brings it close to alternative (b) on sync surface
while keeping (c)-style path-scoped auto-loading via `.claude/rules/`.
Alternative (a) remains the fallback if the reorganisation is judged not worth
its cost.

## Implementation Plan

### Phase 1: Create directory structure
1. Create `.claude/rules/` directory
2. Create `.claude/skills/` directory
3. Create `docs/agent-reference/` directory

### Phase 2: Write rule files (3 files, per D10)
1. cpp.md
2. python.md
3. project-docs.md

### Phase 3: Write skill files (2 files, per D10)
1. adr-creation/SKILL.md
2. tracking-review/SKILL.md

### Phase 4: Write the reference file (1 file, per D10)
1. agent-reference.md — one `##` section per topic (glossary, hardware &
   environment, coordinate frames, calibration lifecycle, risks & debt,
   architectural baseline, testing detail, common pitfalls, workflow
   conventions), with ADR summaries as pointers to the ADRs

### Phase 5: Rewrite CLAUDE.md
1. Backup current CLAUDE.md
2. Write new slim CLAUDE.md (~210 lines)
3. Verify dispatch table references correct files

### Phase 6: Validation
1. Verify all original content is preserved (diff analysis)
2. Test rule triggering on sample file paths
3. Test skill invocation
4. Verify reference files are complete

## Risks

### R1: Content loss during migration
**Mitigation:** Diff original CLAUDE.md against all new files to verify
coverage. Checklist each original section.

### R2: Rule patterns too broad or too narrow
**Mitigation:** Test patterns against actual file paths in repo. Adjust
during implementation.

### R3: Skills not invoked when needed
**Mitigation:** Clear trigger criteria in each skill. Model invocation for
routine skills. Explicit only for consequential ones.

### R4: Reference doc becomes stale
**Mitigation:** The single reference file holds durable information (glossary,
hardware specs) and *pointers* to ADRs rather than synced copies, so ADR changes
do not silently drift the reference. Consolidating to 6 satellite files (D10)
keeps the sync surface small for a single maintainer.

## Success Criteria

1. CLAUDE.md is ≤220 lines
2. All original content is preserved in modular files (verified by section checklist)
3. Rule files trigger correctly on their target paths
4. Skills are discoverable and invocable by model
5. Reference docs are complete and self-contained
6. No duplication between `.claude/` and `.github/instructions/`
7. Safety-critical content (D8 rules) remains in always-read CLAUDE.md
8. Every original section (§1-§13) has explicit destination documented
9. A representative-session load test passes: for at least three task types
   (e.g. "design a detector change", "edit config", "plan a laser experiment"),
   enumerate which files actually load and assert every safety-relevant item
   reachable in today's always-read CLAUDE.md is still reachable without the
   agent having to already know to open a reference file

## Validation Checklist

Every original section must have a documented destination. Implementation is
not complete until this checklist is verified.

| Original Section | Destination | Verified |
|------------------|-------------|----------|
| §1 Project Identity | CLAUDE.md | [ ] |
| §2 Operating Persona | CLAUDE.md | [ ] |
| §3 Architectural Baseline | agent-reference.md | [ ] |
| §4.hardware | agent-reference.md | [ ] |
| §4.safety (children, thermal, laser, no-internet) | CLAUDE.md | [ ] |
| §5 System Boundaries | .claude/rules/cpp.md | [ ] |
| §6 Source of Truth | CLAUDE.md | [ ] |
| §7.1 C++ hot-path | .claude/rules/cpp.md | [ ] |
| §7.2 C++ general | .claude/rules/cpp.md | [ ] |
| §7.3 Python | .claude/rules/python.md | [ ] |
| §7.4 Deviation rules | .claude/rules/cpp.md | [ ] |
| §8.1 When to write ADR | .claude/skills/adr-creation/SKILL.md | [ ] |
| §8.2 ADR template | .claude/skills/adr-creation/SKILL.md | [ ] |
| §8.3 ADR anti-patterns | .claude/rules/project-docs.md | [ ] |
| §8.4 File naming | .claude/rules/project-docs.md | [ ] |
| §8.5 Design doc sync | .claude/rules/project-docs.md | [ ] |
| §8.6 Code documentation | .claude/rules/cpp.md (brief) | [ ] |
| §8.7 Directory layout | agent-reference.md | [ ] |
| §9.1 Test categories | agent-reference.md | [ ] |
| §9.2 ADR-007 replay test | agent-reference.md + CLAUDE.md (summary) | [ ] |
| §9.3 Performance budgets | agent-reference.md + CLAUDE.md (summary) | [ ] |
| §9.4 What to test | .claude/skills/tracking-review/SKILL.md | [ ] |
| §9.5 Test vs reality | agent-reference.md | [ ] |
| §10 Common Pitfalls | agent-reference.md + CLAUDE.md (safety-critical subset); tracking-review skill (checklist) | [ ] |
| §11.1 Ask-vs-deliver | CLAUDE.md | [ ] |
| §11.2 Checkpoint workflow | agent-reference.md | [ ] |
| §11.3 File creation | agent-reference.md | [ ] |
| §11.4 Reasoning visibility | CLAUDE.md | [ ] |
| §11.5 Tooling habits | CLAUDE.md | [ ] |
| §11.6 Session opening/closing | agent-reference.md | [ ] |
| §11.7 Repo tooling | .claude/rules/project-docs.md | [ ] |
| §12 Glossary | agent-reference.md | [ ] |
| §13 Risks & Debt | agent-reference.md + CLAUDE.md (R-02/R-04/R-05 safety subset) | [ ] |
| (Safety) real-hardware-actuation skill | deferred to follow-up plan; actuation block in CLAUDE.md D8 + PreToolUse hook | [ ] |
| (Safety) D8 blocking rules in CLAUDE.md | CLAUDE.md | [ ] |

## Out of Scope

- Modifying existing `.github/instructions/` files
- Creating new `.github/instructions/` files
- Changes to ADRs or design documents
- Changes to ticket/board workflow
- Changes to tools/board/ scripts
- Authoring the four net-new skills (experiment-design, hardware-change-assessment, bench-validation, real-hardware-actuation) — these document workflows absent from the source CLAUDE.md and belong in a separate plan with its own review

## Deferred / Open Questions

### From 2026-07-06 review

- **`.claude/rules/` auto-apply mechanism — RESOLVED (2026-07-06)** — D1/D6 (was P1, feasibility & adversarial)

  Verified against the official Claude Code docs (code.claude.com/docs/en/memory#organise-rules-with-clauderules and /skills). The mechanism **exists and works as the design assumes** — feasibility was correct, adversarial's "Copilot/Cursor-only" concern is refuted. Corrections applied to the spec: rule frontmatter field is `paths:` not `applyTo:` (D5/D6 updated); skills are `<name>/SKILL.md` with `name`+`description` and the explicit-only skill uses `disable-model-invocation: true` (D6 updated). One residual, now captured in D8: rules/skills/CLAUDE.md are context, not enforcement — a hard actuation block requires a `PreToolUse` hook, not rule text. Loading model confirmed: `paths:`-scoped rules load only on a matching file read, so the safety content kept always-read per M2 remains the correct call.

- **Split may relocate/grow per-session load rather than reduce it** — D1 (P1, product-lens & adversarial, confidence 100)

  Rules and skill descriptions must be resident for the model to select them, so a normal coding session loads CLAUDE.md plus several rules plus the dispatch table plus every skill description — plausibly comparable to today's always-on cost — while total authored content nearly doubles (~870 → ~1,630 lines) and the plan authors net-new contract never in the validated original. The mechanism moves lines around and adds more without a demonstrated drop in what a working session actually loads. Quantify the expected loaded-line count for 2–3 representative sessions versus the monolith before committing.

  **Partly mitigated (2026-07-06):** verification confirmed `paths:`-scoped rules load only on a matching file read, and D10 cuts total authored content to ~830 lines across 6 files (no longer a near-doubling). The always-read surface is now CLAUDE.md (~210) plus 2 skill descriptions; a C++ session adds only `cpp.md`. The quantify-for-representative-sessions ask (Success Criterion 9) still stands as the confirmation gate.

- **Context-overhead problem — RESOLVED (2026-07-06)** — Overview (was P1, product-lens)

  The maintainer confirmed the goal directly: reduce the size of the always-read CLAUDE.md. The premise is accepted as an explicit maintainer preference (a leaner always-read contract), independent of a measured token-budget failure. The reorganisation is greenlit; D10's consolidated 6-file split is the committed approach, with alternative (a) trim-in-place available if the split proves not worth its cost.

- **Fan-out sync burden — RESOLVED (2026-07-06)** — Risks / R4 (was P2, product-lens)

  Resolved by D10: the split is consolidated from 22 satellite files to **6** (3 path-scoped rules, 2 skills, 1 reference), grouped by trigger rather than one-per-topic, and ADR-derived content is now *pointers to the ADRs* rather than synced copies. This removes the primary drift surface (the ADR-summary and boundary files that had to track upstream changes) and cuts the single-maintainer sync burden accordingly.
