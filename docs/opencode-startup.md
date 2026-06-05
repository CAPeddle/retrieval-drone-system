# OpenCode Startup — Drone Ball Retrieval System

> Bootstrap guide for working this repository through the **OpenCode** IDE on a new machine,
> using **OpenCode Zen** models for execution and a separate **Claude Code** subscription for
> design and final review. Self-contained: this file plus the repo is all you need.
>
> Companion to [`AGENTS.md`](../AGENTS.md) (the cross-vendor entry point) and
> [`CLAUDE.md`](../CLAUDE.md) (the full operating contract). This document does **not** restate
> the architecture or the ADRs — it explains how the existing workflow runs on OpenCode.

---

## 0. TL;DR bootstrap checklist

```text
[ ] Install OpenCode and open this repo as the project root.
[ ] Authenticate OpenCode Zen (opencode auth login → OpenCode Zen, or /connect, paste API key).
[ ] Open opencode.json → §3 here → replace the model IDs with the real ones from your Zen catalogue.
[ ] Start OpenCode in the repo. Confirm AGENTS.md auto-loaded and CLAUDE.md came in via `instructions`.
[ ] Run /board to confirm the board scripts work (python on PATH).
[ ] Optional: recreate local `.remember/` notes from `docs/opencode-session-continuity.md`.
[ ] Run /next-ticket to see what is ready.
[ ] Keep Claude Code installed separately for design-tier and final-review work (§2).
```

Everything below explains those lines.

---

## 1. What OpenCode loads automatically (and the one gotcha)

OpenCode reads instruction files the same way Cursor reads rules. The behaviour that matters here:

| File | Loaded? | Notes |
|---|---|---|
| `AGENTS.md` (repo root) | **Yes, automatically** | OpenCode's native rules file. Already cross-vendor and lists OpenCode by name. |
| `CLAUDE.md` | **No — not when `AGENTS.md` exists** | OpenCode precedence: *"if you have both `AGENTS.md` and `CLAUDE.md`, only `AGENTS.md` is used."* |
| `.github/instructions/*.instructions.md` | No | These are Copilot's file-scoped instructions. |

**The gotcha:** your deep operating contract — persona/modes, hot-path discipline (§7), source-of-truth hierarchy (§6), pitfalls (§10), glossary — lives in `CLAUDE.md`, which OpenCode would otherwise ignore.

**The fix is already wired** in [`opencode.json`](../opencode.json) via the `instructions` field, which *combines* with `AGENTS.md`:

```json
"instructions": ["CLAUDE.md", ".github/instructions/*.instructions.md"]
```

So OpenCode loads `AGENTS.md` (auto) **plus** `CLAUDE.md` and all four `.github/instructions` files (cpp-hot-path, python-tooling, tracking-architecture, adr-governance, …). The full contract and the file-scoped coding standards are in context. Confirm this on first run — if CLAUDE.md content is missing from the model's behaviour, the `instructions` glob is the first thing to check.

---

## 2. The two-tool division of labour

You are running **two** tools, each where it is permitted and strongest. They share one repo, one board, one set of story files — so the workflow is unified regardless of which tool is driving.

> **Why two tools, not one:** Anthropic prohibits driving OpenCode with a Claude Pro/Max
> subscription, and OpenCode stopped bundling the plugins that enabled it as of v1.3.0. So the
> Claude subscription cannot legitimately run *inside* OpenCode. It runs in **Claude Code**, where
> it is allowed. OpenCode runs **Zen** models (pay-per-token, billed by OpenCode Zen).

| Work | Tool | Why |
|---|---|---|
| Execute `mechanical` + `small` tickets | **OpenCode (Zen)** | High volume, well-specified, cheap. |
| Execute a design-tier *plan* (after the plan exists) | **OpenCode (Zen)** | Plan is the spec; execution is mechanical. |
| Read-only research (Explore / best-practices) | **OpenCode (Zen)** | High volume, disposable context. |
| First-pass code & safety review | **OpenCode (Zen)** | Cheap reviewer/safety-reviewer subagents. |
| Design-tier **brainstorm → spec → plan** | **Claude Code** | Subscription value; `superpowers:brainstorming` + `writing-plans` skills. |
| **Final** correctness / safety sign-off | **Claude Code** | The cardinal-sin gate (false positive on `safe_for_control`) deserves the strongest model and a human in the loop. |
| ADR authoring / supersession | **Claude Code** | Load-bearing prose; §8 governance. |

