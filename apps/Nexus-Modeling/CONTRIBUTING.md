# Contributing to Nexus Modeling

Thanks for helping build Nexus Modeling. This project is a C++23 Vulkan-first geometry/rendering kernel with strict correctness and determinism requirements.

## Before You Start

1. Read README.md and docs/README.md.
2. Check ROADMAP.md for current milestone priorities.
3. Pick or create a GitHub Issue before opening a pull request.

## Development Rules

- Keep changes focused and reversible.
- Do not mix unrelated refactors into feature or bug-fix PRs.
- Preserve public API stability under src/kernel/include/nexus.
- Keep Null backend parity for interface and behavior changes.
- Treat synchronization, transitions, and lifetimes as first-class concerns.

## Build and Test (Required)

Run from repository root:

1. cmake -S . -B build
2. cmake --build build -j$(nproc)
3. ctest --test-dir build --output-on-failure

If you cannot run a required gate, state exactly what was skipped and why in the PR.

## Definition of Done

A contribution is done when all of the following are true:

1. Behavior is implemented and scoped to a single concern.
2. Tests are added or updated for behavior/regression coverage.
3. Build and test gates pass locally, or skips are documented.
4. Relevant documentation is updated.
5. No secrets/tokens/credentials are introduced.

## Pull Request Checklist

- Describe user-visible behavior changes.
- Call out synchronization/resource-lifetime implications.
- Link the issue and tests covering the change.
- Include residual risks and untested paths.
- Complete the required `Kernel Capability Declaration` section in `.github/PULL_REQUEST_TEMPLATE.md`.

## Review Style

Reviews prioritize findings in this order:

1. Correctness regressions.
2. Synchronization and resource-lifetime risks.
3. API contract breaks.
4. Missing or weak tests.

Use concrete file/line references and keep summaries secondary to findings.
