# CLAUDE.md Modularization Design

## Overview

Split the monolithic CLAUDE.md (~870 lines) into a Claude-native modular
structure enabling incremental discovery. The goal: reduce per-session context
overhead while preserving all guidance.

**Target structure:**
- `CLAUDE.md` (~210 lines): Always-read core — identity, safety, workflow, dispatch
- `.claude/rules/` (8 files): Auto-applied by file path
- `.claude/skills/` (6 files): Invoked on demand or by model
- `docs/agent-reference/` (8 files): Detailed reference material

## Design Decisions

### D1: Discovery Mechanism — Hybrid

Use Claude-native mechanisms matched to content type:

| Content Type | Mechanism | Rationale |
|--------------|-----------|-----------|
| Coding standards, architecture boundaries | `.claude/rules/` with `applyTo` | Auto-applies when editing relevant paths |
| Multi-step procedures | `.claude/skills/` | Explicit invocation for workflows |
| Reference material | `docs/agent-reference/` | Consulted on demand, not loaded by default |
| Safety contract, identity, dispatch | `CLAUDE.md` | Always-read, non-negotiable |

### D2: Preserve Existing `.github/instructions/`

The 8 existing `.github/instructions/*.md` files serve GitHub Copilot and
other tooling. Do not duplicate into `.claude/rules/`. The two systems
coexist without overlap.

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

| Rule | `applyTo` Pattern |
|------|-------------------|
| cpp-hot-path | `tracking-core/**/*.cpp`, `tracking-core/**/*.hpp`, `tracking-core/**/*.h` |
| python-tooling | `services/**/*.py`, `tools/**/*.py`, `viewer/**/*.py` |
| tracking-core-boundaries | `tracking-core/src/**/*.cpp`, `tracking-core/src/**/*.hpp` |
| coordinate-calibration | `tracking-core/src/**/coordinate*.cpp`, `tracking-core/src/**/calibrat*.cpp`, `tracking-core/src/**/mapper*.cpp`, `tracking-core/src/**/homograph*.cpp` |
| actuator-safety | `laser-controller/**`, `mavlink-adapter/**`, `drone/**` |
| testing-validation | `tests/**`, `*_test.cpp`, `*_test.py`, `*_bench*` |
| adr-conventions | `docs/adr/*.md` |
| ticket-board | `docs/tickets/*.md`, `BOARD.md` |

Patterns are comma-separated OR conditions. Each pattern is a glob. Patterns
include file extensions to avoid over-triggering on docs or unrelated files.

### D6: File Format Schema

All `.claude/rules/` and `.claude/skills/` files use YAML frontmatter:

**Rule file frontmatter:**
```yaml
---
description: "One-line description for discovery"
applyTo:
  - "path/pattern/*.ext"
  - "another/pattern/**"
---
```

**Skill file frontmatter:**
```yaml
---
description: "One-line description for model invocation"
alwaysApply: false
---
```

The `description` field enables model selection of relevant skills.
`alwaysApply: false` means the skill is invoked on demand, not automatically.

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

## Directory Structure

```
.claude/
  rules/
    cpp-hot-path.md                    # ~50 lines
    python-tooling.md                  # ~20 lines
    tracking-core-boundaries.md        # ~55 lines
    coordinate-calibration.md          # ~50 lines
    actuator-safety.md                 # ~45 lines
    testing-validation.md              # ~50 lines
    adr-conventions.md                 # ~40 lines
    ticket-board.md                    # ~55 lines

  skills/
    adr-creation.md                    # ~70 lines
    experiment-design.md               # ~80 lines
    hardware-change-assessment.md      # ~70 lines
    tracking-review.md                 # ~75 lines
    bench-validation.md                # ~65 lines
    real-hardware-actuation.md         # ~80 lines

docs/
  agent-reference/
    glossary.md                        # ~90 lines
    hardware-environment.md            # ~75 lines
    coordinate-frames.md               # ~70 lines
    calibration-lifecycle.md           # ~75 lines
    risks-and-debt.md                  # ~120 lines
    architectural-baseline.md          # ~70 lines
    common-pitfalls.md                 # ~100 lines
    workflow-conventions.md            # ~60 lines (§11.2-11.3, 11.6)

CLAUDE.md                              # ~210 lines (down from ~870)
```

**Total modular content:** ~1,420 lines across 22 files + 210 line CLAUDE.md
**Original:** ~870 lines in single file