Rule of thumb: **OpenCode does the typing; Claude Code does the deciding.** When an OpenCode session hits a design question it cannot resolve from the ADRs, it stops and hands back to you to run the design phase in Claude Code (the `/start-ticket` command enforces this for `design`-tier tickets).

---

## 3. Models — verify these before first run

`opencode.json` ships with placeholder Zen model IDs. **The OpenCode Zen catalogue changes; I could not verify the exact current IDs.** Before your first real session, list the models available on your Zen account (the model picker in the TUI, or `opencode auth login` → OpenCode Zen) and replace the IDs.

Provider prefix is `opencode/`. Suggested tiering (edit `opencode.json` to match your catalogue):

| Slot in `opencode.json` | Role | Pick a model that is… |
|---|---|---|
| `model` (default) + `agent.build` / `agent.plan` | Execution workhorse | A strong, cost-reasonable **coding** model. |
| `small_model` | Titles, cheap housekeeping | The cheapest fast model. |
| `@explore`, `@researcher` (agent frontmatter) | High-volume read-only | A cheap/fast model — they read a lot and are disposable. |
| `@reviewer`, `@safety-reviewer` | First-pass review | A stronger-reasoning model; review quality matters more than speed. |

The placeholders use one ID everywhere (`opencode/qwen3-coder`) so the config loads and runs out of the box; split the tiers once you have confirmed real IDs. The **Claude tier is deliberately absent from `opencode.json`** — that work lives in Claude Code, so OpenCode never needs an `anthropic/*` or `opencode/claude-*` entry.

---

## 4. How Claude/Copilot concepts map onto OpenCode primitives

The existing workflow was written for Claude Code skills and Copilot. Here is the translation. **No behaviour changes — only the mechanism.**

| Concept in the existing docs | OpenCode primitive | Where |
|---|---|---|
| Claude `Explore` subagent | `@explore` subagent | [`.opencode/agents/explore.md`](../.opencode/agents/explore.md) |
| `ce-best-practices-researcher` / web research | `@researcher` subagent | [`.opencode/agents/researcher.md`](../.opencode/agents/researcher.md) |
| `ce-correctness-reviewer` + `ce-performance-reviewer` | `@reviewer` subagent | [`.opencode/agents/reviewer.md`](../.opencode/agents/reviewer.md) |
| Adversarial "What if?" / safety review (CLAUDE.md §2 Review mode) | `@safety-reviewer` subagent | [`.opencode/agents/safety-reviewer.md`](../.opencode/agents/safety-reviewer.md) |
| "Use the question tool / checkpoint after each ticket" | Primary agent pauses and asks you in the TUI | built-in |
| `ce-work` execution loop | The `build` primary agent + the commands below | [`.opencode/commands/`](../.opencode/commands/) |
| `superpowers:brainstorming` / `writing-plans` | **Run in Claude Code** (produces the spec/plan artifacts) | §2 |
| Tier `mechanical` / `small` / `design` | Unchanged — see AGENTS.md "Workflow at a glance" | `AGENTS.md` |
| Routine board scripts (`ticket_*.py`, `board_check.py`) | Plain shell, explicitly allow-listed in `opencode.json` | unchanged |

### Custom commands (type `/` in the TUI)

| Command | Does | File |
|---|---|---|
| `/board` | Lints the board and summarises columns | [`.opencode/commands/board.md`](../.opencode/commands/board.md) |
| `/next-ticket` | Finds the next ready ticket (deps all done) | [`.opencode/commands/next-ticket.md`](../.opencode/commands/next-ticket.md) |
| `/start-ticket TRK-003` | Tier-gates, pre-flights via `@explore`, moves to in-progress | [`.opencode/commands/start-ticket.md`](../.opencode/commands/start-ticket.md) |
| `/finish-ticket TRK-003` | Verifies, dispatches reviewers, marks done, lints | [`.opencode/commands/finish-ticket.md`](../.opencode/commands/finish-ticket.md) |
| `/review` | First-pass review of the current `git diff` | [`.opencode/commands/review.md`](../.opencode/commands/review.md) |
| `/ticket-archive` | Archives older Done tickets, keeping the newest 10 visible | [`.opencode/commands/ticket-archive.md`](../.opencode/commands/ticket-archive.md) |
| `/agents` | Lists the four OpenCode subagents and when to dispatch them | [`.opencode/commands/agents.md`](../.opencode/commands/agents.md) |
| `/help` | Shows the command menu from inside the OpenCode TUI | [`.opencode/commands/help.md`](../.opencode/commands/help.md) |

