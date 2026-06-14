# Plan 001: Z2 OpenCode Web + Project Setup

> Set up the Drone Ball Retrieval System on HP Z2 (Ubuntu 24.04) with
> OpenCode Web for remote development and compound-engineering skills deployed.

---

## Context

The project lives on a Windows workstation. The HP Z2 (192.168.2.48, Ubuntu
24.04.4 LTS) is a dedicated Linux development machine with SSH access.
Goals: clone the repo, install OpenCode Web on port 4096 as a persistent
systemd service, deploy compound-engineering skills, and install build
tooling. The Z2 already has git, curl, wget, apt, and Snap but no Node.js,
no OpenCode, no Docker. It has prior Claude Code / Codex / Copilot configs.

## Acceptance

- [ ] Repo cloned on Z2 at `~/projects/Drone` with `git pull` working via SSH
- [ ] `opencode --version` succeeds on Z2 (binary on PATH)
- [ ] OpenCode Web is reachable at `http://192.168.2.48:4096` with password auth
- [ ] OpenCode Web starts automatically on Z2 boot (systemd user service)
- [ ] `opencode.json` model IDs updated with real Zen catalogue IDs
- [ ] `/board` runs successfully in OpenCode on Z2
- [ ] ce-* skills (`ce-work`, `ce-plan`, `ce-debug`, etc.) are loadable
- [ ] Build toolchain installed: `cmake`, `g++`, `libopencv-dev`, `libzmq3-dev`
- [ ] Python venv with `opencv-python`, `pyzmq`, `numpy` installs cleanly

## Plan

### Sequence map

```
U1 ──→ U3 ──→ U6 ──→ U8 ──→ U9
                  ↗
U2 ──→ U6 ────→
U2 ──→ U7 ────→
U4 ──────────────→
U5 ──────────────→
```

- U1, U2, U4, U5 run in parallel (U4 needs Windows-side access to skills dir)
- U3 after U1 (needs SSH key)
- U6 after U2 + U3 (needs OpenCode binary + repo)
- U7 after U2 (needs binary path)
- U8 after U3 + U6
- U9 last

### U-IDs

#### U1. Generate SSH key + configure Git on Z2

SSH into Z2, generate a new ed25519 key, configure `~/.ssh/config` with the
`github.com-personal` host alias, register the public key on GitHub, verify
connectivity.

Rationale for a new key on Z2 (vs copying the Windows key): clean identity
separation; if the Z2 is compromised the Windows key is not exposed.

Files to create:

- `~/.ssh/id_ed25519_z2` — new key
- `~/.ssh/config` — entry:
  ```
  Host github.com-personal
      HostName github.com
      User git
      IdentityFile ~/.ssh/id_ed25519_z2
  ```
- `~/.gitconfig` — `user.name` + `user.email` set per repo or global

Verify: `ssh -T git@github.com-personal` returns the "successfully
authenticated" message.

#### U2. Install OpenCode on Z2 via curl script

Run the install script: `curl -fsSL https://opencode.ai/install | bash`.

Add to PATH (script reports its install path; typically `~/.opencode/bin` or
`/usr/local/bin`). Accept the EULA.

Verify: `opencode --version`.

#### U3. Clone repo to Z2

Clone to `~/projects/Drone`:

```bash
mkdir -p ~/projects
git clone git@github.com-personal:CAPeddle/retrieval-drone-system.git ~/projects/Drone
```

Verify directory structure: `opencode.json`, `AGENTS.md`, `CLAUDE.md`,
`BOARD.md`, `tracking-core/`, `docs/`, `.opencode/` all present.

#### U4. Deploy compound-engineering skills to Z2

Skills live at `~/.config/opencode/skills/` on the Z2 (same path as Windows
relative to home). Copy the `ce-*` directories from Windows:

```bash
# On Windows:
rsync -avz --include='ce-*/' --include='*/' --exclude='*' \
  "$env:USERPROFILE\.config\opencode\skills\ce-"*/ \
  "cpeddle@192.168.2.48:~/.config/opencode/skills/"
```

Or SCP each skill directory individually. The full list of ce-* skills to
deploy: ce-brainstorm, ce-code-review, ce-commit, ce-commit-push-pr,
ce-compound, ce-compound-refresh, ce-debug, ce-demo-reel, ce-doc-review,
ce-frontend-design, ce-ideate, ce-optimize, ce-plan, ce-polish-beta,
ce-product-pulse, ce-proof, ce-release-notes, ce-report-bug,
ce-resolve-pr-feedback, ce-sessions, ce-setup, ce-simplify-code,
ce-slack-research, ce-strategy, ce-test-browser, ce-test-xcode, ce-work,
ce-work-beta, ce-worktree, lfg, ce-agent-native-architecture,
ce-agent-native-audit, ce-clean-gone-branches, ce-dhh-rails-style,
ce-gemini-imagegen, ce-riffrec-feedback-analysis

Also check if any non-ce-* skills (like Azure skills) that are used by
ce-* dependants should come along. Azure skills are under `~/.agents/skills/`
— likely out of scope for this setup unless the plan references Azure infra.

