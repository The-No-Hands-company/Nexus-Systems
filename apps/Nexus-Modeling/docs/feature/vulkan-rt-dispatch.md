# Vulkan Ray-Tracing Dispatch — Shader Binding Table

## Summary

Completes the CPU-side plumbing for real `vkCmdTraceRaysKHR` dispatch on RT-capable
hardware. The ray-tracing *pipeline* was already created (raygen/miss/hit groups via
`vkCreateRayTracingPipelinesKHR`), but `traceRays` ran with empty shader-binding-table
(SBT) regions — so no shaders could be reached. This adds SBT construction and binds
it automatically, closing roadmap month-13 #1.

## What landed

- **SBT layout math** ([VulkanShaderBindingTable.h](../../src/kernel/src/backend/vulkan/VulkanShaderBindingTable.h)):
  pure, GPU-free `computeShaderBindingTableLayout(handleSize, handleAlignment,
  baseAlignment, missCount, hitCount)` implementing the Vulkan alignment rules
  (per-record stride = `align(handleSize, handleAlignment)`; each region base-aligned;
  raygen `size == stride`). **Unit-tested** in `tests/kernel/test_VulkanShaderBindingTable.cpp`.
- **RT pipeline properties** ([VulkanDevice](../../src/kernel/src/backend/vulkan/VulkanDevice.cpp)):
  query `VkPhysicalDeviceRayTracingPipelinePropertiesKHR` (chained into the existing
  `VkPhysicalDeviceProperties2`), exposed via `rtPipelineProps()`.
- **SBT build** ([VulkanRayTracing.cpp](../../src/kernel/src/backend/vulkan/VulkanRayTracing.cpp)
  `buildShaderBindingTable`): `vkGetRayTracingShaderGroupHandlesKHR` → host-visible,
  device-address SBT buffer, handles copied into raygen/miss/hit slots at the computed
  offsets → strided device-address regions.
- **Wiring**: `vkCreateRayTracingPipeline` builds and stores the SBT in the pipeline
  entry; `VulkanCommandBuffer::bindPipeline` activates those regions when an RT pipeline
  is bound, so the existing `bindPipeline → traceRays` path now dispatches with a valid
  SBT. `vkDestroyPipeline` frees the SBT buffer.

## Verification status

- **Unit-tested:** SBT layout/alignment arithmetic (the error-prone part).
- **Compile-verified:** all SBT/pipeline/command-buffer wiring builds clean against the
  Vulkan headers (`-Werror`); full kernel suite green with no regressions.
- **Not yet runtime-verified:** the actual ray dispatch requires a GPU exposing
  `VK_KHR_ray_tracing_pipeline` (tier 2). On software/CI ICDs without it,
  `rtPipelineProps` is zero and `buildShaderBindingTable` returns an invalid table;
  `traceRays` is null-guarded — so non-RT paths are unaffected. Needs validation on
  RT hardware (RenderDoc/Nsight capture + a known-good raygen/miss/hit shader set).

## Bring-up shaders & hardware-gated test

- **Shaders** ([shaders/rt/raytrace.{rgen,rmiss,rchit}](../../shaders/rt/)): a minimal
  raygen (one primary ray/pixel → output image), miss (background color), and
  closest-hit (barycentric debug color) set — the canonical assets for RT bring-up.
- **Test** ([tests/kernel/test_VulkanRayTracingDispatch.cpp](../../tests/kernel/test_VulkanRayTracingDispatch.cpp)):
  - `ShadersCompileToModules` — runs wherever a Vulkan device exists (GLSL→SPIR-V +
    module creation need no GPU RT support). **Verifies the bring-up shaders compile.**
  - `RayTracingPipelineBuildsOnHardware` — gated on `caps().rayTracingTier >= 2`;
    creates the RT pipeline (exercising the SBT build) on real RT hardware, skips
    otherwise.
  - `DispatchAndReadbackOnHardware` — gated on tier 2; the full path: triangle
    BLAS/TLAS → RT pipeline + SBT → `traceRays` into an SSBO → readback → asserts a
    mix of hit (barycentric) and miss (background) pixels. Skips on this CI device.

## Descriptor binding + readback (completing the dispatch path)