> Files live under `.opencode/agents/` and `.opencode/commands/` (**plural** — that is OpenCode's
> convention; singular directories are silently ignored). Subagents are invoked by `@name` mention
> or automatically by the primary agent; commands are invoked by `/name`.

---

## 5. The work loop on OpenCode

This is the existing v0.3 loop ([`docs/v0.3-execution-guide.md`](v0.3-execution-guide.md)) and session prompt ([`docs/implementation-session-prompt.md`](implementation-session-prompt.md)), re-homed onto OpenCode primitives.

```text
                         ┌─────────────────────────────────────────────┐
   /next-ticket  ───────▶│  ticket ready?  (deps all `status: done`)   │
                         └───────────────┬─────────────────────────────┘
                                         │
                    design tier? ────────┼──────── yes ──▶  go to Claude Code:
                                         │                   brainstorm → spec → plan
                                         │                   (artifacts land at
                                         │                    docs/superpowers/specs|plans/)
                                         │                   then return here to execute
                                       no│
                                         ▼
   /start-ticket ID  ──▶  @explore pre-flight  ──▶  move to in-progress  ──▶  brief + go-ahead
                                         │
                                         ▼
                          execute Plan U-ID steps (U1 → U2 → …)
                          build + tests every 2–3 steps
                                         │
                                         ▼
   /finish-ticket ID ──▶  verify (paste output)  ──▶  @reviewer (+ @safety-reviewer if
                          ADR-007 / hot-path / coord / detection)  ──▶  fix blockers
                                         │
                          safety-critical? ──▶  final sign-off in Claude Code
                                         │
                                         ▼
                          ticket_move → done  ──▶  board_check  ──▶  /next-ticket
```

**Resuming after a break** is unchanged: `/board`, look at In Progress and Done (recent), then `/next-ticket`. All state is in the repo files; no session memory is required (this is the same guarantee the execution guide makes).

`.remember/` may still be useful as local scratch, but it is deliberately gitignored. Do not depend on it for portable startup. If you want local continuity notes on the new machine, recreate them from [`docs/opencode-session-continuity.md`](opencode-session-continuity.md) and promote anything durable into a committed story log, ADR, solution doc, or startup-doc edit.

**A ready-made session opener** still exists at [`docs/implementation-session-prompt.md`](implementation-session-prompt.md). Two edits when pasting it into OpenCode: read instructions are already loaded (no need to "read repo memory"), and replace skill names with the OpenCode equivalents from §4 (`Explore` → `@explore`, `ce-*-reviewer` → `@reviewer`/`@safety-reviewer`).

---

## 6. What we borrow from the agentic-CD playbooks (and what we don't)

You asked to consider three external workflows:

