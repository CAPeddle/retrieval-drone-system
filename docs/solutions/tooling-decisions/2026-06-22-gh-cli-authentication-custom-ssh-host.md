---
title: gh CLI Authentication with Custom SSH Host Alias
date: 2026-06-22
category: tooling-decisions/
module: gh-cli-authentication
problem_type: tooling_decision
component: tooling
severity: medium
applies_when:
  - "A project uses a custom SSH host alias (e.g., github.com-personal) in ~/.ssh/config"
  - gh CLI API operations (pr create, pr list, run watch) fail despite git push working"
tags: [gh-cli, github, authentication, ssh, git-remote]
---

# gh CLI Authentication with Custom SSH Host Alias

## Context

The project's git remote uses a custom SSH host alias (`git@github.com-personal:CAPeddle/retrieval-drone-system.git`) mapped via `~/.ssh/config` to route to `github.com` with a specific identity key. This works fine for `git push`/`pull` — SSH authentication goes through the host alias.

However, `gh` (GitHub CLI) uses its own authentication mechanism: a personal access token (PAT) or OAuth token stored in `~/.config/gh/hosts.yml`, separate from git's SSH configuration. `gh` API operations — `pr create`, `pr list`, `run watch` — talk to `api.github.com` over HTTPS, not SSH, so they never touch the SSH host alias. Without a token, `gh` commands fail with:

```
Push succeeded, but gh isn't authenticated in this environment. The branch is on origin — you can create the PR from the link.
```

## Guidance

Authenticate `gh` with a PAT that has at minimum the `repo` scope. Two approaches:

**Token-based (preferred for agent/automation environments):**
1. Generate a classic PAT at https://github.com/settings/tokens with `repo` scope (or a fine-grained PAT scoped to the repository)
2. Set via environment variable or login:

   ```bash
   export GH_TOKEN="github_pat_..."
   # or
   echo "github_pat_..." | gh auth login --with-token
   ```

**Interactive login (one-time setup):**
```bash
gh auth login --git-protocol ssh
```

If the token lacks the `admin:public_key` scope, the `gh config set git_protocol ssh` step will emit a 403 on `user/keys` — this is non-critical and can be ignored. Git operations already work via the SSH host alias; `gh` only needs the token for API access.

## Why This Matters

Without understanding the separation between git's SSH auth and `gh`'s token-based auth, you get the misleading "push succeeded but gh isn't authenticated" error. git push works (SSH host alias has the right key), but `gh` API calls fail (no token). The fix is trivial once you know where to look — a PAT with `repo` scope resolves it.

## When to Apply

- When setting up `gh` in a new environment on this project
- When `gh` commands fail while `git push`/`pull` succeeds
- When a git remote uses a custom SSH host alias (which is invisible to `gh`)

## Examples

**Checking auth status:**
```bash
gh auth status    # "You are not logged into any GitHub hosts" — needs fixing
```

**Verifying API access after auth:**
```bash
gh pr list --repo CAPeddle/retrieval-drone-system   # no output = success
gh auth status    # "✓ Logged in to github.com account CAPeddle"
```

## Related

- [GitHub CLI auth docs](https://cli.github.com/manual/gh_auth_login)
- [`~/.ssh/config` host alias pattern](https://docs.github.com/en/authentication/connecting-to-github-with-ssh/about-ssh)
