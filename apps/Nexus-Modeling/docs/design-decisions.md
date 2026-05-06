# Kernel Design Decisions

This document records the *why* behind each major technical choice in the graphics kernel. Future contributors and the Nexus-Cloud integration team should read this before proposing changes to the core.

---

## Vulkan-first, not cross-platform-first

**Decision**: The primary backend is Vulkan 1.4. Metal and DX12 backends exist in the layer map but are not implemented during the scaffold phase.

**Rationale**:
- Nexus Modeling's primary platform is Linux/Windows. Vulkan covers both natively.
- Vulkan 1.4 gives access to every modern GPU feature needed: mesh shaders, ray tracing, timeline semaphores, descriptor indexing, dynamic rendering.
- Vulkan's explicit model forces correct resource tracking up front — bugs are caught at validation layer time, not silently on some platforms.
- SPIR-V as the universal shader IR means Metal/DX12 backends can transpile from the same compiled shaders via SPIRV-Cross/DXC — no duplicate HLSL/MSL authoring needed.
- Apple's Metal has first-class Vulkan via MoltenVK, so iOS/macOS reach is not blocked.

**What this is not**: The `IDevice` abstraction is intentionally thin and backend-agnostic. Nothing in application code should depend on Vulkan types.

---

## Dynamic Rendering — no VkRenderPass

**Decision**: All Vulkan rendering uses `VK_KHR_dynamic_rendering` (`vkCmdBeginRenderingKHR`). No `VkRenderPass` objects are created at runtime.

**Rationale**:
- Render passes in Vulkan 1.0/1.1 were tile-based-GPU metadata. On all modern discrete GPUs, the tiling benefit is marginal; the cost is subpass/dependency boilerplate.
- Dynamic rendering is core in Vulkan 1.3+ and promoted from extension in 1.2.
- This simplifies the `IDevice` interface: `RenderPassHandle` in `GraphicsPipelineDesc` is a format-only descriptor, not a heavyweight Vulkan object.
- RenderDoc, Nsight, and all major capture tools fully support dynamic rendering.

**Trade-off**: Tile-based GPUs (mobile, some ARM) lose some implicit bandwidth optimisations. When a mobile Vulkan backend is added, a render-pass fallback path should be offered for VK_QCOM_render_pass_transform.

---

## Reversed-Z depth

**Decision**: Depth buffer uses reversed-Z — far plane maps to 0.0, near plane to 1.0. Compare op is `VK_COMPARE_OP_GREATER_OR_EQUAL`.

**Rationale**:
- Float32 depth precision is distributed logarithmically from 0.0. Reversed-Z places the dense precision region at distance (far), not at the camera (near), which is where z-fighting actually occurs.
- CAD and architectural modelling commonly have scenes spanning metres to millimetres in the same view — standard-Z z-fight visibly at even moderate camera distances.
- NVIDIA, AMD, and Intel all recommend reversed-Z in their developer guides.
- Cost: zero at runtime. Projection matrix is negated once in `Camera::projection()`.

---

## Typed handle system instead of pointers

**Decision**: All GPU resources are returned as `Handle<Tag>` — a 64-bit integer keyed into a backend slot-map. No raw pointers or `std::shared_ptr` cross the `IDevice` boundary.

**Rationale**:
- Handles are trivially copyable, storable in arrays, hashable, and safe to pass to Nexus-Cloud over IPC/JSON without pointer fixup.
- The slot-map (fixed-size array with free-list) gives O(1) alloc/free and pointer-stable storage with no heap fragmentation.
- Incorrect use (use-after-destroy, double-free) is caught at the slot-map level in debug builds — far cleaner than dangling pointer UB.
- `kNullHandle = UINT64_MAX` — safe sentinel distinct from index 0, no ambiguity.

**Pattern**: `handle.valid()` before any use. The backend checks in debug; in release it trusts the caller (no overhead).

---

## VMA for GPU memory management

**Decision**: `VulkanMemoryAllocator` (VMA) 3.1.0 is the only allocator used by the Vulkan backend.

**Rationale**:
- VMA is the de-facto standard for Vulkan memory management — used by Godot, Unreal, Filament, and hundreds of shipping titles.
- It implements sub-allocation, defragmentation, budget tracking (`VK_EXT_memory_budget`), and DEDICATED_ALLOCATION automatically.
- `VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT` enables BDA on all allocations, required for ray tracing and mesh shaders.
- A single `VMA_IMPLEMENTATION` translation unit (`VulkanAllocator.cpp`) prevents multiple-definition ODR issues.

**Configuration**:
```cpp
// Dynamic Vulkan function loading — no static link to libvulkan needed
VMA_STATIC_VULKAN_FUNCTIONS  0
VMA_DYNAMIC_VULKAN_FUNCTIONS 1
```

---

## Push constants over descriptor sets for per-draw data

