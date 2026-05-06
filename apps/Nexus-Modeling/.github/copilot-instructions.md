# Copilot Instructions for Nexus Modeling

Nexus Modeling is a C++23 Vulkan-first geometry/rendering kernel. Prioritize correctness, explicit synchronization, and API stability over quick hacks.

## Repository intent

- Build a production-grade 3D modeling/rendering kernel that can scale to a very large codebase.
- Maintain compatibility with Nexus-Cloud integration expectations.
- Preserve deterministic, CI-safe behavior on the Null backend.

## Architecture boundaries

- Public API headers: src/kernel/include/nexus
- Internal implementation: src/kernel/src
- Tests: tests/kernel
- Do not expose internal headers as public API.
- Do not introduce cross-layer shortcuts that bypass IDevice/ICommandBuffer contracts.

## Required quality gates

When changing behavior or interfaces, run:

1. cmake --build build -j$(nproc)
2. ctest --test-dir build --output-on-failure

If something cannot be run locally, state what was skipped and why.

## Coding expectations

- Keep changes focused and reversible.
- Avoid unrelated refactors in the same change.
- Prefer explicit lifetime ownership and cleanup.
- Treat synchronization and resource transitions as first-class design constraints.
- Keep Null backend behavior in parity with interface changes.

## Renderer/kernel expectations

- Keep frame-scheduler path authoritative for production rendering flow.
- For render-pass changes, ensure transitions/layouts and pass ordering stay explicit.
- Do not regress GBuffer + composite sequencing guarantees.
- Keep reversed-Z behavior consistent unless intentionally redesigned.

## Testing expectations

- Add or update tests for each behavior change.
- Prefer behavior-level assertions over implementation-detail assertions.
- Add regression tests for fixed bugs.
- Keep tests deterministic and headless-safe where possible.

## Documentation expectations

- Update README.md for workflow or architecture changes.
- Update docs/ when adding or changing subsystem behavior.
- Keep comments concise and useful for non-obvious logic.

## Security and safety

- Never commit secrets, credentials, tokens, or private keys.
- Avoid destructive git commands unless explicitly requested.

## Preferred review style

- Prioritize findings in this order: correctness regressions, synchronization/resource-lifetime risks, API contract breaks, then missing or weak tests.
- Report concrete, actionable findings first with file/line references; keep high-level summaries brief and secondary.
- Explicitly call out residual risk and untested paths before approving.