The increase is due to:
- Explicit frontmatter in each file
- Clearer section structure for standalone reading
- Procedure details in skills that were implicit before
- New skills documenting implicit workflows

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
| Dispatch Table | ~50 | (new) | Rules, skills, reference mapping (21 files) |
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

**Moves to hardware-environment.md:**
- Pi 5 specs (RAM, OS, GPU absence)
- Co-tenancy CPU budget details
- Camera specs (sensor, shutter, focus)
- CSI cable constraints
- Network bandwidth/latency details
- Power supply details
- Environmental conditions detail (lighting, reflections, floor)

### Moves to `.claude/rules/`

| Original Section | Target Rule | Key Content |
|------------------|-------------|-------------|
| §7.1 + §7.2 C++ conventions | cpp-hot-path.md | Hot-path discipline (§7.1) + general conventions (§7.2) |
| §7.3 Python conventions | python-tooling.md | Formatting, typing, framework avoidance |
| §5 System Boundaries | tracking-core-boundaries.md | Core responsibilities, adapters, ZMQ contract |
| §7 + ADR-003/006/010 | coordinate-calibration.md | Z compensation, projection pipeline |
| §5 + ADR-008 | actuator-safety.md | Failsafe-off, dry-run, MAVLink contract |
| §9 Testing | testing-validation.md | Categories, ADR-007 gate, performance budgets |
| §8.2-8.4 ADR conventions | adr-conventions.md | Template, anti-patterns, file naming |
| §11.7 + existing | ticket-board.md | Story schema, U-IDs, transitions |

Note: `cpp-hot-path.md` contains both §7.1 (hot-path specific) and §7.2
(general C++ conventions). The file name reflects the primary concern; both
apply when editing tracking-core C++ files.

### Moves to `.claude/skills/`

| Original Section | Target Skill | Key Content |
|------------------|--------------|-------------|
| §8.1-8.2 When/how to write ADR | adr-creation.md | Triggers, template, validation |
| (New) | experiment-design.md | Hypothesis, variables, recording |
| (New) | hardware-change-assessment.md | Calibration/safety/contract impact |
| §10 pitfalls + §9.4 | tracking-review.md | Four-domain test, pitfall checklist |
| (New) | bench-validation.md | Test data, metrics, gate decision |
| (New - safety critical) | real-hardware-actuation.md | Pre-flight, execution, recording |

### Moves to `docs/agent-reference/`

| Original Section | Target File | Key Content |
|------------------|-------------|-------------|
| §12 Glossary | glossary.md | Terms, acronyms |
| §4 Physical Environment (detail) | hardware-environment.md | Pi 5 specs, camera, network, thermal (see D9) |
| ADR-003/006/010 details | coordinate-frames.md | Tiers, FloorPlane2D, Z compensation |
| §4 + ADR-004 | calibration-lifecycle.md | Phases, health states, drift |
| §13 Risks & Debt | risks-and-debt.md | R-01–R-07, D-01–D-06 |
| §3 Architectural Baseline | architectural-baseline.md | ADR table, locked/open decisions |
| §10 Common Pitfalls | common-pitfalls.md | P-01–P-08 with failure modes |
| §11.2, §11.3, §11.6 | workflow-conventions.md | Checkpoint workflow, file creation, session rituals |

Note: §11.1 (ask-vs-deliver), §11.4 (reasoning visibility), and §11.5
(tooling habits) stay in CLAUDE.md because they affect every session.

## Implementation Plan

### Phase 1: Create directory structure
1. Create `.claude/rules/` directory
2. Create `.claude/skills/` directory
3. Create `docs/agent-reference/` directory

### Phase 2: Write rule files (8 files)
1. cpp-hot-path.md
2. python-tooling.md
3. tracking-core-boundaries.md
4. coordinate-calibration.md
5. actuator-safety.md
6. testing-validation.md
7. adr-conventions.md
8. ticket-board.md

### Phase 3: Write skill files (6 files)
1. adr-creation.md
2. experiment-design.md
3. hardware-change-assessment.md
4. tracking-review.md
5. bench-validation.md
6. real-hardware-actuation.md

### Phase 4: Write reference files (8 files)
1. glossary.md
2. hardware-environment.md
3. coordinate-frames.md
4. calibration-lifecycle.md
5. risks-and-debt.md
6. architectural-baseline.md
7. common-pitfalls.md
8. workflow-conventions.md

