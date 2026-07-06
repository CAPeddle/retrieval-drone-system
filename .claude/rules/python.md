---
description: Python tooling conventions for tracking-core viewer, tools, and integration tests. No determinism contract — maintainability over performance.
paths:
  - "tracking-core/src/viewer/**/*.py"
  - "tracking-core/tools/**/*.py"
  - "tracking-core/tests/python_integration/**/*.py"
---

# Python Tooling Rules (§7.3)

Python tooling has no determinism contract. Rules are light and focused on
maintainability.

- Python 3.11 minimum. Use type hints. `mypy --strict` in CI once CI exists.
- Format with `black`, lint with `ruff`. No exceptions.
- Use `pydantic` (or equivalent) for ZMQ message schemas on the consumer side.
  Schema drift between the C++ producer and Python consumer is a recurring
  failure mode; the `schema_version` field is the first defence and runtime
  validation is the second.
- Use `pyzmq` (or equivalent) for ZMQ bindings.
- Avoid frameworks for small tools. A 200-line viewer does not need FastAPI,
  Django, or Flask — use the standard library where possible.
- Replay tools and test harnesses for the tracking subsystem live under
  `tracking-core/tools/`. Production tooling lives in its owning top-level
  subsystem directory (`viewer/`, `laser-controller/`, `mavlink-adapter/`, etc.)
  once promoted out of `tracking-core/`.
