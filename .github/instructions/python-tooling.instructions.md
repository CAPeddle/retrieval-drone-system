---
applyTo: "tracking-core/src/viewer/**,tracking-core/tools/**,tracking-core/tests/python_integration/**,viewer/**,laser-controller/**,mavlink-adapter/**"
description: "Python tooling conventions for viewers, tools, and test harnesses in the tracking system."
---
# Python Tooling Conventions

- **Python 3.11 minimum.** Use type hints throughout.
- Format with `black`, lint with `ruff`.
- Use `pydantic` for ZMQ message schemas on the consumer side — prevents silent schema drift.
- Use `pyzmq` for ZeroMQ bindings.
- Avoid frameworks for small tools. A 200-line viewer does not need FastAPI/Flask.
- Replay tools and test harnesses live under `tools/`.
- Production tooling (LaserController adapter, MAVLink adapter, viewer) lives in its owning top-level subsystem directory (`viewer/`, `laser-controller/`, `mavlink-adapter/`) when promoted.
- Unit tests use `pytest`.
- `mypy --strict` will be enforced in CI when CI exists — write code that passes it now.
