# Month 3 Render Pipeline Checklist

This checklist executes the Month 3 roadmap commitment in
[vision-and-roadmap.md](vision-and-roadmap.md).

Primary slice: Render Pipeline v1.

## Scope Lock

In scope:

1. Finalize deferred baseline: GBuffer, shadow chain, composite contracts, descriptor lifecycle.
2. Render graph validation hooks for pass ordering and resource state transitions.
3. Frame capture metadata export for debugging and CI artifacts.

Out of scope:

1. New geometry kernel work.
2. Parametric/constraint solver.
3. Full render-graph node DAG authoring UI.

## Execution Cadence

Target split for Month 3:

1. 60% implementation
2. 30% tests and regression hardening
3. 10% docs/design notes

## Work Packages

## R3.1 GBuffer / Shadow / Composite Baseline Audit

Status: Complete

The GBuffer allocation, shadow atlas chain, composite pass binding, and
shadow lighting binding descriptor contracts were already implemented and
covered by RendererBehavior tests prior to Month 3.  No new work required;
all related tests pass in the current baseline.

Done when:

1. GBuffer + shadow + composite descriptor tests pass deterministically.

## R3.2 Render Graph Validation Hooks

Status: Complete

1. Define `RenderGraphValidator` contract with pass-ordering and resource-state
   transition validation.
2. Implement pass-ordering rules: shadow passes must precede geometry passes;
   geometry passes must precede composite passes.
3. Implement resource-state transition validation: GBuffer targets must be in
   ColorAttachmentWrite before geometry pass; must transition to ShaderRead
   before composite; shadow atlas must be in DepthWrite during shadow passes
   and DepthRead before lighting sampling.
4. Expose a `RenderGraphValidationReport` with `valid` flag and `issues` vector.
5. Wire validator into `Renderer::render()` as an opt-in diagnostic check.
6. Add deterministic headless tests covering all rule classes.

Done when:

1. Validator API is public and covered by at least one success and one failure
   test per rule class.

Completion notes:

1. Added public `RenderGraphValidator` API and implementation with pass-ordering
   and resource-state validation rules.
2. Wired optional validator execution into `Renderer::render()` with
   `RenderGraphValidationReport` persistence.
3. Added deterministic test coverage in `tests/kernel/test_RenderPipelineV1.cpp`.

## R3.3 Frame Capture Metadata Export

Status: Complete

1. Define `FrameCaptureDesc` carrying per-frame metadata: frame index, pass
   sequence, resource state snapshots, draw call counts, and GPU timestamp
   estimates.
2. Define `IFrameCaptureExporter` interface with `beginCapture()`,
   `recordPass()`, `endCapture()` and `exportSnapshot()`.
3. Implement `NullFrameCaptureExporter` for headless/CI use.
4. Wire `IFrameCaptureExporter*` optional hook into `Renderer`.
5. Add deterministic tests: capture records expected pass entries, frame index
   monotonically increases, snapshot is stable across identical frames.

Done when:

1. Exporter API is public and covered by deterministic headless tests.

Completion notes:

1. Added public `IFrameCaptureExporter` and `NullFrameCaptureExporter` API with
   per-pass/frame metadata snapshots.
2. Wired optional exporter hooks into `Renderer::beginFrame()` and
   `Renderer::endFrame()`.
3. Added deterministic test coverage in `tests/kernel/test_RenderPipelineV1.cpp`.

## Month 3 Exit Gate

Month 3 is complete only when all are true:

1. R3.1 (GBuffer/shadow/composite baseline) is verified green.
2. R3.2 (render graph validation) is implemented with pass-ordering and
   state-transition rules covered by tests.
3. R3.3 (frame capture metadata export) is implemented and covered by
   deterministic headless tests.

Status: COMPLETE

Test baseline: 272 discovered / 272 passed / 0 failed.