- [bdfinst/agentic-dev-team](https://github.com/bdfinst/agentic-dev-team) — `specs → plan → build → pr`, persona plan-review, pre-commit `/code-review` gate.
- [beyond.minimumcd.org/docs/agentic-cd](https://beyond.minimumcd.org/docs/agentic-cd/) — agent changes meet the same bar as human changes; intent as versioned artifacts; separate coding vs review agents; small batches.
- [EveryInc/compound-engineering-plugin](https://github.com/EveryInc/compound-engineering-plugin) — already reviewed in-repo: [`docs/solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md`](solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md).

Per that in-repo review and the chosen direction, the posture is **re-home the existing flow and cherry-pick** — not wholesale adoption, because this project has two things those playbooks don't model: formal ADR governance and multi-subsystem boundaries.

**Adopted (mapped above):**

| Pattern | Source | How it shows up here |
|---|---|---|
| Separate coding vs review agents | agentic-cd | OpenCode executes; `@reviewer`/`@safety-reviewer` review; Claude Code signs off. |
| Intent as versioned artifacts | agentic-cd | Story files + `docs/superpowers/specs|plans/` already do this. |
| Small batches / manageable context | agentic-cd | `@explore` keeps research out of the main context; one ticket at a time. |
| Stable **U-IDs** in plans | compound-eng | Already in the execution guide; tracked as [META-004](tickets/META-004-adopt-u-ids-in-plans.md). |
| **Parallel Safety Check** before fan-out | compound-eng | Tracked as [META-005](tickets/META-005-adopt-parallel-safety-check.md); apply it before dispatching multiple edit-capable agents/worktrees. |
| Persona reviewers | compound-eng / agentic-dev-team | `@reviewer`, `@safety-reviewer`. |
| Optional review-before-commit gate | agentic-dev-team | `/review` on demand. See the deliberate choice below. |

**Deliberately *not* adopted (with reasons):**

- **A blocking pre-commit `/code-review` hook.** AGENTS.md cardinal rule #4 is *"never commit unless the user asks."* A hook that auto-runs on every commit fights that and adds ceremony a solo hobbyist project doesn't need. `/review` stays **on-demand**; the human decides when to commit.
- **Mandatory BDD/Gherkin specs + four-persona plan review for every change.** Reserved implicitly for `design`-tier work (which already runs brainstorm → spec → plan in Claude Code). `mechanical`/`small` tickets stay light — that is the whole point of the tier system.
- **Trunk-based / no-feature-branch and release-train mandates** from agentic-cd. Irrelevant to a single-developer repo with no CI yet ([D-06](../CLAUDE.md)); revisit if a second developer or deployment target appears.
- **Coverage mandates.** agentic-cd explicitly warns these make agents optimise for the metric; CLAUDE.md §9.1 already rejects a numeric coverage gate.

The one principle from agentic-cd worth pinning to the wall: **an agent-generated change must meet or exceed the bar of a human-generated one.** That is why the safety predicate work (ADR-007) always escalates to a final Claude Code review with a human in the loop, no matter how clean the Zen execution looked.

---

## 7. Permissions, safety rails, and the "never commit" rule

[`opencode.json`](../opencode.json) sets conservative defaults so an autonomous Zen session can't surprise you:

- `edit: ask`, `bash.*: ask` — you approve edits and arbitrary commands. Set `edit: allow` if you want a faster inner loop once you trust the setup.
- **Allow-listed** without prompting: the routine board scripts (`ticket_new.py`, `ticket_move.py`, `board_check.py`, `ticket_archive.py`), `cmake`, `ctest`, `pytest`, and read-only git (`status`, `diff`, `log`). Other scripts under `tools/board/`, including ADR and solution scaffolding, still prompt by default.
- `git commit`: **ask** (honours the cardinal "never commit unless asked" rule — OpenCode will prompt, you decide).
- `git push`: **deny** outright.
- Read-only subagents (`@explore`, `@researcher`, `@reviewer`, `@safety-reviewer`) have `edit: deny` and `bash: deny` in their own frontmatter, so research and review can never mutate the tree.

If a command you use often keeps prompting, add it to the `bash` allow-list in `opencode.json` rather than loosening the global default.

---

## 8. Files this setup adds

```text
opencode.json                         OpenCode project config (models, instructions bridge, permissions)
.opencode/
  agents/
    explore.md                        Read-only ticket pre-flight research
    researcher.md                     External best-practice / API research
    reviewer.md                       First-pass correctness + hot-path + tests review
    safety-reviewer.md                Adversarial ADR-007 / physical-environment review
  commands/
    board.md                          /board   — lint + summarise the board
    next-ticket.md                    /next-ticket — find the next ready ticket
    start-ticket.md                   /start-ticket ID — tier-gate, pre-flight, in-progress
    finish-ticket.md                  /finish-ticket ID — verify, review, done, lint
    ticket-archive.md                 /ticket-archive — archive old Done items, keep newest 10
    review.md                         /review  — first-pass review of the diff
    agents.md                         /agents — list the OpenCode subagents
    help.md                           /help — list slash commands
docs/opencode-startup.md              This file
docs/opencode-session-continuity.md   Portable scaffold for local `.remember/` notes
```

Nothing here changes the architecture, the ADRs, the board format, or the story-file schema. It is a thin adapter that lets the existing way of working run on OpenCode with Zen models, while keeping the Claude subscription doing what only it can legitimately do, in Claude Code.
