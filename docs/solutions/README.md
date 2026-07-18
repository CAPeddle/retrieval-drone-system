# Solutions Library

Documented patterns, recipes, reviews, and recovery procedures captured from work on this project (or evaluated from external sources). Each solution is queryable by frontmatter — `applies_to`, `tags`, `problem_type` — so a future agent can find prior work without re-deriving it.

## Relationship to other doc collections

| Doc collection | Role |
|---|---|
| [`docs/adr/`](../adr/) | Architectural **decisions** — WHY we chose what we chose |
| `docs/solutions/` (this dir) | **Patterns, recipes, reviews, learnings** — HOW to do things, or what we observed |
| [`docs/research/`](../research/) | Historical v0.2 brainstorms — rank-5 reference per [CLAUDE.md §6](../../CLAUDE.md) |

## Categories

- [`workflow/`](workflow/) — Agent coordination, kanban tweaks, planning patterns
- [`cpp/`](cpp/) — Tracking-core C++ patterns and gotchas
- [`python/`](python/) — Viewer and tooling patterns
- [`calibration/`](calibration/) — Calibration drift recovery and procedures
- [`external-reviews/`](external-reviews/) — Critical reviews of external workflows or tools considered for borrowing
- [`hardware/`](hardware/) — Raspberry Pi / hardware setup gotchas and recovery procedures
- [`tooling-decisions/`](tooling-decisions/) — Language, library, or tool choices with durable rationale

## File format

See [`.claude/rules/solutions.md`](../../.claude/rules/solutions.md) for the required frontmatter schema and conventions (auto-applied by Claude Code when editing files under `docs/solutions/`).

## Index (most recent first)

| Date | File | Type | Applies to | Tags |
|---|---|---|---|---|
| 2026-07-18 | [cpp/2026-07-18-camera-centre-from-plane-homography.md](cpp/2026-07-18-camera-centre-from-plane-homography.md) | pattern | tracking-core | opencv, homography, plane-pose, decomposehomographymat, camera-centre, z-compensation, adr-010 |
| 2026-07-18 | [cpp/2026-07-18-opencv-svd-outputarray-matx-silent-failure.md](cpp/2026-07-18-opencv-svd-outputarray-matx-silent-failure.md) | pattern | tracking-core | opencv, svd, outputarray, matx, silent-failure, homography |
| 2026-07-18 | [workflow/2026-07-18-stacked-pr-merge-order-recovery.md](workflow/2026-07-18-stacked-pr-merge-order-recovery.md) | recovery | * | stacked-prs, github, merge-order, pull-requests, recovery, gh-cli |
| 2026-07-18 | [python/2026-07-18-artifact-metadata-derived-not-asserted.md](python/2026-07-18-artifact-metadata-derived-not-asserted.md) | pattern | tracking-core | calibration, extrinsics, artifact-contract, undistortion, metadata |
| 2026-07-18 | [cpp/2026-07-18-opencv-aruco-version-portability-seam.md](cpp/2026-07-18-opencv-aruco-version-portability-seam.md) | pattern | tracking-core | opencv, aruco, cmake, version-portability, preprocessor-seam |
| 2026-07-18 | [cpp/2026-07-18-composite-test-targets-model-occlusion.md](cpp/2026-07-18-composite-test-targets-model-occlusion.md) | pattern | tracking-core | testing, replay-harness, ball-detector, synthetic-compositing, occlusion |
| 2026-07-16 | [cpp/2026-07-16-spdlog-async-hot-path-logging.md](cpp/2026-07-16-spdlog-async-hot-path-logging.md) | pattern | tracking-core | spdlog, logging, hot-path, compile-time-gating, cmake |
| 2026-07-15 | [hardware/pi3b-camera-node-undervoltage.md](hardware/pi3b-camera-node-undervoltage.md) | recovery | camera-node | raspberry-pi, power, under-voltage, camera, hardware |
| 2026-06-22 | [tooling-decisions/2026-06-22-gh-cli-authentication-custom-ssh-host.md](tooling-decisions/2026-06-22-gh-cli-authentication-custom-ssh-host.md) | tooling_decision | * | gh-cli, github, authentication, ssh, git-remote |
| 2026-06-04 | [workflow/session-decision-log-2026-05-31.md](workflow/session-decision-log-2026-05-31.md) | reference | * | agent-workflow, decision-log, questions, session-history |
| 2026-06-04 | [workflow/tracking-core-rename-and-build-overhaul-handoff.md](workflow/2026-06-04-tracking-core-rename-and-build-overhaul-handoff.md) | recipe | tracking-core | workflow, build-system, handoff, ticket-state, validation |
| 2026-05-31 | [external-reviews/compound-engineering-plugin-review.md](external-reviews/2026-05-31-compound-engineering-plugin-review.md) | review | * | agent-workflow, plan-driven-development, parallel-subagents, kanban |
