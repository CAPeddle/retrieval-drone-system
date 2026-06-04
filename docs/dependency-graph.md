# v0.3+ Dependency Graph — Critical Path

This diagram shows the dependency relationships between tickets. The critical path for v0.3 ship runs through infrastructure → detection → tracking → coordinate mapping → safety predicate → export → validation.

```mermaid
graph TD
    %% === Infrastructure Layer ===
    TRK002[TRK-002<br/>Build System] --> TRK003[TRK-003<br/>Configuration]
    TRK002 --> TRK004[TRK-004<br/>Async Logger]
    TRK002 --> TRK005[TRK-005<br/>Frame Ring Buffer]
    TRK003 --> TRK006[TRK-006<br/>Camera Capture]
    TRK005 --> TRK006
    TRK006 --> TRK007[TRK-007<br/>Frame Timestamping]
    TRK007 --> TRK008[TRK-008<br/>Frame Quality]

    %% === Detection Layer ===
    TRK008 --> TRK009a[TRK-009a<br/>PSD Benchmark]
    TRK009a --> TRK009b[TRK-009b<br/>PSD Correlation]
    TRK009b --> TRK009c[TRK-009c<br/>Clustering]
    TRK009c --> TRK009d[TRK-009d<br/>Grace Period]
    TRK008 --> TRK010[TRK-010<br/>Ball Detector]
    TRK008 --> TRK011[TRK-011<br/>Marker Detector]

    %% === Calibration Tooling (parallel, Python) ===
    TRK002 --> TRK012[TRK-012<br/>Intrinsic Cal Tool]
    TRK012 --> TRK013[TRK-013<br/>Extrinsic Cal Tool]

    %% === Tracking Layer ===
    TRK009d --> TRK014[TRK-014<br/>Track Lifecycle]
    TRK010 --> TRK014
    TRK014 --> TRK015[TRK-015<br/>Obs Gating]
    TRK015 --> TRK016[TRK-016<br/>Track Prediction]

    %% === Coordinate Mapping Layer ===
    TRK013 --> TRK017[TRK-017<br/>Homography Loader]
    TRK017 --> TRK018[TRK-018<br/>Z Compensation]
    TRK018 --> TRK019[TRK-019<br/>Uncertainty Prop]

    %% === Safety Predicate ===
    TRK016 --> TRK020a[TRK-020a<br/>System Clauses]
    TRK019 --> TRK020b[TRK-020b<br/>Geometric Clauses]
    TRK020a --> TRK020c[TRK-020c<br/>Hysteresis]
    TRK020b --> TRK020c

    %% === Export Layer ===
    TRK020c --> TRK021[TRK-021<br/>ZMQ Publisher]
    TRK021 --> TRK022[TRK-022<br/>Heartbeat Thread]
    TRK003 --> TRK023[TRK-023<br/>System Health]
    TRK021 --> TRK023

    %% === Calibration Health ===
    TRK011 --> TRK024[TRK-024<br/>Cal Health Monitor]
    TRK017 --> TRK024
    TRK023 --> TRK024

    %% === Validation ===
    TRK021 --> TRK025[TRK-025<br/>Replay Test Harness]
    TRK021 --> TRK026[TRK-026<br/>Perf Benchmarks]

    %% === Viewer ===
    TRK021 --> VIEW002[VIEW-002<br/>Python Viewer]
    VIEW002 --> VIEW003[VIEW-003<br/>SFC Visualisation]

    %% === Phase 3 ===
    TRK021 --> CAM001[CAM-001<br/>Pi 3B Streaming]
    CAM001 --> TRK027[TRK-027<br/>Multi-Camera Fusion]
    TRK027 --> TRK028[TRK-028<br/>Drone Detector]
    TRK028 --> MAV001[MAV-001<br/>MAVLink Adapter]

    %% === Phase 4 ===
    TRK021 --> LASER001[LASER-001<br/>LaserController]
    LASER001 --> LASER002[LASER-002<br/>MCU Firmware]
    LASER002 --> LASER004[LASER-004<br/>Servo Characterisation]
    LASER004 --> LASER003[LASER-003<br/>Closed-Loop Aiming]
    LASER001 --> LASER003

    %% === Phase 5 ===
    LASER004 --> TRK029[TRK-029<br/>Observability Validation]
    TRK029 --> TRK030[TRK-030<br/>Active Cal Refinement]

    %% === Styling ===
    classDef infra fill:#e8f4f8,stroke:#2980b9
    classDef detect fill:#fef9e7,stroke:#f39c12
    classDef track fill:#eafaf1,stroke:#27ae60
    classDef coord fill:#f9ebea,stroke:#e74c3c
    classDef safety fill:#fdedec,stroke:#c0392b
    classDef export fill:#ebf5fb,stroke:#3498db
    classDef valid fill:#f4ecf7,stroke:#8e44ad
    classDef phase3 fill:#fbeee6,stroke:#e67e22
    classDef phase4 fill:#eaecee,stroke:#7f8c8d
    classDef phase5 fill:#d5f5e3,stroke:#1abc9c

    class TRK002,TRK003,TRK004,TRK005,TRK006,TRK007,TRK008 infra
    class TRK009a,TRK009b,TRK009c,TRK009d,TRK010,TRK011 detect
    class TRK012,TRK013 detect
    class TRK014,TRK015,TRK016 track
    class TRK017,TRK018,TRK019 coord
    class TRK020a,TRK020b,TRK020c safety
    class TRK021,TRK022,TRK023,TRK024 export
    class TRK025,TRK026,VIEW002,VIEW003 valid
    class CAM001,TRK027,TRK028,MAV001 phase3
    class LASER001,LASER002,LASER003,LASER004 phase4
    class TRK029,TRK030 phase5
```

## Critical Path (v0.3 ship)

The longest dependency chain determines minimum elapsed time:

```
TRK-002 → TRK-003 → TRK-006 → TRK-007 → TRK-008 → TRK-009a → 009b → 009c → 009d
  → TRK-014 → TRK-015 → TRK-016 → TRK-020a → TRK-020c → TRK-021 → TRK-025
```

**16 serial steps** on the critical path. Parallel lanes:
- Ball detector (TRK-010) and marker detector (TRK-011) can proceed alongside laser detector.
- Calibration tools (TRK-012, TRK-013) run in parallel with detection work.
- Coordinate mapping (TRK-017→018→019) can proceed once calibration tools are done.
- Viewer (VIEW-002, VIEW-003) runs in parallel once ZMQ publisher exists.

## Execution Order (recommended serial for single developer)

```
TRK-002 → 003 → 004 → 005 → 006 → 007 → 008
  → 009a → 009b → 009c → 009d → 010 → 011
  → 012 → 013 → 014 → 015 → 016
  → 017 → 018 → 019
  → 020a + 020b (parallel) → 020c
  → 021 → 022 → 023 → 024
  → 025 → 026 → VIEW-002 → VIEW-003
```
