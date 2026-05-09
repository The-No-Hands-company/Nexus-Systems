# API Freeze Alpha (Month 12)

This document defines the Month 12 API freeze audit for Nexus Modeling as it approaches 1.0-alpha.

## Purpose

Month 1 established the v0 freeze policy. Month 12 adds a machine-checked audit to prevent accidental public API drift while preserving additive, intentional changes.

## Public API Audit Contract

The audited surface is all public headers under:

1. src/kernel/include/nexus

Authoritative manifest:

1. tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt

Automated audit test:

1. tests/kernel/test_ApiFreezeAudit.cpp

## Audit Rules

1. Every public header path in the manifest must exist.
2. Every public header in src/kernel/include/nexus must be listed in the manifest.
3. Header count drift fails the API audit test unless the manifest is intentionally updated in the same change.

## Update Procedure

When intentionally adding public API headers:

1. Add/update headers under src/kernel/include/nexus.
2. Update tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt.
3. Add behavior-level tests for new contracts.
4. Update docs/month-12-alpha-compatibility-report.md.

When removing/renaming public API headers:

1. Include explicit compatibility rationale.
2. Update migration notes and docs.
3. Update manifest and tests in the same PR.

## Policy Outcome

This freeze-audit mechanism upgrades API stability from policy-only to policy plus deterministic CI enforcement.
