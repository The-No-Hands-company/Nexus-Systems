# Memory and Lifetime Report

This document defines the explicit Month 1 memory and lifetime reporting artifact.

## Purpose

Nexus Modeling already contains resource lifecycle and reset/resize regression tests.
This report makes that coverage visible and reproducible instead of leaving it implicit.

## Reporting Command

From repository root:

1. `ctest --test-dir build --output-on-failure -R "(Lifecycle|Resize|Destroy|ResetScene)"`

## Current Coverage Themes

1. Buffer and descriptor lifecycle behavior.
2. Shadow target clear/rebuild behavior across resize and reset.
3. Uploaded geometry buffer destruction and ownership handoff.
4. Scheduler-owned descriptor retention and release.

## Artifact Source

The CI workflow writes a text artifact for the filtered lifetime suite so
contributors can inspect current status directly.

## Current Recorded Baseline

Recorded locally on 2026-05-06 using:

1. `ctest --test-dir build --output-on-failure -R "(Lifecycle|Resize|Destroy|ResetScene)"`

Observed status:

1. 24 tests passed, 0 failed.
2. Vulkan-specific lifecycle cases were skipped as expected in this environment.

## Interpretation Rules

1. This is a control artifact, not a substitute for full leak tooling.
2. A green lifetime report means the current deterministic lifecycle checks passed.
3. Dedicated leak detectors and richer memory dashboards can be added later without changing this baseline contract.

## Month 1 Acceptance Link

This document satisfies the Month 1 requirement for an explicit memory/lifetime reporting artifact
defined in [vision-and-roadmap.md](vision-and-roadmap.md).