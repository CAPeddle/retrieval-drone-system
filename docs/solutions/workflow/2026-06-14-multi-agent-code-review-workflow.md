---
title: Multi-Agent Code Review Findings for OpenCode Configuration
date: 2026-06-14
module: opencode agent configuration and documentation
problem_type: workflow_issue
component: development_workflow
severity: medium
applies_when:
  - reviewing multi-file agent configuration changes
  - setting up multi-agent review pipelines for developer tooling
  - auditing agent instruction files for contradiction and redundancy
tags:
  - code-review
  - multi-agent
  - opencode
  - workflow-improvement
  - agent-configuration
---

# Multi-Agent Code Review Findings for OpenCode Configuration

## Context

A comprehensive multi-agent code review was run on the `feat/z2-opencode-web-setup` branch — 12 files, +47/-24 lines across OpenCode configuration, agent instructions, commands, and project documentation. Six parallel review agents (correctness, testing, maintainability, project-standards, ce-agent-native-reviewer, ce-learnings-researcher) were dispatched, each returning severity-ranked findings. The review surfaced issues spanning stale documentation, config contradictions, redundant workflow steps, naming inconsistencies, missing safety guards, and unnecessary ceremony — a broader and more structured set than any single-reviewer pass would have caught.

## Guidance

**Dispatch parallel, role-specialised review agents rather than a single "review this" prompt.** Each agent should have a distinct perspective and return severity-ranked findings. Key roles that compose well:

- **Correctness** — semantic errors, logic bugs, edge cases
- **Testing** — missing tests, untestable code, test-quality issues
- **Maintainability** — naming, structure, cohesion, duplication
- **Project-standards** — convention drift, ADR/doc consistency
- **Agent-native architecture** — agent parity gaps, permission mismatches
- **Learnings/solutions library** — contradictions with previously documented solutions

**Merge and deduplicate findings after collection.** The agents will overlap. A maintainer (or the implementing agent) must consolidate overlapping findings, promote cross-cutting patterns, and resolve conflicts before actioning.

**Implement findings in order of severity.** Higher-severity items are addressed first; lower-severity items are batched or deferred explicitly.

## Why This Matters

- **Single-reviewer blind spots are systematic.** A correctness-focused reviewer won't catch stale docs in a help command. A maintainability reviewer won't flag missing schema compliance in YAML frontmatter. Parallel specialists cover these gaps without requiring the reviewer to hold every concern simultaneously.
- **Severity-ranked findings enable proportional response.** Without severity, every finding gets equal weight — leading to either over-investment in trivial issues or missed critical ones.
- **Deduplication prevents conflicting fixes.** Two reviewers may flag the same root cause from different angles. Without merge/dedup, the implementer risks fixing the same thing twice or applying conflicting remedies.

## When to Apply

- **Any multi-file change that touches cross-cutting concerns** (config + docs + instructions + code). Parallel review shines here — each reviewer's lens catches different surfaces.
- **Changes that modify project conventions or agent workflows.** A standards reviewer and an agent-native reviewer will catch different aspects of the same change.
- **Before creating a PR for medium-to-large changes.** For trivial single-file changes, a single reviewer is sufficient. The parallel overhead pays for itself when the change spans three or more files or two or more concern boundaries.
- **When retrofitting learnings into documentation.** The learnings-researcher reviewer specifically checks whether the change contradicts or duplicates existing `docs/solutions/` entries.

## Examples

**Before (single generic review prompt):**

> "Review this PR for me"

Result: surface-level comments, missed the stale `help.md` description, missed the YAML frontmatter schema violation, missed the redundant `agent.build.model` override.

**After (parallel specialist dispatch):**

Six agent definitions, each with distinct SYSTEM + USER instructions targeting one review lens. Result: 10 findings surfaced, severity-ranked, deduplicated. Nine implemented. M-10 explicitly deferred with rationale. Each finding tied to a specific `file:line` and a specific rule or contract.

**Merge/dedup example from the session:**

M-03 (reviewer A: "explore output duplicated in start-ticket step 4") and M-11 (reviewer B: "start-ticket ceremony overhead") were merged: removing step 4 solved both, because the ceremony was the packet-compression step itself.

**File references from the implementation:**

- `.opencode/agents/explore.md` — merged context-packet format into output, added guard for missing solution docs
- `.opencode/commands/start-ticket.md` — reduced from 7 steps to 5, removed redundant context-packet compression and tool-plan steps
- `.github/instructions/tickets.instructions.md` — added execution escape hatch ("if execution reveals complexity that requires new searching, stop and describe the gap before continuing")
- `opencode.json` — removed redundant `agent.build.model` override (matched default)

## Related

- [Compound-Engineering Plugin Review](../external-reviews/2026-05-31-compound-engineering-plugin-review.md) — evaluates the 51-agent review pipeline that inspired this pattern
- [Session Decision Log Template](../workflow/session-decision-log-2026-05-31.md) — session tracking used during the review