A real dispatch must bind the TLAS and an output target and read results back; both
were missing from the public API and are now added:

- `DescriptorType::AccelerationStructure` + `DescriptorBindingDesc::accelStruct`, with
  the Vulkan `updateDescriptorSet` path writing `VkWriteDescriptorSetAccelerationStructureKHR`.
- `RayTracingPipelineDesc::descriptorBindings` — RT pipeline creation now builds a
  set-0 `VkDescriptorSetLayout` from these (previously pipeline layouts were
  push-constant-only with `setLayoutCount = 0`, so nothing could be bound). The layout
  is built to match `allocateDescriptorSet`, so a descriptor set with the same bindings
  is layout-compatible. Freed in `vkDestroyPipeline`.
- `IDevice::readbackBuffer` — maps a host-visible (`GpuToCpu`) buffer and copies to host
  (Vulkan invalidates the allocation first); default no-op on backends that don't support it.

## Pipeline descriptor set layouts (raster + multi-set)

The same descriptor-set-layout wiring now covers all pipeline types, not just RT:
- `GraphicsPipelineDesc` / `ComputePipelineDesc` / `MeshShaderPipelineDesc` /
  `RayTracingPipelineDesc` each take `descriptorBindings` (a convenience for a single
  set 0) and `descriptorSetLayouts` (`DescriptorSetLayoutDesc[]`, index = set number)
  for pipelines needing multiple sets. Previously every pipeline layout was
  push-constant-only (`setLayoutCount = 0`), so descriptor binding was invalid on
  hardware — only exercised on the Null backend.
- A shared `buildPipelineSetLayouts()` builds the per-set `VkDescriptorSetLayout`s to
  match `allocateDescriptorSet` (descriptorCount 1, stage ALL); the pipeline owns and
  frees them.
- The deferred composite path is now hardware-complete: `Renderer::compositeCoreSetLayout()`
  (set 0) + `compositeShadowSetLayout()` (set 1) feed a two-set pipeline layout, verified
  on a real device by `VulkanPipeline.CompositeTwoSetLayoutCreatesValidPipeline` (pipeline
  valid + both descriptor sets layout-compatible). The renderer binds from the same tables,
  so layout and runtime bindings cannot drift.

## Deferred-draw integration harness

`tests/kernel/test_VulkanDeferredDraw.cpp` exercises the deferred path end-to-end on a
real Vulkan device (passes on the software rasterizer, so it runs in CI):
- `OffscreenClearAndReadback` — render to an offscreen color target, copy it to a buffer
  via the new `ICommandBuffer::copyTextureToBuffer`, read back, and verify the cleared
  pixels. Establishes the render-target + readback verification mechanism.
- `DescriptorBoundDrawReadsUniformBuffer` — a fullscreen draw whose fragment shader reads a
  UBO bound through a descriptor set; readback confirms the bound value reached the shader.
- `DescriptorBoundDrawReadsTwoSets` — the full composite shape: a fragment shader reads a
  UBO from set 0 and a UBO from set 1 and writes their sum; readback confirms multi-set
  descriptor binding is valid AT DRAW TIME.

Note: fullscreen-triangle passes set `cullMode = CullMode::None` — the standard
fullscreen triangle is back-facing under Vulkan's Y-down clip space and would otherwise be
culled by the default back-face culling.

### Software-rasterizer gate (lavapipe/llvmpipe)

CPU software rasterizers **advertise** `VK_KHR_ray_tracing_pipeline` but their
implementations are incomplete and **crash during RT pipeline creation** (observed:
SIGSEGV in `lvp_CreateRayTracingPipelinesKHR`). `VulkanDevice::queryCapabilities` now
detects `VK_PHYSICAL_DEVICE_TYPE_CPU` (`DeviceCapabilities::softwareDevice`) and refuses
to report RT capability on such devices (`rayTracingPipeline`/`rayQuery` forced false,
tier 0). This keeps both the renderer and the bring-up test off the broken path — the
test skips cleanly instead of crashing.

## Follow-ups

- Stage the SBT to device-local memory (currently host-visible; fine for correctness,
  not the fast path).
- Per-geometry hit-group indexing (SBT record offsets) once material→hit-group mapping
  is defined.
