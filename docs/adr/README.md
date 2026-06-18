# Architecture Decision Records — Drone Ball Retrieval System

This directory contains the Architecture Decision Records (ADRs) for the Tracking & Visualisation Core of the Drone Ball Retrieval System.

ADRs are the source of truth for **why** major architectural choices were made. The consolidated design document at `docs/design/v0.3-tracking-visualisation-system-design.md` is the readable architectural snapshot; it references these ADRs rather than duplicating their rationale.

## Index

| ADR | Title | Status | Date |
|-----|-------|--------|------|
| [ADR-001](ADR-001-hybrid-cpp-python.md) | Hybrid C++ Core with Python Tooling | Accepted | 2026-05-08 |
| [ADR-002](ADR-002-zeromq-control-transport.md) | ZeroMQ Control Transport | Accepted | 2026-05-08 |
| [ADR-003](ADR-003-graceful-coordinate-degradation.md) | Graceful Coordinate Degradation | Accepted | 2026-05-08 |
| [ADR-004](ADR-004-hybrid-calibration-lifecycle.md) | Hybrid Calibration Lifecycle | Accepted | 2026-05-08 |
| [ADR-005](ADR-005-active-laser-modulation.md) | Active Laser Modulation | Accepted | 2026-05-08 |
| [ADR-006](ADR-006-floor-plane-2d-world-frame.md) | FloorPlane2D World Frame | Accepted | 2026-05-08 |
| [ADR-007](ADR-007-geometric-safe-for-control-predicate.md) | Geometric `safe_for_control` Predicate | Accepted | 2026-05-08 |
| [ADR-008](ADR-008-laser-controller-adapter.md) | LaserController Adapter | Accepted | 2026-05-08 |
| [ADR-009](ADR-009-active-calibration-refinement-proposed.md) | Active Calibration Refinement Using the Laser as a Probe | **Proposed** | 2026-05-08 |
| [ADR-010](ADR-010-object-class-z-compensation.md) | Object-Class Z Compensation in Coordinate Mapping | Accepted | 2026-05-08 |

## Conventions

- ADRs are numbered sequentially. Numbers are never reused, even for superseded decisions.
- An ADR is never deleted. Decisions that are no longer in effect are marked `Superseded by ADR-NNN`.
- Each ADR contains: Status, Context, Decision, Consequences, Alternatives Considered, and Related ADRs.
- File naming: `ADR-NNN-short-title-with-hyphens.md`. No apostrophes, no spaces.

## Lifecycle

```
Proposed → Accepted → [Active Use]
                ↓
           Superseded by ADR-XXX
```

Most ADRs are added with status `Accepted` because by the time an ADR is written down, the decision has been deliberated and committed. ADRs added in `Proposed` status (e.g., ADR-009) carry explicit acceptance gates inside the document.

## Creating new ADRs

1. Pick the next free number.
2. Create `ADR-NNN-short-title.md` using the section structure of the existing files.
3. Add a row to the index above.
4. Reference related ADRs at the bottom of your file.
5. If the new ADR replaces an old one, update the old ADR's status to `Superseded by ADR-NNN`.

## See also

- Consolidated v0.3 design document: `../design/v0.3-tracking-visualisation-system-design.md`
- Original v0.2 baseline (as historical reference): the prose-form `Gemini Drone Ball Retrieval System Design Review.md` in the project knowledge.