**Decision**: `createGraphicsPipeline` always creates a `VkPipelineLayout` with a 128-byte push constant range covering all stages. Descriptor sets are used only for large/persistent data.

**Rationale**:
- 128 bytes covers a `mat4` (64B) + `vec4` + a few `uint` indices — sufficient for most per-draw data (transform, material index, draw ID).
- Push constants are register-backed on all modern Vulkan drivers — zero overhead, no descriptor pool management.
- `ICommandBuffer::pushConstants()` exposes this cleanly: `cb.pushConstants(stages, &data, sizeof(data))`.
- 128 bytes is the guaranteed minimum push constant space in Vulkan spec (many drivers give 256+).

---

## glslang runtime shader compilation

**Decision**: glslang is a FetchContent dependency that compiles GLSL to SPIR-V at runtime when `.glsl` source is provided.

**Rationale**:
- Artists and shader developers iterate fastest with hot-reload from source files.
- Pre-compiled `.spv` is the release path — zero runtime compile cost in shipping builds.
- `ENABLE_OPT=0` (no SPIRV-Tools): removes the SPIRV-Tools transitive dependency, which is large and has filesystem cleanup issues on some mount types (EXFAT). GLSL correctness validation is sufficient in glslang alone.
- The compiler is wrapped behind `nexus::gfx::vkshader::compileGlslToSpirv()` — callers never touch glslang headers directly.

**Warning**: Do not enable `ENABLE_OPT=1` without first verifying SPIRV-Tools archives build cleanly on your filesystem. The parallel `ar` tool corrupts archives on EXFAT/NTFS mounts.

---

## Neural plugins via runtime dlopen

**Decision**: DLSS4, XeSS, and OIDN are loaded at runtime via `dlopen`/`LoadLibrary`. None are linked at compile time.

**Rationale**:
- DLSS requires NVIDIA hardware. XeSS requires Intel hardware. OIDN runs on CPU. Zero reason to hard-link all three everywhere.
- Runtime loading means the binary ships without SDK redistributables and falls back gracefully on hardware that lacks a given feature.
- Build options `NEXUS_ENABLE_DLSS`, `NEXUS_ENABLE_XESS`, `NEXUS_ENABLE_OIDN` only control *whether to attempt loading* — they do not create link-time dependencies.
- The `nexus::neural::NeuralRenderer::create(device)` factory encapsulates all probing; application code sees only `INeuralRenderer`.

---

## Hardware tier system

**Decision**: Four tiers (Low / Mid / High / Ultra) gate feature activation at runtime. No features are hard-disabled at build time.

**Rationale**:
- A modelling suite must run on integrated graphics (students, low-cost workstations) as well as flagship workstation GPUs.
- Compile-time feature gates create multiple binaries to maintain. Runtime gates keep one binary with graceful degradation.
- The tier is computed once during `RenderContext::create()` from `DeviceCapabilities::vram` and feature presence.
- All code paths degrade cleanly — e.g., if `caps.meshShaders == false`, `IDevice::createMeshShaderPipeline` returns an invalid handle and the renderer falls back to indexed triangle lists.

| Tier | VRAM threshold | Key feature unlocks |
|---|---|---|
| Low | < 4 GB | Rasterisation only, bilinear upscale, OIDN CPU denoising |
| Mid | 4–8 GB | Ray queries, FSR3 |
| High | 8–16 GB | Full RT pipeline, mesh shaders, XeSS/DLSS |
| Ultra | > 16 GB | Neural texture decode, DLSS4 Ray Reconstruction |

---

## Async compute on a separate queue

**Decision**: Denoising, upscaling, and skinning compute workloads submit to a dedicated async compute queue when `caps.asyncCompute == true`.

**Rationale**:
- On AMD and NVIDIA, the async compute engine runs concurrently with the graphics pipeline — denoising overlaps with the next frame's geometry stage.
- Timeline semaphores (`VK_KHR_timeline_semaphore`) sequence the async compute queue relative to the graphics queue without CPU round-trips.
- When async compute is unavailable (single-queue devices), work falls back to the graphics queue inline — same `ICommandBuffer` API, different `QueueType` parameter.

---

## Single-pass GBuffer vs deferred

**Decision**: The high-level frame pipeline is designed for **deferred rendering** with a GBuffer pass, but the kernel itself is pipeline-agnostic.

**Rationale**:
- Nexus Modeling scenes are geometry-heavy (millions of mesh faces, hundreds of materials). Deferred shading allows arbitrary material complexity with a single geometry pass.
- The `Renderer` class composes the frame: GBuffer → lighting → post (TAA, denoise, upscale). The kernel (`IDevice`, `ICommandBuffer`) has no opinion on the composition.
- Forward+ is a valid alternative for lower-tier hardware — the abstraction layer does not prevent it.
