---
name: Retrieval Drone System
last_updated: 2026-06-14
---

# Retrieval Drone System Strategy

## Target problem

Off-the-shelf drone builds are optimised for getting airborne fast, but they lock out the engineering trade-offs that make this project interesting. The crux is domain coupling: frame, materials, sensing, computer vision, control — each small decision propagates, so you cannot simply swap in a better part and call it done.

## Our approach

Treat the drone as a physical robotics research platform where every build step reduces uncertainty or creates reusable knowledge. Coupled domains stay separated in design (clear seams), work proceeds in small feedback loops before full-system integration, and experiments compound rather than reset.

## Who it's for

**Primary:** Hobbyist or software-oriented engineer who has outgrown off-the-shelf FPV builds and wants to own the full stack: frame, materials, electronics, sensing, calibration, control loop, software architecture, and experiment workflow. They are hiring this project to turn an open-ended physical robotics ambition into a disciplined, compounding engineering loop.

**Secondary:** Collaborators and AI planning/coding agents. Collaborators need shared direction and a clear record of decisions. Agents need structured context, stable boundaries, and explicit intent so they can help with planning, implementation, review, documentation, and experiment design without drifting from the strategy.

## Key metrics

- **Uncertainty-to-decision cycle time** — How long it takes to turn an open engineering question into a recorded decision (leading).
- **Experiment quality rate** — Percentage of experiments that start with a clear question, hypothesis, measurement method, expected outcome, and decision rule.
- **Knowledge reuse rate** — How often previous decisions, calibration procedures, test results, or design notes are reused instead of rediscovered.
- **Baseline capability regression** — Whether previously proven capabilities (calibration accuracy, tracking tests, control-loop behaviour, material reproducibility) still work after changes.
- **Physical-system trust level** — Measured gap between what the system believes and what the physical world does: tracking error, calibration reprojection error, servo repeatability, command-to-observation latency.

## Tracks

### Airframe, materials, and physical build

Make the drone physically real, durable, modifiable, and experimentally useful.

_Why it serves the approach:_ Physical robustness is a prerequisite for every other track. Without a reliable test platform, sensing and control measurements are confounded by structural variance.

### Sensing, calibration, and world modelling

Make the system able to measure the world accurately enough to trust.

_Why it serves the approach:_ Calibration-first means world-coordinate accuracy is the foundation for every control and behaviour decision downstream.

### Control loops, actuation, and behaviour

Turn measurement into reliable physical action.

_Why it serves the approach:_ Small feedback loops before full-system integration — validate control concepts on cheap rigs before committing to the final airframe.

### Experiment workflow and compound engineering system

Make every iteration produce reusable knowledge, tests, decisions, and context.

_Why it serves the approach:_ This is the compounding engine. Without it, each experiment starts from zero and learning does not accumulate.

## Milestones

- **Calibrated camera world-coordinate system** — Establish a trusted world-coordinate frame for the physical test area, with recorded error.
- **Thrown-ball identification and tracking** — Detect and track a thrown ball in the world-coordinate frame, proving dynamic physical experimentation.
- **Repeatable tracking baseline** — Regression-protected baseline for detection accuracy, tracking continuity, latency, occlusion handling, and world-coordinate error.
- **Closed-loop intercept or approach demo** — Use tracked world coordinates to command a physical system toward the moving target, proving the transition from observation to controlled action.