### Phase 5: Rewrite CLAUDE.md
1. Backup current CLAUDE.md
2. Write new slim CLAUDE.md (~180 lines)
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

### R4: Reference docs become stale
**Mitigation:** Reference docs contain durable information (glossary, hardware
specs, ADR summaries). Maintenance rule in risks-and-debt.md.

## Success Criteria

1. CLAUDE.md is ≤220 lines
2. All original content is preserved in modular files (verified by section checklist)
3. Rule files trigger correctly on their target paths
4. Skills are discoverable and invocable by model
5. Reference docs are complete and self-contained
6. No duplication between `.claude/` and `.github/instructions/`
7. Safety-critical content (D8 rules) remains in always-read CLAUDE.md
8. Every original section (§1-§13) has explicit destination documented

## Validation Checklist

Every original section must have a documented destination. Implementation is
not complete until this checklist is verified.

| Original Section | Destination | Verified |
|------------------|-------------|----------|
| §1 Project Identity | CLAUDE.md | [ ] |
| §2 Operating Persona | CLAUDE.md | [ ] |
| §3 Architectural Baseline | docs/agent-reference/architectural-baseline.md | [ ] |
| §4.hardware | docs/agent-reference/hardware-environment.md | [ ] |
| §4.safety (children, thermal, laser, no-internet) | CLAUDE.md | [ ] |
| §5 System Boundaries | .claude/rules/tracking-core-boundaries.md | [ ] |
| §6 Source of Truth | CLAUDE.md | [ ] |
| §7.1 C++ hot-path | .claude/rules/cpp-hot-path.md | [ ] |
| §7.2 C++ general | .claude/rules/cpp-hot-path.md | [ ] |
| §7.3 Python | .claude/rules/python-tooling.md | [ ] |
| §7.4 Deviation rules | .claude/rules/cpp-hot-path.md | [ ] |
| §8.1 When to write ADR | .claude/skills/adr-creation.md | [ ] |
| §8.2 ADR template | .claude/skills/adr-creation.md | [ ] |
| §8.3 ADR anti-patterns | .claude/rules/adr-conventions.md | [ ] |
| §8.4 File naming | .claude/rules/adr-conventions.md | [ ] |
| §8.5 Design doc sync | .claude/rules/adr-conventions.md | [ ] |
| §8.6 Code documentation | .claude/rules/cpp-hot-path.md (brief) | [ ] |
| §8.7 Directory layout | docs/agent-reference/architectural-baseline.md | [ ] |
| §9.1 Test categories | .claude/rules/testing-validation.md | [ ] |
| §9.2 ADR-007 replay test | .claude/rules/testing-validation.md + CLAUDE.md (summary) | [ ] |
| §9.3 Performance budgets | .claude/rules/testing-validation.md + CLAUDE.md (summary) | [ ] |
| §9.4 What to test | .claude/rules/testing-validation.md | [ ] |
| §9.5 Test vs reality | .claude/rules/testing-validation.md | [ ] |
| §10 Common Pitfalls | docs/agent-reference/common-pitfalls.md | [ ] |
| §11.1 Ask-vs-deliver | CLAUDE.md | [ ] |
| §11.2 Checkpoint workflow | docs/agent-reference/workflow-conventions.md | [ ] |
| §11.3 File creation | docs/agent-reference/workflow-conventions.md | [ ] |
| §11.4 Reasoning visibility | CLAUDE.md | [ ] |
| §11.5 Tooling habits | CLAUDE.md | [ ] |
| §11.6 Session opening/closing | docs/agent-reference/workflow-conventions.md | [ ] |
| §11.7 Repo tooling | .claude/rules/ticket-board.md | [ ] |
| §12 Glossary | docs/agent-reference/glossary.md | [ ] |
| §13 Risks & Debt | docs/agent-reference/risks-and-debt.md | [ ] |
| (Safety) real-hardware-actuation skill | .claude/skills/real-hardware-actuation.md | [ ] |
| (Safety) D8 blocking rules in CLAUDE.md | CLAUDE.md | [ ] |

## Out of Scope

- Modifying existing `.github/instructions/` files
- Creating new `.github/instructions/` files
- Changes to ADRs or design documents
- Changes to ticket/board workflow
- Changes to tools/board/ scripts
