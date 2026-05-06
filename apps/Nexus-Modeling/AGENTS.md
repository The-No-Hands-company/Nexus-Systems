# AGENTS

Scope: entire Nexus Modeling workspace.

This file defines the default operating contract for human contributors and coding agents.

## Mission

Build a production-grade C++23 Vulkan-first geometry and rendering kernel that can scale to long-term DCC complexity while remaining testable, deterministic, and API-compatible with Nexus-Cloud workflows.

## Core principles

- Keep public interfaces stable and intentional.
- Prefer explicit correctness over implicit convenience.
- Treat synchronization, lifetime, and memory ownership as first-class concerns.
- Add or update tests with every behavior change.
- Keep changes reversible: small commits, clear diffs, no unrelated rewrites.

## Required quality gates

For code changes in this repository, run and pass:

1. Build:
	- cmake --build build -j$(nproc)
2. Tests:
	- ctest --test-dir build --output-on-failure

If a change cannot be validated locally (platform/tooling limitation), document exactly what was not run and why.

## Architecture boundaries

- Public API headers live in src/kernel/include/nexus.
- Internal implementation headers live under src/kernel/src.
- Do not expose internal headers as public API.
- Preserve contract compatibility with Nexus-Cloud facing interfaces.

## Rendering/kernel expectations

- Vulkan is the primary backend target.
- Null backend must remain a first-class CI/headless path.
- Reversed-Z behavior must remain consistent unless intentionally redesigned.
- Synchronization and layout transitions should be explicit where possible.

## Testing expectations

- Prefer behavior tests over implementation-detail tests.
- Extend existing suites before adding brittle one-off tests.
- Add regression coverage for any bug fix.
- Keep tests deterministic and CI-safe (headless where required).

## Documentation expectations

- Update README.md for user-visible workflow or architecture changes.
- Update docs/ for design or operational changes.
- Keep comments concise and focused on non-obvious decisions.

## Safety and hygiene

- Never commit secrets, tokens, or credentials.
- Avoid destructive git operations unless explicitly requested.
- Do not silently alter unrelated files.