#### U5. Install system build dependencies

```bash
sudo apt update && sudo apt install -y \
    build-essential cmake git \
    libopencv-dev \
    libzmq3-dev
```

Python venv:

```bash
sudo apt install -y python3 python3-venv python3-pip
cd ~/projects/Drone/tracking-core
python3 -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt
```

Verify: `cmake --version` (≥3.16); `g++ --version` (C++17); `python3 --version` (≥3.11).

#### U6. Authenticate OpenCode Zen + update model IDs

1. `opencode auth login` → OpenCode Zen, paste API key
2. List available models from Zen catalogue
3. Edit `~/projects/Drone/opencode.json`:
   - Replace `model` placeholder ID
   - Replace `small_model` placeholder ID
   - Replace `agent.build.model` and `agent.plan.model` IDs
   - Keep `instructions` field as-is (already wired correctly)
4. Verify that both AGENTS.md and CLAUDE.md content are loaded (check model
   behaviour for CLAUDE.md-specific terms like "safe_for_control", "ADR-007")

Reference: `docs/opencode-startup.md §3` for model tiering guidance.

#### U7. OpenCode Web systemd user service

Create `~/.config/systemd/user/opencode-web.service`:

```ini
[Unit]
Description=OpenCode Web Server
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=%h/.opencode/bin/opencode web --hostname 0.0.0.0 --port 4096
Restart=on-failure
RestartSec=5
Environment=OPENCODE_SERVER_PASSWORD=<set during install>

[Install]
WantedBy=default.target
```

Then:

```bash
systemctl --user daemon-reload
systemctl --user enable opencode-web.service
systemctl --user start opencode-web.service
systemctl --user status opencode-web.service
# Enable lingering so service survives logout:
loginctl enable-linger cpeddle
```

Verify: `curl -s http://localhost:4096` returns a non-empty response;
`http://192.168.2.48:4096` accessible from another machine, password prompt.

Record the chosen password in the operator's password manager (not in the
repo). The password is set per the user's preference.

#### U8. Replicate project config + verify board tools

1. Create `.remember/` from template in `docs/opencode-session-continuity.md`
2. Verify Python board scripts run: `python tools/board/board_check.py`
3. Verify model IDs are set (repeat from U6 if behaviour is off)
4. Run `/board` within OpenCode to confirm commands work

#### U9. End-to-end smoke test

- SSH into Z2, `systemctl --user status opencode-web.service` shows active
- Browse to `http://192.168.2.48:4096`, password prompt appears, login works
- OpenCode Web shows the Drone project as workspace
- `/board` command works inside OpenCode Web
- `/next-ticket` shows available tickets (if any)

## Key decisions

| Decision | Chosen approach | Rationale |
|---|---|---|
| SSH key | New ed25519 key on Z2 | Clean separation; Windows key not exposed if Z2 compromised |
| OpenCode install | curl install script | Simplest path; self-contained binary; no Node deps |
| Persistence | systemd user service | Auto-start on boot, survives logout via `loginctl enable-linger` |
| Web port | 4096 | Matches OpenCode docs convention |
| CE skills deploy | rsync from Windows | Preserves directory structure; non-destructive |
| Build toolchain | apt packages | Standard Ubuntu packages; CMake FetchContent covers the rest |

## Common pitfalls to avoid

- **SSH host alias mismatch.** The remote URL is
  `git@github.com-personal:CAPeddle/retrieval-drone-system.git`. The
  `~/.ssh/config` Host directive must be exactly `github.com-personal`.
  Misspelling or casing differences cause `Host key verification failed`.
- **`.opencode/` spelling.** The directory is `.opencode/` (plural), not
  `.opencode/` (singular). OpenCode silently ignores the singular form.
- **Model placeholder IDs.** `opencode.json` ships with placeholder model
  IDs. Attempting real work without updating them produces runtime failures.
  Replace after `opencode auth login`.
- **instructions bridge.** CLAUDE.md is NOT auto-loaded when AGENTS.md
  exists. The `instructions` field in `opencode.json` is the fix. Verify
  on first run that CLAUDE.md content is present in model behaviour.
- **systemd user service prerequisites.** `loginctl enable-linger` is
  required for user services to survive logout. Without it, the service
  dies when the SSH session ends.
- **Permission model mismatch.** The project's opencode.json uses
  `edit: ask` and `bash.*: ask` with an allow-list for board scripts.
  If OpenCode Web defaults differ, verify they aren't silently overridden
  by any system-level or user-level config.

## References

- `docs/opencode-startup.md` — bootstrap checklist, model tiering,
  two-tool division of labour
- `docs/opencode-session-continuity.md` — `.remember/` template
- `CLAUDE.md §4` — hardware platform constraints (Z2 is dev, Pi 5 is target)
- `docs/solutions/external-reviews/2026-05-31-compound-engineering-plugin-review.md`
  — review of compound-engineering patterns
- `docs/solutions/workflow/2026-06-04-tracking-core-rename-and-build-overhaul-handoff.md`
  — handoff pattern for cross-machine setup validation
