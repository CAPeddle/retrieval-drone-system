# Plan 002: Fix DOCS-006 Review Findings

> Address the seven remaining review findings on the DOCS-006 layout reconciliation
> change without expanding scope beyond documentation and board hygiene.

---

## Context

DOCS-006 reconciled repository layout guidance with the multi-subsystem structure
documented in `README.md`. A follow-up review found seven remaining issues:
one contradiction in `CLAUDE.md §11.3`, three completion/hygiene issues, two
British English violations, and one large line-ending diff in `.github`
instruction files.

This plan is for implementation in the worktree at
`.worktrees/fix/docs-006-layout` on branch `fix/docs-006-layout`. Do not apply
these steps in the original `master` checkout.

## Acceptance

- [ ] `CLAUDE.md §11.3` distinguishes repo-local workflow artefacts from
  external delivery outputs under `/mnt/user-data/outputs/`.
- [ ] The DOCS-006 story, spec, implementation plan, and this review-fix plan
  are part of the final change set and are not left untracked.
- [ ] Active layout maps mention `docs/superpowers/` where they describe the
  workflow artefact layout, or explicitly defer to `README.md` as the canonical
  map.
- [ ] `Done (recent)` in `BOARD.md` is back to the repository convention of at
  most 10 visible done items, or the deviation is explicitly justified in the
  final handoff.
- [ ] New/changed prose uses British English: `artefact` / `artefacts`, not
  `artifact` / `artifacts`.
- [ ] The `.github` line-ending change is either reverted to the prior line
  endings or documented as an intentional mechanical normalisation in the
  final change summary.
- [ ] `python3 tools/board/board_check.py`, `git diff --check`, and targeted
  stale-reference searches pass.

## Plan

### Sequence Map

```text
U1 -> U2 -> U3 -> U4 -> U5 -> U6 -> U7
```

Run the units in order. U6 depends on the earlier edits because line-ending
handling can otherwise obscure the real diff. U7 is the final verification
pass.

### U-IDs

#### U1. Fix `CLAUDE.md §11.3` Output Path Guidance

Goal: remove the contradiction where `CLAUDE.md §11.3` says real deliverables
go under `/mnt/user-data/outputs/`, while DOCS-006 correctly stores repo
workflow artefacts under `docs/superpowers/`.

Files:

- `CLAUDE.md`

Implementation steps:

1. Open `CLAUDE.md §11.3 File creation conventions`.
2. Replace the absolute rule that all real deliverables go under
   `/mnt/user-data/outputs/` with a two-case rule:
   - Repository workflow artefacts live in their documented repo paths. For
     design-tier tickets, that means `docs/superpowers/specs/` and
     `docs/superpowers/plans/`, with links from the story file frontmatter.
   - External delivery outputs that are not meant to be committed to the repo
     go under `/mnt/user-data/outputs/`, mirroring the intended project layout.
3. Keep the existing guidance that section drafts may be inline and final
   assembled artefacts should become real files.
4. Use British English: `artefact`, `artefacts`, `behaviour`.

Verification for U1:

- Search `CLAUDE.md` for `/mnt/user-data/outputs/` and confirm it is no longer
  described as the destination for repo workflow artefacts.
- Search changed `CLAUDE.md` prose for `artifact` and `artifacts`; there should
  be no American spellings in new/current guidance.

#### U2. Include The New DOCS-006 Artefacts In The Change Set

Goal: ensure the files created for the design-tier DOCS-006 workflow are not
left untracked.

Files:

- `docs/tickets/DOCS-006-reconcile-layout-with-multi-subsystem-structure.md`
- `docs/superpowers/specs/DOCS-006-layout-reconciliation-design.md`
- `docs/superpowers/plans/DOCS-006-layout-reconciliation-plan.md`
- `docs/plans/002-fix-docs-006-review-findings.md`

Implementation steps:

1. Run `git status --short` from `.worktrees/fix/docs-006-layout`.
2. Confirm the four files above appear in the working tree and are intended.
3. Do not delete, rename, or move them.
4. If preparing a commit later, stage these three files along with the tracked
   documentation edits.

Verification for U2:

- `git status --short` shows the four files as present in the change set.
- `docs/tickets/DOCS-006-reconcile-layout-with-multi-subsystem-structure.md`
  frontmatter still links `spec:` to
  `docs/superpowers/specs/DOCS-006-layout-reconciliation-design.md` and `plan:`
  to `docs/superpowers/plans/DOCS-006-layout-reconciliation-plan.md`.

#### U3. Reconcile Layout Maps And `docs/superpowers/` Coverage

Goal: eliminate the remaining layout-map drift noted at `CLAUDE.md:482`, where
active maps omit `docs/superpowers/` even though `README.md` documents workflow
artefacts there.

Files:

- `README.md`
- `CLAUDE.md`

Implementation steps:

1. Treat `README.md` as the canonical orientation map for the repository root.
2. In `README.md ## Layout`, add `docs/superpowers/` under `docs/` with concise
   wording such as `Design-tier specs and plans`.
3. In `CLAUDE.md §8.7`, avoid maintaining a second full tree that can drift.
   Prefer a short location table or summary that points to `README.md` for the
   full repository layout and lists only load-bearing workflow paths:
   - `docs/adr/`
   - `docs/design/`
   - `docs/tickets/`
   - `docs/superpowers/specs/`
   - `docs/superpowers/plans/`
   - `tools/board/`
4. Preserve root `tools/board/` as repository workflow tooling.
5. Do not reintroduce root `core/`, root `services/`, root `tests/`, or root
   `config/` as canonical implementation directories.

