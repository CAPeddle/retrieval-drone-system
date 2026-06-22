---
title: "fix: Reconcile repository layout documentation"
status: completed
created: 2026-06-21
updated: 2026-06-21
origin: docs/tickets/DOCS-006-reconcile-layout-with-multi-subsystem-structure.md
ticket: DOCS-006
type: fix
---

# fix: Reconcile Repository Layout Documentation

## Problem Frame

`README.md` defines the chosen repository shape as a multi-subsystem sibling layout rooted at `tracking-core/`, `viewer/`, `camera-node/`, `laser-controller/`, `mavlink-adapter/`, and `drone/`. `CLAUDE.md §8.7` still describes an older single-project layout with root `core/`, `services/`, `tools/`, `tests/`, and `config/`, and related agent instructions still repeat pieces of the stale model.

This plan reconciles the documentation so future agents follow one portable layout model. It does not move directories or implement subsystem code.

---

## Scope Boundaries

In scope:

- Update authoritative and agent-facing documentation so repository layout guidance matches `README.md`.
- Replace stale `services/`-as-canonical-adapter-home wording with the chosen top-level subsystem directories.
- Update ticket references where they would otherwise direct future work into stale paths.
- Preserve `tracking-core/` internal paths for code that has not yet been promoted or split.
- Preserve root `tools/board/` as canonical repository workflow tooling.

Out of scope:

- Moving `tracking-core/src/viewer/` to `viewer/`; that remains `VIEW-001`.
- Creating missing subsystem directories before their implementation tickets require them.
- Reopening ADR-001, ADR-002, or ADR-008 subsystem boundary decisions.
- Rewriting unrelated build, test, or workflow content.

### Deferred to Follow-Up Work

- Broader Copilot instruction modernisation belongs to `DOCS-005`.
- Viewer promotion and associated path changes belong to `VIEW-001`.

---

## Key Technical Decisions

- Treat `README.md` as the current layout baseline for this reconciliation, because it explicitly names the multi-subsystem target layout and marks current on-disk differences as transitional.
- Keep `CLAUDE.md` as the authoritative operating contract, but update its layout section and related adapter wording to align with the README baseline.
- Use top-level subsystem directories for production adapters and viewers in docs, while allowing existing `tracking-core/` development paths to remain until their own tickets move them.
- Keep root `tools/board/` in the canonical layout because those scripts are repo workflow helpers, not tracking-core implementation tooling.
- Verify by searching for stale layout tokens rather than relying on manual reading alone.

---

## Implementation Units

### U1. Establish The Canonical Layout Wording

**Goal:** Update `CLAUDE.md` so §8.7 and nearby path guidance describe the multi-subsystem repository structure without contradicting `README.md`.

**Requirements:** Advances DOCS-006 acceptance by making the highest-priority operating contract internally consistent with the chosen layout.

**Dependencies:** None.

**Files:**

- `CLAUDE.md`

**Approach:** Replace the stale root `core/`, `services/`, `tests/`, and `config/` layout claims with a root-level subsystem map. Preserve root `tools/board/` as repository workflow tooling. Preserve the distinction between authoritative docs and implementation directories. Update related Python tooling and adapter wording that currently says production tooling lives under `services/` so it points to subsystem-owned top-level directories instead.

**Patterns to follow:** `README.md` layout section and subsystem table.

**Test scenarios:**

- Test expectation: none -- documentation-only change with no executable behaviour.

**Verification:** `CLAUDE.md` no longer presents `core/`, `services/`, `tests/`, or `config/` as root-level canonical implementation directories, preserves `tools/board/` as repository workflow tooling, and still preserves ADR-governed subsystem boundaries.

### U2. Align Cross-Vendor Agent Instructions

**Goal:** Update agent instruction files so Copilot and OpenCode-adjacent guidance no longer routes future work into stale paths.

**Requirements:** Ensures agents following `.github` instructions see the same layout contract as `CLAUDE.md` and `README.md`.

**Dependencies:** U1.

**Files:**

- `.github/copilot-instructions.md`
- `.github/instructions/python-tooling.instructions.md`
- `.github/instructions/tracking-architecture.instructions.md`

