# Hardening Debt Ledger (Month 1-3)

Purpose: track remaining hardening debt in the early kernel/rendering stack with measurable closure criteria.

Status key:
- Open: not yet started.
- In progress: partial implementation exists but does not meet closure criteria.
- Closed: implementation and verification criteria are complete.

## HD-VK-SYNC-001 — Vulkan sync utility stub replacement

- Scope: src/kernel/src/backend/vulkan/VulkanSync.cpp, src/kernel/src/backend/vulkan/VulkanSync.h, src/kernel/src/backend/vulkan/VulkanDevice.cpp.
- Status: Closed.
- Current state:
  - Timeline semaphore operations are implemented and centralized in Vulkan sync helpers.
  - Device capability reporting now includes explicit timeline semaphore support.
  - Timeline tests are capability-gated.
  - Queue-family ownership transfer helper logic is implemented in VulkanQueue and routed in VulkanDevice upload barriers.
  - Explicit queue ownership transfer helper regression test added in tests/kernel/test_VulkanRegression.cpp.
  - No timeline/queue TODO-only comments remain in VulkanSync.cpp and VulkanQueue.cpp.
- Closure criteria:
  - Route Vulkan queue-ownership transfer helper logic through Vulkan sync/queue modules (no queue sync TODO-only stubs). (Done)
  - Add at least one explicit queue ownership transfer regression test in tests/kernel/test_VulkanRegression.cpp. (Done)
  - No timeline/queue TODO-only comments remain in VulkanSync.cpp and VulkanQueue.cpp. (Done)
- Verification:
  - cmake --build build -j$(nproc)
  - ctest --test-dir build --output-on-failure -R "VulkanRegression.Timeline|VulkanRegression.*Queue"

## HD-VK-PIPELINE-001 — Vulkan pipeline module ownership split

- Scope: src/kernel/src/backend/vulkan/VulkanPipeline.cpp and src/kernel/src/backend/vulkan/VulkanDevice.cpp.
- Status: Closed.
- Current state:
  - Graphics/compute/mesh/ray tracing pipeline creation ownership moved into VulkanPipeline.cpp.
  - Pipeline destruction ownership moved into VulkanPipeline.cpp via vkDestroyPipeline.
  - VulkanDevice forwards both create/destroy calls without changing public IDevice behavior.
  - PSO caches are explicitly invalidated on destroy by pipeline id across all pipeline categories.
- Closure criteria:
  - Move graphics/compute/mesh/ray tracing pipeline creation logic to VulkanPipeline.cpp. (Done)
  - Keep public IDevice behavior unchanged (API-compatible move only). (Done)
  - Add/adjust tests to ensure PSO cache behavior is preserved after relocation. (Done)
  - Add destruction/cache-invalidation parity assertion after destroy + recreate with identical descriptor. (Done)
- Verification:
  - cmake --build build -j$(nproc)
  - ctest --test-dir build --output-on-failure -R "VulkanPipeline|VulkanRegression.*Pipeline"

## HD-VK-TEST-001 — skipped Vulkan tests under strict runtime gating

- Scope: tests/kernel/test_VulkanPipeline.cpp, tests/kernel/test_VulkanRegression.cpp.
- Status: Closed.
- Current state:
  - A subset of low-level Vulkan tests now uses minimal Vulkan context (mesh/ray disabled).
  - Timeline tests now run when timeline semaphores are supported and skip explicitly otherwise.
  - Additional Vulkan regression tests now run under strict runtime setup instead of broad device-creation skips:
    - BufferLifecycleForCompositeMaterialTable
    - ShadowMapDepthTextureLifecycle
    - ShadowLightingContractBufferUploadLifecycle
    - DescriptorSetAllocateUpdateFreeLifecycle
    - DescriptorSetAllocateSamplerBinding
  - Remaining skips are explicit hardware capability gates (mesh shader and ray tracing paths).
- Remaining closure criteria:
  - Reduce avoidable skips by at least 3 additional tests using runtime capability checks (not environment broad-catch skips). (Done)
  - Every remaining Vulkan skip reason must map to explicit capability/environment predicates. (Done)
- Verification:
  - ctest --test-dir build --output-on-failure -R "VulkanPipeline|VulkanRegression"
  - Capture skip counts before/after and record deltas in docs/kernel-baseline-report.md.

## HD-RENDER-SHADOW-ATLAS-3C — shadow atlas descriptor instability

