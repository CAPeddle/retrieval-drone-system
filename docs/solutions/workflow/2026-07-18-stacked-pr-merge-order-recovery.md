---
title: "Stacked-PR chains merge bottom-up with explicit retargeting; recovery lives at the chain head"
captured: 2026-07-18
applies_to: ["*"]
tags: [stacked-prs, github, merge-order, pull-requests, recovery, gh-cli]
problem_type: recovery
source: internal
component: github-workflow
resolution_type: manual_recovery
applies_when: "Merging a stacked chain of GitHub PRs (each PR based on the previous PR's still-open feature branch), or diagnosing a chain where a stacked PR was closed unmerged or merged into a feature-branch base"
symptoms:
  - "A CLOSED stacked PR shows mergedAt: null (closed, never merged) - its phase is missing from master"
  - "A MERGED PR's baseRefName is a feature branch, not master - its phase merged sideways into an unmerged branch"
  - "gh pr reopen on the closed PR fails with 'GraphQL: Could not open the pull request'"
---

# Stacked-PR chains merge bottom-up with explicit retargeting; recovery lives at the chain head

## Summary

A stacked PR chain (each PR based on the previous PR's feature branch) is only
mergeable bottom-up with an explicit retarget step between merges: merge the
base PR, retarget the *next* PR's base to master, merge, repeat. GitHub does
not do this for you, and the UI actively misleads: after the base PR merges,
the remaining stacked PRs still show their stale feature-branch bases, so the
merge button performs a sideways merge into an unmerged branch and "Close"
looks like reasonable cleanup. When a chain is mishandled this way, the work is
never lost — stacked heads are cumulative, so the chain-head branch contains
every phase — and the reliable recovery is one fresh PR from the chain head
against master, not reopening or replaying the per-phase PRs. Reach for this
when merging any stacked chain, when a MERGED PR's `baseRefName` is a feature
branch, or when a CLOSED stacked PR shows `mergedAt: null`.

## Context

An autonomous phase-per-PR execution delivered four phases of tracking-core
work as a stacked chain on CAPeddle/retrieval-drone-system:

- PR #12 `feat/trk-014-016-track-lifecycle` → base `master`
- PR #13 `feat/trk-017-019-coordinate-mapping` → base = #12's branch
- PR #14 `feat/trk-020-safety-predicate` → base = #13's branch
- PR #15 `feat/trk-021-export-viewer` → base = #14's branch

Each PR body carried the instruction "Stacks on PR #N (retarget to master
after #N merges)". The instruction was correct; it was also the only place the
merge choreography lived, and it did not survive contact with the merge
buttons.

## What happened

The event log (`gh api repos/CAPeddle/retrieval-drone-system/issues/N/events`)
tells the story in under a minute of wall clock:

- **21:26:27** — #12 merged into master. Correct: the base of the stack goes
  first.
- **21:26:34** — seven seconds later, #13 was **closed unmerged**. After #12's
  merge, GitHub showed #13 against its stale base; closing it looked like
  tidying up, but it dropped phase B from the merge path.
- **21:27:17** — #14 was **merged** — but its base was still #13's feature
  branch. Phase C merged *sideways* into an unmerged branch, not into master.
  GitHub reports this as MERGED with no complaint.
- **21:27:24** — #15 closed unmerged.

Net state: master contained phase A only. Nine commits (phases B, C, D) were
stranded — but safe, because the chain-head branch
`feat/trk-021-export-viewer` was built on top of every prior phase and
therefore contained the complete chain. Verified:

```bash
# false — the chain head is not in master's ancestry
git merge-base --is-ancestor origin/feat/trk-021-export-viewer origin/master

# the 9 stranded commits (phases B+C+D)
git log --oneline origin/master..origin/feat/trk-021-export-viewer
```

The fastest diagnosis of the whole mess is one command:

```bash
gh pr list --state all --json number,state,baseRefName,headRefName,mergedAt
```

Two tell-tales in the output: a CLOSED stacked PR with `mergedAt: null`
(closed, not merged — its phase never landed), and a MERGED PR whose
`baseRefName` is a feature branch rather than master (a sideways merge — its
phase landed somewhere that is not the mainline).

## Approach / Recovery

The instinctive fix — reopen the closed PRs and merge them in order — does not
work:

```bash
gh pr reopen 15
# GraphQL: Could not open the pull request
```

GitHub refuses to reopen a PR once the stacked-base bookkeeping has gone stale
(the base branch state it was tracking no longer matches). Do not fight this.

The working recovery exploits the cumulative property of stacked heads: the
chain head's diff against master is, by construction, exactly the union of
every unmerged phase. One fresh PR restores everything:

```bash
gh pr create --base master --head feat/trk-021-export-viewer \
  --title "feat: phases B-D (TRK-017..021) — chain-head recovery" \
  --body "Restores the stranded stack. Per-phase review context: #13, #14, #15."
```

The diff was verified to be exactly the missing phases (+4784/−685). Linking
the closed per-phase PRs in the body preserves the per-phase review context —
the reviews happened and are still readable; only the merge path was broken.
One merge of this PR restores master.

The sideways merge commit that #14 left on #13's branch needs no surgery: it
is not in the chain head's ancestry (the chain head was built from #14's
branch *before* that merge commit had anywhere to flow from), and #13's branch
is deleted once recovery lands.

