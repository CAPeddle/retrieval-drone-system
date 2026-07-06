# Retrieval Drone System

An autonomous ball-retrieval drone ecosystem: a fixed ground-based camera observes the room, a steerable laser pointer aims at the target, and a drone retrieves the ball. Each component is its own subsystem with its own lifecycle. The root of this repo is the orchestration layer; subsystems live in sibling folders (plain folders for now, may become separate git repos later).

## Status

**Active subsystem:** `tracking-core/` — single-camera floor-plane tracking on Pi 5. v0.3 vertical slice ("laser on tabletop"). See [CLAUDE.md §1](CLAUDE.md) for success criteria.

**Current work:** see [BOARD.md](BOARD.md).

## Workflow

Work flows through [BOARD.md](BOARD.md) — a flat kanban with one-line tickets. Each ticket has a **story file** at `docs/tickets/<ID>-<slug>.md` that holds context, acceptance criteria, plan, and an append-only log.

**Two-file invariant** (the parallel-safety design):

| File | Holds | Edit rules |
|---|---|---|
| `BOARD.md` | Ticket lines, grouped by column | Single-line move per edit. No inline detail. See [`.claude/rules/tickets.md`](.claude/rules/tickets.md). |
| `docs/tickets/<ID>-<slug>.md` | All ticket state, plan, history | Free to edit. Status front-matter must agree with BOARD column. See [`.claude/rules/tickets.md`](.claude/rules/tickets.md). |

This split means parallel agent sessions working on different tickets never contend on the same file.

**Columns:**

| Column | Meaning | Exit trigger |
|---|---|---|
| Backlog | Identified but not committed to | You decide it's up next |
| Next | Ready to pick up | Work begins (move to In Progress) |
| In Progress | Plan exists, implementation underway | Acceptance criteria met, tests pass, merged |
| Blocked | Waiting on an external dependency | Dependency clears |
| Done (recent) | Last ~10 completed items | Auto-archived to `docs/board-archive.md` when >10 |

**Ticket IDs** use per-subsystem prefixes — `TRK-`, `VIEW-`, `CAM-`, `LASER-`, `MAV-`, `DRONE-` for subsystems, `DOCS-` and `META-` for cross-cutting work. Numbers are sequential per prefix, never reused.

**Three tiers** govern how much process a ticket runs through:

| Tier | When to use | Workflow |
|---|---|---|
| `mechanical` | `git mv`, reference updates, single-commit cleanup | Inline plan in story file. Execute. |
| `small` | 5–10 steps, no architectural decisions | Hand-written plan in story file. No formal spec. |
| `design` | Architectural impact, multiple alternatives, unclear acceptance | Brainstorming step → spec file → plan file → implement. |

**The brainstorming and plan steps produce artifact files.** The artifact format is what the workflow enforces, not the tool that produced it:

- **Spec:** `docs/superpowers/specs/<ID>-<slug>-design.md`. Linked from the story file's `spec:` front-matter.
- **Plan:** `docs/superpowers/plans/<ID>-<slug>-plan.md`. Linked from `plan:` front-matter.

Claude Code's `superpowers:brainstorming` and `superpowers:writing-plans` skills automate these; the artifacts live at the same paths and the story file links to them.

**Tooling.** The [`tools/board/`](tools/board/) directory contains stdlib-only Python helpers — `ticket_new.py`, `ticket_move.py`, `board_check.py`, `ticket_archive.py` — that perform the status + log + BOARD updates above atomically. Prefer scripts over hand-editing; see [`tools/board/README.md`](tools/board/README.md) for usage.

**Risk register (CLAUDE.md §13) vs board.** The R-NN / D-NN items in [CLAUDE.md](CLAUDE.md) are risk-tracking, not active work. They become board tickets only when work is actually scheduled. The risk register is the source of truth for *what could go wrong*; the board is the source of truth for *what is being done*.

## Subsystems

| Dir | Role | Hardware | Status | Notes |
|---|---|---|---|---|
| [`tracking-core/`](tracking-core/) | C++ tracking pipeline, ZMQ publisher, `safe_for_control` predicate | Pi 5 + NoIR CSI camera | **Scaffolded** | — |
| [`viewer/`](viewer/) | Python ZMQ subscriber, floor-plane visualisation | Any host | Stub | Currently nested under `tracking-core/src/viewer/` — to be promoted |
| [`camera-node/`](camera-node/) | Secondary camera streaming source | Pi 3B + NoIR CSI camera | Future | Phase 3+ (multi-camera) |
| [`laser-controller/`](laser-controller/) | Laser MCU firmware + Python adapter (ADR-008) | MCU + Pi 5 | Future | Contract defined; no code |
| [`mavlink-adapter/`](mavlink-adapter/) | ZMQ → MAVLink translator for drone EKF | Pi 5 or drone companion | Future | Phase 3+ |
| [`drone/`](drone/) | Flight controller config, electronics, mechanical | Drone hardware | Future | — |

## Layout

```
Drone/
  CLAUDE.md             Operating contract for AI agents (authoritative)
  README.md             This file — orientation
  BOARD.md              Kanban — what's being worked on (under construction)
  docs/
    adr/                Architecture Decision Records
    design/             Consolidated design snapshots (currently missing)
    research/           Historical brainstorm .md files
  tracking-core/        Pi 5 tracking subsystem
  viewer/               Floor-plane visualisation (currently inside tracking-core/)
  camera-node/          Future: Pi 3B camera streamer
  laser-controller/     Future: laser MCU + adapter
  mavlink-adapter/      Future: drone bridge
  drone/                Future: drone subsystem
  .claude/              Path-scoped rules + skills for Claude Code
```

The "currently …" notes mark cleanup steps not yet executed. The target layout above is the source of truth; the current on-disk state is in transition.

## Pointers

- **Operating contract:** [CLAUDE.md](CLAUDE.md) — read before any non-trivial work.
- **Architecture decisions:** [`docs/adr/`](docs/adr/).
- **Agent rules & skills:** path-scoped [`.claude/rules/`](.claude/rules/) and [`.claude/skills/`](.claude/skills/), dispatched from [CLAUDE.md](CLAUDE.md).
- **Historical design research:** [`docs/research/`](docs/research/).
