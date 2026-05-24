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

## Follow-ups

- Stage the SBT to device-local memory (currently host-visible; fine for correctness,
  not the fast path).
- Per-geometry hit-group indexing (SBT record offsets) once material→hit-group mapping
  is defined.