## Prevention

The root cause is a workflow gap, not an operator error: after the base PR of
a stack merges, GitHub presents each remaining PR against its stale
feature-branch base, the merge button on such a PR merges sideways, and
closing it looks like cleanup. The correct move — **Edit base → master, then
merge** — is not surfaced anywhere in the UI flow. A one-line instruction in
each PR body demonstrably did not prevent the mistake; choreography that must
happen in sequence needs to be stated as a sequence, at the moment of handoff.

The merge choreography, explicitly:

```bash
# 1. Merge the base of the stack
gh pr merge 12 --squash

# 2. Retarget the NEXT PR's base to master — before touching its merge button
gh pr edit 13 --base master

# 3. Merge it, then repeat retarget→merge up the chain
gh pr merge 13 --squash
gh pr edit 14 --base master
gh pr merge 14 --squash
gh pr edit 15 --base master
gh pr merge 15 --squash
```

Generalised rules:

1. **Merge stacked chains strictly bottom-up with an explicit retarget between
   every step**: merge the base PR → retarget the next PR's base to master →
   merge → repeat. The retarget is not optional bookkeeping; it is the step
   that makes the next merge land on the mainline.
2. **Never merge a PR whose base is an unmerged feature branch.** That is a
   sideways merge by definition, whatever the UI's merge button implies.
3. **A CLOSED stacked PR with `mergedAt: null` is a red flag to investigate,
   never cleanup.** Closed-not-merged means a phase never landed; find out
   where it went before deleting anything.
4. **Recovery is always available from the chain head.** Stacked heads are
   cumulative, so the head branch contains every phase; prefer one fresh
   chain-head→master PR over trying to reopen or replay the per-phase chain —
   reopening will likely be refused anyway.
5. **When an agent hands over a stacked chain, it must state the merge
   choreography as explicit numbered steps in the handoff message** — not only
   in PR bodies. Instructions embedded per-PR are invisible at the moment the
   operator is clicking merge buttons in sequence.

## Trade-offs

- The chain-head recovery PR trades per-phase merge granularity for certainty:
  master gains phases B–D in a single merge rather than three, so `git log`
  attribution to individual phases lives in the branch history and the linked
  closed PRs, not in distinct merge commits. Given that reopening was refused
  outright, the alternative (recreating three per-phase PRs with rewritten
  bases) buys granularity at the cost of three more merges and three more
  opportunities for the same base-retarget mistake.
- Stating merge choreography in the handoff message duplicates what the PR
  bodies say. The duplication is the point: the PR-body copy serves whoever
  lands on one PR in isolation; the handoff copy serves the operator executing
  the sequence. This incident shows the second copy is the load-bearing one.
- Stacked chains remain worth the risk for phase-per-PR review quality — the
  alternative (one monolithic PR) forfeits per-phase review entirely. The cost
  is that the merge step becomes a choreographed procedure rather than a
  button.

## Source links

- Diagnosis: `gh pr list --state all --json number,state,baseRefName,headRefName,mergedAt` on CAPeddle/retrieval-drone-system; per-PR event logs via `gh api repos/CAPeddle/retrieval-drone-system/issues/{12,13,14,15}/events`
- The chain: PR #12 (`feat/trk-014-016-track-lifecycle`, merged to master), PR #13 (`feat/trk-017-019-coordinate-mapping`, closed unmerged), PR #14 (`feat/trk-020-safety-predicate`, merged sideways into #13's branch), PR #15 (`feat/trk-021-export-viewer`, closed unmerged)
- Recovery PR: chain head `feat/trk-021-export-viewer` → master (+4784/−685), body linking #13/#14/#15 for per-phase review context
- Stranded-commit verification: `git merge-base --is-ancestor` and `git log origin/master..origin/feat/trk-021-export-viewer` as shown above
- Recovery landed as PR #16 (chain head -> master); superseded closed PRs #13/#14/#15 hold the per-phase reviews
- Related: [gh CLI authentication with custom SSH host alias](../tooling-decisions/2026-06-22-gh-cli-authentication-custom-ssh-host.md) — another GitHub/gh workflow gotcha on this repo
- Related: [tracking-core rename/build-overhaul handoff pattern](../workflow/2026-06-04-tracking-core-rename-and-build-overhaul-handoff.md) — a different case of keeping sequenced work valid across a handoff boundary
