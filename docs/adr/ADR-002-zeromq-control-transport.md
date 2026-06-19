# ADR-002: ZeroMQ Control Transport

## Status
Accepted (2026-05-08, originally documented in v0.2 design; v0.3 adds explicit socket configuration)

## Context
The tracking core must publish state to multiple downstream consumers — Python viewer, MAVLink adapter for the drone flight controller, laser-aim control loop (Phase 4+), and replay logger — without any consumer's behaviour blocking the core. Downstream consumers may be co-located on the Pi 5 or on remote hosts (the Pi 3B network sensor, ground-station laptop, etc.).

Forces:
- The core publishes at up to 90 Hz; consumers may read at lower rates or stall transiently.
- Stale data is worse than missing data when feeding a flight controller (per ADR-007).
- A broker introduces latency and a single point of failure.
- The transport choice should not commit us to a serialisation format.

## Decision
We will use ZeroMQ as the inter-process transport for the tracking-state stream. The C++ core acts as the producer; all consumers are subscribers.

Specific contract:
- **Pattern:** PUB/SUB.
- **Direction:** the core BINDs to a TCP endpoint (default `tcp://*:5555`); consumers CONNECT.
- **Socket options on the publisher:**
  - `ZMQ_SNDHWM = 1` (high-water mark of one message in the outbound queue)
  - `ZMQ_CONFLATE = 1` (only the most recent message is retained when consumers lag)
  - `ZMQ_LINGER = 0` (no blocking on shutdown)
- **Socket options on each consumer:**
  - `ZMQ_RCVHWM = 1`, `ZMQ_CONFLATE = 1`
- **Serialisation:** JSON in v0.3 for human-debuggability. Migration to MessagePack is a Phase 4+ optimisation.
- **Schema:** every message includes `schema_version`, `message_id` (monotonic), `publish_timestamp_ms`, `frame_capture_timestamp_ms`, a `system_health` block, and an `objects` array (which may be empty).

The core emits a heartbeat message at ≥1 Hz independent of frame ingestion, so consumers can distinguish "tracker silent because nothing is detected" from "tracker process dead."

## Consequences

**Positive:**
- Brokerless: one fewer process, one fewer point of failure.
- Non-blocking: a stalled consumer cannot starve the tracker.
- `CONFLATE=1` enforces last-message-wins semantics, which matches the safety contract: consumers always act on the freshest known state.
- Multiple consumers attach without coordination.
- Heartbeat decouples liveness signal from data signal.

**Negative:**
- PUB/SUB has the slow-joiner problem: a consumer connecting after the publisher has started may miss messages until its subscription handshake completes. Consumers must explicitly wait for a `tracker_state == RUNNING` message before acting (see ADR-007).
- No per-message acknowledgement; consumer crashes are silent to the producer.
- Bytewise message loss is undetectable except via `message_id` gaps.

**Risks:**
- Consumers that subscribe with permissive filters and lower their own `RCVHWM` may receive bursts of messages on reconnect, processing stale state. Mitigated by requiring all consumers to set `RCVHWM=1, CONFLATE=1`. Documented as a contract requirement.
- Network partitions on remote-consumer setups produce silent gaps. The MAVLink adapter (separate component, not part of the core) handles this by raising a `vision_unhealthy` flag on its side.

## Alternatives Considered
- **JSON over WebSocket.** Rejected for the control path: parsing overhead, head-of-line blocking on TCP, no native pub/sub. Acceptable for the Python viewer, but using two transports adds complexity. ZMQ JSON gives us debuggability without the WebSocket overhead.
- **MQTT.** Rejected: requires a broker; quality-of-service guarantees do not align with our "drop stale, never block" preference; broker becomes a new failure mode.
- **ROS 2 / DDS.** Rejected for v0.3: heavy runtime, opinionated build system, larger learning curve. Not ruled out for later phases if integration with a ROS 2 fleet becomes a requirement (would be a new ADR superseding this one).
- **In-process API (e.g., function calls or shared memory).** Rejected: violates ADR-001's process-separation goal and prevents headless deployment with multiple consumer hosts.

## Related ADRs
- ADR-001 (Hybrid C++/Python) — establishes the producer/consumer boundary.
- ADR-007 (`safe_for_control` Predicate) — defines the consumer-side semantics.
- ADR-008 (LaserController Adapter) — a future ZeroMQ consumer.
