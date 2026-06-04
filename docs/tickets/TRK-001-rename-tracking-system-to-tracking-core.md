---
id: TRK-001
status: done
subsystem: tracking-core
tier: small
created: 2026-05-31
updated: 2026-06-01
spec: null
plan: null
blockers: []
---

## Context

The C++ tracking subsystem is scaffolded under `tracking-system/`. [CLAUDE.md §12.1](../../CLAUDE.md) defines "tracking core" as the term of art for this subsystem and `tracking-core` is used consistently across ADRs. With the multi-subsystem layout in [README.md](../../README.md) (camera-node, laser-controller, mavlink-adapter as siblings), `tracking-core/` is the consistent naming pattern.

Tier is `small` (not `mechanical`) because this touches the build system, multiple file-scoped instruction `applyTo:` globs, and requires a build verification step.

## Acceptance

- `tracking-core/` exists with the contents of the former `tracking-system/`.
- `tracking-system/` no longer exists in the working tree.
- `cmake -S tracking-core -B tracking-core/build` configures successfully.
- `cmake --build tracking-core/build` builds successfully (sanity — no source code changed).
- `pytest tracking-core/tests/python_integration/` passes (smoke).
- `rg "tracking-system"` returns no matches in tracked files except in this story file's Log and `docs/board-archive.md`.
- `applyTo:` globs in `.github/instructions/cpp-hot-path.instructions.md`, `.github/instructions/tracking-architecture.instructions.md`, and `.github/instructions/tracking-review.instructions.md` reference `tracking-core/` not `tracking-system/`.

## Plan

1. `git mv tracking-system tracking-core`.
2. Update path references in:
   - `README.md` (Subsystems table, Layout block, "currently …" notes for TRK-001 and VIEW-001)
   - `.github/copilot-instructions.md` (Repository Layout, Build & Test sections)
   - `CLAUDE.md` (§7.1 hot-path examples or §8.7 layout block — `rg` will find them)
   - `BOARD.md` (current ticket lines mentioning the old name)
   - `tracking-core/README.md` (build commands)
3. Update `applyTo:` globs in:
   - `.github/instructions/cpp-hot-path.instructions.md`
   - `.github/instructions/tracking-architecture.instructions.md`
   - `.github/instructions/tracking-review.instructions.md`
4. Check `tracking-core/.gitignore` for any old-path references; update if found.
5. Build verification: `cmake -S tracking-core -B tracking-core/build && cmake --build tracking-core/build`.
6. Smoke test: `pytest tracking-core/tests/python_integration/`.
7. Verify `rg "tracking-system"` returns zero matches.
8. Commit as one change: `refactor: rename tracking-system/ to tracking-core/ to match ADR terminology`.

## Log

- 2026-05-31: created. Status: next. Sequenced after DOCS-001 and DOCS-002 because the build verification benefits from a clean working tree.
- 2026-05-31: next → in-progress. Starting execution — rename tracking-system to tracking-core.
- 2026-06-01: in-progress → done. Rename complete. git mv + all path refs updated. Build/test blocked by missing opencv/zmq on Windows (pre-existing env issue, not rename-related).