**Approach:** Adjust repository layout and path guidance to reflect the multi-subsystem target. Keep current transitional paths explicit where files still live under `tracking-core/`. Extend file-scoped Python tooling guidance to promoted top-level subsystem paths so future files receive the same conventions. Avoid turning DOCS-006 into the broader Copilot refresh tracked by `DOCS-005`.

**Patterns to follow:** `AGENTS.md` for cross-vendor entry-point wording; `.github/instructions/*.instructions.md` frontmatter conventions.

**Test scenarios:**

- Test expectation: none -- documentation-only change with no executable behaviour.

**Verification:** Agent instructions no longer state that root `.md` files are design research or that all production adapters live under `services/`.

### U3. Correct Scheduled Ticket Path Guidance

**Goal:** Update future ticket acceptance criteria and plans that still name stale `services/` paths or ambiguous viewer locations.

**Requirements:** Prevent future implementation tickets from reintroducing the old layout.

**Dependencies:** U1.

**Files:**

- `docs/tickets/LASER-001-laser-controller-adapter.md`
- `docs/tickets/MAV-001-mavlink-adapter.md`
- `docs/tickets/VIEW-002-python-viewer.md`

**Approach:** Change adapter ticket paths from `services/...` to their top-level subsystem directories. Keep viewer wording compatible with the current transitional state by referencing `VIEW-001` where necessary rather than pretending the move already happened.

**Patterns to follow:** Existing ticket story schema in `.github/instructions/tickets.instructions.md`.

**Test scenarios:**

- Test expectation: none -- ticket documentation-only change with no executable behaviour.

**Verification:** Future-facing ticket paths match the chosen subsystem layout and do not create status drift in frontmatter.

### U4. Search And Board Verification

**Goal:** Verify that the documentation set no longer contains unresolved contradictions relevant to DOCS-006.

**Requirements:** Provides falsifiable completion evidence for the documentation reconciliation.

**Dependencies:** U1, U2, U3.

**Files:**

- `BOARD.md`
- `docs/tickets/DOCS-006-reconcile-layout-with-multi-subsystem-structure.md`

**Approach:** Search for stale layout tokens in governance docs, agent instructions, and scheduled tickets. Run the board consistency check after the ticket status move and story updates. Any remaining stale token should be either fixed or explicitly justified as historical/reference material.

Use this verification checklist for active guidance paths:

- Search `CLAUDE.md`, `AGENTS.md`, `README.md`, `.github/`, `docs/tickets/`, and `tracking-core/README.md` for stale canonical-layout claims involving `services/`, root `core/`, root `tests/`, root `config/`, `tracking-core/src/viewer`, and “root `.md` files are design research”.
- Treat root `tools/board/` references as current repository workflow guidance, not stale tracking implementation paths.
- Exclude historical/reference material under `docs/research/` and past learning entries under `docs/solutions/` unless the text is presented as current guidance.
- Move `DOCS-006` to `done` with the board script after the documentation edits are complete, then run `python3 tools/board/board_check.py`.

**Patterns to follow:** `tools/board/README.md` and existing ticket log conventions.

**Test scenarios:**

- Test expectation: none -- verification is search/lint based for documentation state, not unit-testable behaviour.

**Verification:** `python3 tools/board/board_check.py` passes after the final status move, and stale `services/` or root-layout references are either removed from active guidance or intentionally retained in historical/reference contexts.

---

## Risks And Mitigations

- **Risk:** Overcorrecting docs to describe target directories that do not exist yet. **Mitigation:** Mark current transitional paths explicitly and leave physical moves to their own tickets.
- **Risk:** Accidentally changing architectural boundaries while editing path wording. **Mitigation:** Preserve ADR-001 and ADR-008 process boundaries; change locations, not responsibilities.
- **Risk:** Duplicating DOCS-005 scope. **Mitigation:** Limit `.github/copilot-instructions.md` edits to layout contradictions only.

---

## Deferred Implementation Notes

- Exact prose should be concise and should avoid restating the whole README in every instruction file.
- If search finds stale paths only in historical documents under `docs/research/` or past solution entries, leave them unchanged unless they are presented as current guidance.
