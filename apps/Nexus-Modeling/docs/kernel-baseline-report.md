# Kernel Baseline Report

Status date: 2026-05-07

This document records the initial Month 1 baseline state for Nexus Modeling.

## Build Status

1. Local build gate passes:
   - cmake --build build -j$(nproc)

## Test Status

1. Full local test gate passes:
   - ctest --test-dir build --output-on-failure
2. Current discovered tests: 290
3. Current result on recorded baseline run: 290 passed, 0 failed

## Vulkan Skip Baseline

The current environment records expected Vulkan-related skips for tests that
depend on unavailable runtime pipeline support or unsupported headless features.

This baseline is acceptable for Month 1 so long as:

1. Skip behavior remains intentional and documented.
2. Null backend remains deterministic and fully runnable in CI.

HD-VK-TEST-001 skip delta (2026-05-07):

1. Prior VulkanRegression skips in this environment: 12.
2. Current VulkanRegression skips in this environment: 5.
3. Net delta: -7 skips (three avoidable lifecycle tests, two descriptor-set lifecycle tests, and two shadow-atlas regressions converted to runnable).

## Perf Smoke Baseline

Current status:

1. Automated Null perf smoke harness exists.
2. Reproducible matrix is defined in [perf-benchmark-matrix.md](perf-benchmark-matrix.md).
3. Initial Null baseline has been captured locally.
4. Vulkan capture remains environment-dependent and is recorded as pending where GPU runners are unavailable.

## Memory and Lifetime Baseline

Current status:

1. Lifecycle and reset/resize behavior is covered by deterministic tests.
2. Explicit reporting artifact is defined in [memory-lifetime-report.md](memory-lifetime-report.md).
3. Current filtered lifecycle report run: 24 tests passed, 0 failed, with expected Vulkan skips in this environment.
4. This is a baseline control artifact, not yet a full leak dashboard.

## Kernel Health Dashboard Status

Current status:

1. Build and test gates exist and are used locally.
2. A GitHub Actions workflow is added to make the health signal visible to contributors.
3. Perf and memory reporting remain partial and should be expanded after initial workflow adoption.

## Ownership Lock Status

Current status:

1. Domain ownership map exists in [kernel-capability-map.md](kernel-capability-map.md).
2. PR template requires capability declaration and owner information.

## Residual Gaps

Month 1 is not fully closed until these are complete:

1. Vulkan perf baseline capture on a suitable runner.
2. Optional future upgrade from lifecycle report to richer memory instrumentation.

## Acceptance Link

This document is the initial baseline report for Month 1 requirements defined in
[vision-and-roadmap.md](vision-and-roadmap.md).