- Scope: tests/kernel/test_VulkanRegression.cpp (ShadowPassBindingDescBuildsCascadeAtlasPassDescriptors, ShadowPassBindingDescClearsAndRebuildsAcrossResize), renderer shadow scheduler path.
- Status: Closed.
- Current state:
  - Crash-path root cause traced to invalid color-target attachment handling in headless scheduler flow (intermittent out-of-bounds attachment access).
  - Dynamic rendering attachment setup now validates color/depth handles before indexing resource pools.
  - Scheduler render flow now skips swapchain composite pass when no valid color target exists.
  - Both shadow atlas regression tests are re-enabled by default and pass in full gate plus repeated stress runs.
- Closure criteria:
  - Root-cause and fix the crash in cascade atlas build and resize paths. (Done)
  - Re-enable both regression tests without unconditional skips. (Done)
  - Add one additional assertion proving descriptor validity for each populated atlas tile. (Done)
- Verification:
  - ctest --test-dir build --output-on-failure -R "VulkanRegression.ShadowPassBindingDescBuildsCascadeAtlasPassDescriptors|VulkanRegression.ShadowPassBindingDescClearsAndRebuildsAcrossResize"

## HD-VK-MESH-001 — mesh shader backend stub debt

- Scope: src/kernel/src/backend/vulkan/VulkanMeshShader.cpp.
- Status: Closed.
- Current state:
  - `VulkanMeshShader` now provides concrete runtime guards used by mesh pipeline creation.
  - Mesh capability reporting is runtime-accurate (`RenderContextDesc.enableMeshShaders`) rather than physical-device-only, keeping unsupported contexts deterministic.
  - Unsupported runtime path is covered by an always-runnable regression that verifies mesh pipeline creation fails cleanly when mesh shaders are disabled in context.
- Closure criteria:
  - Replace placeholder/stub-only sections with concrete implementation for supported hardware path. (Done)
  - Keep behavior deterministic on unsupported hardware via explicit capability guards. (Done)
  - Add at least one positive-path and one unsupported-path regression test. (Done)
- Verification:
  - cmake --build build -j$(nproc)
  - ctest --test-dir build --output-on-failure -R "VulkanRegression.*MeshShader"
  - ctest --test-dir build --output-on-failure

## HD-NEURAL-PLACEHOLDER-001 — neural placeholder hardening

- Scope: src/kernel/src/neural/NeuralRenderer.cpp, src/kernel/src/neural/OIDNDenoiser.cpp, plugin TODO paths.
- Status: Closed.
- Current state:
  - Neural backend selection is now capability-negotiated and only exposes executable backends.
  - Deterministic software fallback is always available and returns bilinear/pass-through behavior in unsupported runtimes.
  - DLSS/XeSS/OIDN runtime paths no longer advertise executable denoiser/upscaler backends while feature wiring is incomplete.
  - Headless-safe neural fallback regressions were added and pass under Null backend.
- Closure criteria:
  - Convert placeholder-only paths to explicit capability-negotiated behavior with deterministic fallback. (Done)
  - Ensure no TODO-only runtime path remains for enabled code paths. (Done)
  - Add deterministic tests for fallback behavior in headless/CI-safe mode. (Done)
- Verification:
  - cmake --build build -j$(nproc)
  - ctest --test-dir build --output-on-failure -R "Neural|Denoiser|RenderContext"
  - ctest --test-dir build --output-on-failure

## HD-NEURAL-FACTORY-001 — safe Vulkan handle acquisition in neural factory

- Scope: src/kernel/src/neural/NeuralRenderer.cpp, tests/kernel/test_VulkanRegression.cpp.
- Status: Closed.
- Current state:
  - Neural factory Vulkan-handle acquisition now uses safe typed runtime checks under Vulkan backend guard.
  - Removed unsafe shim-style cast pattern from neural backend selection flow.
  - Added Vulkan regression coverage to assert deterministic neural fallback behavior on live Vulkan runtime contexts.
- Closure criteria:
  - Replace unsafe Vulkan-handle shim logic in neural factory with a safe typed path. (Done)
  - Keep deterministic fallback behavior unchanged for unsupported/unwired neural paths. (Done)
  - Add at least one Vulkan runtime regression test validating factory behavior. (Done)
- Verification:
  - cmake --build build -j$(nproc)
  - ctest --test-dir build --output-on-failure -R "NeuralRenderer|VulkanRegression.NeuralRendererFactoryReturnsDeterministicFallbackOnVulkanRuntime"
  - ctest --test-dir build --output-on-failure
