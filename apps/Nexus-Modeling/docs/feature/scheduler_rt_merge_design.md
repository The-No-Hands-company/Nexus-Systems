# Scheduler-driven RayTracing Merge Design

## Summary

This feature hardens the Nexus Modeling scheduler-driven render path by making
`RayTracingMerge` a first-class compute post-pass that operates on a storage-writable
color target. The validation, renderer, and Vulkan backend must agree:

- `RayTracingMerge` may only occur after a `RayTracing` pass.
- The merge target must be in a storage-writable layout (`General` or `ShaderReadWrite`).
- The Vulkan swapchain must expose storage-capable swapchain images for the merge path.

This avoids reopening the swapchain render pass on a cleared color target and keeps
the RT blend as a proper compute dispatch.

## Affected modules

- `src/kernel/src/render/RenderGraphValidator.cpp`
- `src/kernel/src/render/Renderer.cpp`
- `src/kernel/src/backend/vulkan/VulkanSwapchain.cpp`
- `tests/kernel/test_RenderPipelineV1.cpp`
- `docs/feature/scheduler_rt_merge_design.md`

## Ownership

- `software-architect` — finalizes the feature design and expected module boundaries.
- `render-engineer` — implements validator and renderer path behavior.
- `gpu-backend-engineer` — adds Vulkan swapchain storage usage support.
- `test-engineer` — adds deterministic validator regression coverage.
- `build-engineer` — wires the feature if any new source/test file is added.
- `code-reviewer` — reviews the complete diff for correctness and repo conventions.

## Design details

### Render graph contract

`RenderGraphValidator` must enforce:

- `RayTracingMerge` cannot appear before a `RayTracing` pass.
- `RayTracingMerge` color target layout must be `General` or `ShaderReadWrite`.
- Existing deferred ordering rules remain unchanged: Shadow → Geometry → Composite.

### Renderer behavior

- The scheduler-driven render path records `RenderPassType::RayTracing` and
  `RenderPassType::RayTracingMerge` when path tracing is enabled.
- The merge pass is implemented as a compute dispatch that runs after the ray-tracing pass.
- The merge target is transitioned from `ColorAttachment` or `Present` into `General`
  before the dispatch, then returned to `Present` afterwards.

### Vulkan backend behavior

- `VulkanSwapchain::create()` requests `VK_IMAGE_USAGE_STORAGE_BIT` in addition to
  color attachment and transfer destination usage.
- This enables the swapchain image to be used directly as the storage-writable merge target.
- If the platform cannot support storage usage on swapchain images, this path will fail
  at swapchain creation and should be revisited with an intermediate composite target.

## Test strategy

Add validator tests for the new RT merge contract:

- `RenderGraphValidator.RayTracingMergeBeforeRayTracingIsRejected`
- `RenderGraphValidator.RayTracingMergeTargetNotStorageWritableIsRejected`

Maintain renderer behavior coverage already present in
`RendererBehavior.RayTracingMergeRunsComputePassAfterTraceRays`.

## Failure and edge cases

- If the Vulkan swapchain does not support storage usage, merge dispatch using the
  swapchain image will fail at creation time or during barrier setup.
- If `RayTracingMerge` is recorded without a prior `RayTracing` pass, validation must
  reject the graph before submission.

## Implementation plan

1. Add the design document and align reviewers.
2. Update `VulkanSwapchain.cpp` to request storage usage for swapchain images.
3. Confirm `RenderGraphValidator` already enforces RT merge ordering/layout.
4. Add validator regression tests in `tests/kernel/test_RenderPipelineV1.cpp`.
5. Run `cmake --build build --target nexus_kernel_tests -j$(nproc)` and
   `ctest --test-dir build --output-on-failure`.
6. Review the diff for warning-free build and deterministic Null backend behavior.
