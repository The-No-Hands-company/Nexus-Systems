# API Freeze v0

This document records the Month 1 v0 public API boundary freeze for Nexus Modeling.

## Purpose

The v0 freeze is a control mechanism, not a promise of long-term immutability.
It exists to stop accidental public-surface drift while the kernel scales through
Months 1 and 2 of the roadmap.

## Frozen Public API Roots

The following public header roots are the v0 contract surface:

1. src/kernel/include/nexus/Kernel.h
2. src/kernel/include/nexus/gfx
3. src/kernel/include/nexus/render
4. src/kernel/include/nexus/geometry
5. src/kernel/include/nexus/animation
6. src/kernel/include/nexus/neural

## v0 Change Policy

Allowed during v0:

1. Additive API extensions that do not break existing call sites.
2. New public types and functions when they fit an existing domain boundary.
3. Internal implementation refactors that do not alter public behavior.

Not allowed during v0 without explicit review and docs update:

1. Silent renames or removals of public symbols.
2. Semantic changes to public contracts without behavior notes and tests.
3. Cross-domain shortcuts that bypass declared ownership boundaries.

## Review Rules

Any PR touching src/kernel/include/nexus must declare:

1. Capability domain owner.
2. Whether the change is additive or breaking.
3. Required behavior/regression tests.
4. Required docs updates.

## Versioning Intent

v0 means:

1. The surface is intentionally stabilizing.
2. Contributor discipline is required.
3. The project may still evolve before a formal 1.0-alpha API guarantee.

## Month 1 Acceptance Link

This document satisfies the Month 1 requirement for a public API freeze record
defined in [vision-and-roadmap.md](vision-and-roadmap.md).