Verification for U3:

- `README.md ## Layout` includes `docs/superpowers/`.
- `CLAUDE.md §8.7` includes `docs/superpowers/specs/` and
  `docs/superpowers/plans/`, or explicitly defers the full layout to
  `README.md` while naming those paths in workflow guidance.
- Search active guidance for stale root layout claims:
  - `core/` as a root implementation directory
  - `services/` as the canonical adapter home
  - root `tests/` or root `config/` as current canonical directories

#### U4. Restore `Done (recent)` To The Board Convention

Goal: resolve the review finding that `BOARD.md` has 11 items in `Done
(recent)`, while `README.md` says the section is auto-archived when it exceeds
10.

Files:

- `BOARD.md`
- `docs/board-archive.md`
- Any ticket story files touched by the archive script, if the script updates
  them.

Implementation steps:

1. From `.worktrees/fix/docs-006-layout`, run:

   ```bash
   python3 tools/board/ticket_archive.py --keep 10
   ```

2. Inspect the resulting diff.
3. Confirm exactly the oldest excess Done item moved from `BOARD.md` to
   `docs/board-archive.md`.
4. Do not manually bulk-edit the board unless the script fails; if it fails,
   capture the failure and make the smallest equivalent hand edit.

Verification for U4:

- `BOARD.md` has at most 10 `- [x]` entries under `Done (recent)`.
- `docs/board-archive.md` contains the archived item with its original date.
- `python3 tools/board/board_check.py` passes after archiving.

#### U5. Fix British English Violations

Goal: replace the American spellings called out by review and prevent nearby
new prose from drifting.

Files:

- `.github/copilot-instructions.md`
- `docs/superpowers/specs/DOCS-006-layout-reconciliation-design.md`
- Any file edited in U1-U4 where new prose contains the same spelling issue.

Implementation steps:

1. In `.github/copilot-instructions.md`, replace `artifacts` with
   `artefacts` in current guidance.
2. In `docs/superpowers/specs/DOCS-006-layout-reconciliation-design.md`,
   replace `artifact` with `artefact`.
3. Search the current diff for `artifact` and `artifacts`; fix active/new prose.
4. Do not edit historical quotations solely for spelling unless they are part
   of the current DOCS-006 guidance diff.

Verification for U5:

- Targeted search over changed docs finds no American spelling in active prose:

  ```bash
  git grep -n -E '\bartifacts?\b' -- CLAUDE.md README.md .github docs/superpowers docs/tickets/DOCS-006-reconcile-layout-with-multi-subsystem-structure.md
  ```

  If matches remain in historical/reference material, document why they were
  left alone.

#### U6. Resolve Or Explicitly Accept `.github` Line-Ending Churn

Goal: make the CRLF-to-LF change in `.github` instruction files deliberate
rather than accidental review noise.

Files:

- `.github/copilot-instructions.md`
- `.github/instructions/python-tooling.instructions.md`
- `.github/instructions/tracking-architecture.instructions.md`

Preferred implementation:

1. Preserve the repository's existing line-ending style for each file unless
   there is a documented reason to normalise it now.
2. If the only intended changes are a few prose lines, restore each file's
   previous line endings and keep only the content changes.
3. If preserving previous line endings is awkward or the repo already prefers
   LF, keep LF but add an explicit final-change note: `.github` instruction
   files were mechanically normalised to LF while editing layout guidance.

How to decide:

- If `git diff --numstat` shows nearly every line in a `.github` file changed
  despite a small prose edit, prefer reverting line-ending churn.
- If the diff is already small after edits, no separate action is needed.

Verification for U6:

- `git diff -- .github/copilot-instructions.md .github/instructions/python-tooling.instructions.md .github/instructions/tracking-architecture.instructions.md` is reviewable.
- The final handoff either says `line endings preserved` or explicitly says
  `line endings normalised to LF`.

#### U7. Run Final Verification And Prepare Handoff

Goal: prove the seven review findings are addressed and leave a concise handoff
for the next agent or user.

Commands:

```bash
python3 tools/board/board_check.py
git diff --check
git status --short
```

Targeted searches:

```bash
git grep -n -F '/mnt/user-data/outputs/' -- CLAUDE.md
git grep -n -E '\bartifacts?\b' -- CLAUDE.md README.md .github docs/superpowers docs/tickets/DOCS-006-reconcile-layout-with-multi-subsystem-structure.md
git grep -n -E 'services/|root `core/`|root `tests/`|root `config/`|root \.md files are design research' -- CLAUDE.md AGENTS.md README.md .github docs/tickets tracking-core/README.md
```

Interpretation rules:

- `/mnt/user-data/outputs/` may remain only for external, non-repo delivery
  outputs.
- `services/` may remain only in historical context, rejected/stale wording, or
  explanations that it is not canonical.
- `tracking-core/src/viewer/` may remain only as transitional current-state
  wording that points to `VIEW-001` for promotion.
- `tools/board/` references are valid and should remain.

Final handoff checklist:

- List each of the seven review findings and the exact file(s) changed to
  address it.
- Include verification command results.
- State whether `.github` line endings were preserved or normalised.
- State that no commits were made unless the user explicitly requested one.

## Risks

- **Scope creep:** Do not rewrite broad Copilot guidance; DOCS-005 owns that.
- **Layout overstatement:** Do not claim future subsystem directories contain
  production code before their implementation tickets create it.
- **Board drift:** Use `tools/board/ticket_archive.py` before hand-editing
  `BOARD.md`.
- **Review noise:** Resolve line endings before final review so content changes
  remain visible.
