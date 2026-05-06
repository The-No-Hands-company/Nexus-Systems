# Month 2 Geometry Checklist

This checklist executes the Month 2 roadmap commitment in
[vision-and-roadmap.md](vision-and-roadmap.md).

Primary slice: Deterministic Mesh Foundation v0.

## Scope Lock

In scope:

1. Topology validation primitives.
2. Attribute channel framework hardening.
3. Stable mesh element ID strategy draft.
4. Core operations v0: transform, merge, split, weld, normals/tangents rebuild.
5. Script-callable hooks for implemented operations.
6. Golden deterministic topology tests for completed primitives.

Out of scope:

1. New major rendering branch work.
2. Full CAD/constraint solver implementation.
3. High-level UI workflow work.

## Execution Cadence

Target split for Month 2:

1. 65% implementation
2. 25% tests and regression hardening
3. 10% docs/design notes

## Work Packages

## G2.1 Topology Validation Primitives

Status: Complete

1. Add topology validation report contract and issue taxonomy.
2. Validate face arity, index bounds, degenerate edges, and non-manifold edges.
3. Report boundary edge count as deterministic metadata.
4. Add deterministic tests for valid, out-of-range, and non-manifold cases.

Done when:

1. Validation API is public and documented by header comments.
2. Tests cover success path and core failure classes.

## G2.2 Attribute Channels and Consistency

Status: Complete

1. Confirm explicit contracts for positions, normals, UVs, skinning streams.
2. Add tangent-channel placeholder contract and validation hooks.
3. Add tangent rebuild contract and tests for channel consistency across mixed-enabled stream sets.

Done when:

1. Attribute consistency checks are complete for all declared channels.

## G2.3 Stable Mesh Element IDs

Status: Complete

1. Propose vertex/edge/face ID model with deterministic generation.
2. Add minimal storage and API hooks for stable references.
3. Add tests proving IDs are stable under no-op operations and deterministic transforms.

Done when:

1. ID behavior is deterministic and covered by tests.

## G2.4 Core Operations v0

Status: Complete

1. Transform operation API and implementation.
2. Merge operation API and implementation.
	Current note: first explicit append-and-remap mesh combine path is now the active merge baseline.
3. Split operation API and implementation.
	Current note: first deterministic face-range extraction path is now the initial split primitive.
	Current note: destructive split now exists as the first composable in-place split variant.
4. Weld operation API and implementation.
	Current note: first constrained coincident-vertex weld path is in progress as the initial topology-preserving merge/weld slice.
5. Normals/tangents rebuild operation API and implementation.

Done when:

1. Each operation has at least one deterministic success test.
2. Each operation has one high-value failure/edge-path test.

## G2.5 Script-Callable Surface

Status: Complete

1. Define command-facing entry points for completed geometry operations.
2. Add deterministic tests proving operation availability through script-facing contracts.
	Current note: transform, append-merge, split, and weld now have a small command-facing surface intended for future scripting bindings.

Done when:

1. Completed operations are callable via script-facing API.

## G2.6 Golden Topology Regression Set

Status: Complete

1. Add golden fixtures for baseline topology cases.
2. Add deterministic signature checks (indices, edge sets, validation metadata).
3. Integrate into required CI test gates.

Done when:

1. Golden regression suite passes deterministically on required platforms.

## Month 2 Exit Gate

**Status: COMPLETE**

Month 2 is complete only when all are true:

1. Deterministic Mesh Foundation v0 scope items are implemented or explicitly deferred with justification.

All G2.1–G2.6 work packages are implemented and covered by the test suite.
Test baseline: 252 discovered / 252 passed / 0 failed.
2. Implemented geometry operations are script-callable.
3. Golden topology regression checks are stable.
4. No new major rendering branch was started during Month 2.