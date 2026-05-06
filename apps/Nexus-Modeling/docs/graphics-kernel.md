# Nexus Modeling — Graphics Kernel Architecture

## Overview

The graphics kernel is a **C++23** library (`nexus_gfx_core`) that provides a
backend-agnostic, hardware-scalable rendering foundation for all types of 3D
modeling (geometric, mesh, parametric, simulation, data visualization).

---

## Layer Map

```
┌──────────────────────────────────────────────────────┐
│             Application / Python Bindings             │
├──────────────────────────────────────────────────────┤
│   nexus::render                                       │
│   Renderer · SceneGraph · Camera · NeuralRenderer     │
├──────────────────────────────────────────────────────┤
│   nexus::gfx  (abstraction layer)                     │
│   RenderContext · IDevice · ICommandBuffer            │
│   ISwapchain · IGPUAllocator                          │
├──────────────────────────────────────────────────────┤
│   Backends                                            │
│   ┌─────────┐  ┌──────┐  ┌──────┐  ┌──────────┐     │
│   │ Vulkan  │  │ DX12 │  │Metal │  │   Null   │     │
│   │  1.4    │  │ (Win)│  │(Mac) │  │(headless)│     │
│   └─────────┘  └──────┘  └──────┘  └──────────┘     │
└──────────────────────────────────────────────────────┘
```

---

## Key Files

| Path | Purpose |
|---|---|
| `include/nexus/gfx/Types.h` | All shared enums, handles, capability structs |
| `include/nexus/gfx/Device.h` | `IDevice` — resource creation interface |
| `include/nexus/gfx/CommandBuffer.h` | `ICommandBuffer` — draw/dispatch recording |
| `include/nexus/gfx/Swapchain.h` | `ISwapchain` — surface presentation |
| `include/nexus/gfx/Allocator.h` | `IGPUAllocator` — GPU memory / tiled resources |
| `include/nexus/gfx/RenderContext.h` | Top-level kernel entry point |
| `include/nexus/render/Camera.h` | Perspective/ortho camera + frustum extraction |
| `include/nexus/render/SceneGraph.h` | Node hierarchy + frustum culling |
| `include/nexus/render/Renderer.h` | Frame pipeline orchestrator |
| `include/nexus/neural/NeuralRenderer.h` | Denoising / upscaling (DLSS4/XeSS/OIDN) |

---

## Hardware Tier Scaling

| Tier | VRAM | Features active |
|---|---|---|
| Low | < 4 GB | Rasterize only, bilinear upscale, OIDN CPU denoise |
| Mid | 4–8 GB | + Hybrid RT (ray queries), FSR3 |
| High | 8–16 GB | + Full RT pipeline, mesh shaders, XeSS/DLSS |
| Ultra | 16 GB+ | + Neural texture decode, DLSS4 RayReconstruction |

Zero hard requirements — every code path degrades gracefully to Low tier.

---

## Frame Pipeline (High/Ultra tier)

```
beginFrame()
  └─ waitForFence / reset
render(camera, scene)
  ├─ CPU frustum cull  (SceneGraph::collectVisible)
  ├─ GPU indirect draw prep (mesh shader task stage)
  ├─ Rasterize GBuffer  OR  Path-trace accumulation
  ├─ Async compute queue:
  │   ├─ Denoising  (DLSS4 / OIDN fallback)
  │   └─ Up-scaling (DLSS4 / XeSS / FSR3 / bilinear)
  ├─ TAA temporal resolve
  └─ Tone map → swapchain image
endFrame()
  └─ acquire → present
```

---

## Vulkan Feature Requirements

| Feature | Extension | Required |
|---|---|---|
| Dynamic Rendering | `VK_KHR_dynamic_rendering` | Yes |
| Synchronization 2 | `VK_KHR_synchronization2` | Yes |
| Descriptor Indexing | `VK_EXT_descriptor_indexing` | Yes |
| Buffer Device Address | `VK_KHR_buffer_device_address` | Yes |
| Mesh Shaders | `VK_EXT_mesh_shader` | Optional (tier High+) |
| RT Pipeline | `VK_KHR_ray_tracing_pipeline` | Optional (tier High+) |
| Ray Query | `VK_KHR_ray_query` | Optional (tier Mid+) |
| Acceleration Structure | `VK_KHR_acceleration_structure` | Optional |
| Timeline Semaphores | `VK_KHR_timeline_semaphore` | Yes |

---

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DNEXUS_BACKEND_VULKAN=ON \
      -DNEXUS_ENABLE_TESTS=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Dependencies (install via vcpkg or system package manager):
- Vulkan SDK ≥ 1.3  (`libvulkan-dev` / `vulkan-sdk`)
- VMA — Vulkan Memory Allocator  (`vcpkg install vk-memory-allocator`)
- GoogleTest (auto-fetched by CMake)
- Intel OIDN ≥ 2.0  (optional, `vcpkg install oidn`)

---

## Nexus-Cloud API Contract

- The kernel exposes a **headless render path** (Null backend) for server-side
  rendering jobs dispatched from Nexus-Cloud.
- `RenderContext` is created with `preferredBackend = Backend::Null` in
  cloud/CI environments.
- Frame output textures can be readback via `IDevice::uploadBuffer` inverse
  path for image export.

---

## Geometry Ownership Contract

Use the geometry bridge handoff helpers based on ownership intent:

- `GeometryRenderBridge::assignUploadedMeshToNode(...)`
  - Use when the upload container remains the owner of GPU resources.
  - Node receives a copy of mesh handles and section packets.
  - Caller must still destroy the `UploadedGeometryMesh` explicitly.

- `GeometryRenderBridge::adoptUploadedMeshByNode(...)`
  - Use when ownership should move from upload container to scene node.
  - Upload container is invalidated (handles and packets cleared).
  - Node becomes the sole owner of payload lifetime.

- `SceneGraph` auto-destroy on remove (`enableAutoDestroyNodeGpuPayloadOnRemove(...)`)
  - Use for node-owned payload lifetimes where scene graph controls teardown.
  - `removeNode(...)` and `clear()` tear down node payloads recursively before detach.
  - Combine with `adoptUploadedMeshByNode(...)` for one-owner semantics.

For full scene resets that must also release top-level RT state, use
`SceneGraph::clearAndDestroyTLAS(device)`.

Renderer integration helpers:

- `Renderer::resetScene(scene)`
  - Calls `scene.clear()` and invalidates renderer-side cached draw/material bindings.
  - Use when TLAS is managed externally or not active.

- `Renderer::resetSceneAndDestroyTLAS(scene)`
  - Calls `scene.clearAndDestroyTLAS(device)` and invalidates renderer-side cached state.
  - Use for full scene reset boundaries where scene-owned TLAS must be explicitly released.

---

## API Reference

### `nexus::gfx::RenderContext`

Top-level entry point. Owns the device, allocator, and queue manager.

```cpp
#include <nexus/gfx/RenderContext.h>

RenderContextDesc desc{
    .preferredBackend  = Backend::Vulkan,
    .validation        = ValidationLevel::Core,  // Off in release
    .enableRayTracing  = true,
    .enableMeshShaders = true,
    .appName           = "MyApp",
};
auto ctx = RenderContext::create(desc);

IDevice&       dev  = ctx->device();
IGPUAllocator& mem  = ctx->allocator();
HardwareTier   tier = ctx->hardwareTier();

// Async compute convenience wrappers on RenderContext
ctx->submitCompute({cmd});
ctx->submitComputeWithTimeline(
  {cmd},
  {waitSemaphore}, {waitValue},
  {signalSemaphore}, {signalValue}
);

CrossQueueTimelineSubmitDesc crossQueue{};
crossQueue.graphicsCmds      = {graphicsCmd};
crossQueue.computeCmds       = {computeCmd};
crossQueue.dependency        = CrossQueueTimelineDependency::ComputeToGraphics;
crossQueue.timelineSemaphore = timeline;
crossQueue.dependencyValue   = 1;
ctx->submitCrossQueueWithTimeline(crossQueue);

// Or build the descriptor from pass declarations + edges.
RenderPassGraphPlanner planner{};
auto cullPass  = planner.addPass({QueueType::Compute,  {computeCmd}});
auto shadePass = planner.addPass({QueueType::Graphics, {graphicsCmd}});
planner.addDependency(cullPass, shadePass);

auto planned = planner.buildCrossQueueSubmitDesc(timeline, 1);
ctx->submitCrossQueueWithTimeline(planned);

// For larger graphs, build a full ordered submission chain.
// Optional: assign completion fences to terminal passes for frame orchestration.
std::array<FenceHandle, 1> terminalFences{frameFence};
auto chain = planner.buildSubmissionPlan(
  timeline,
  1,
  FencePolicy::AutoFenceTerminalPasses,
  std::span<const FenceHandle>(terminalFences));
// terminalSubmitIndices points at completion submits in chain.submits.
for (uint32_t idx : chain.terminalSubmitIndices) {
  (void)idx;
}
auto completion = RenderPassGraphPlanner::buildCompletionWaitDesc(chain);
ctx->submitPlannedTimelineChain(chain);
```

**`ValidationLevel`** values:

| Level | Overhead | Notes |
|---|---|---|
| `Off` | Zero | Release builds |
| `Core` | Low | Vulkan validation layers — catches API misuse |
| `Full` | Medium | + GPU-assisted validation, sync checks |
| `Aftermath` | Medium | + NVIDIA Aftermath breadcrumbs (requires `NEXUS_ENABLE_AFTERMATH`) |

---

### `nexus::gfx::IDevice`

Resource creation and submission interface. Never instantiated directly — obtain via `RenderContext::device()`.

#### Buffer lifecycle

```cpp
BufferHandle vbo = dev.createBuffer({
    .sizeBytes = sizeof(vertices),
    .usage     = BufferUsage::VertexBuffer | BufferUsage::TransferDst,
    .memory    = MemoryHint::GpuOnly,
    .debugName = "Cube VBO",
});

dev.uploadBuffer(vbo, vertices.data(), sizeof(vertices));

dev.destroyBuffer(vbo);
```

#### Texture lifecycle

```cpp
TextureHandle tex = dev.createTexture({
    .extent     = { 1024, 1024, 1 },
    .mipLevels  = 10,
    .format     = Format::R8G8B8A8_Srgb,
    .usage      = TextureUsage::Sampled | TextureUsage::TransferDst,
    .debugName  = "Albedo",
});
// uploadTexture() — planned, currently stubbed
dev.destroyTexture(tex);
```

#### Shader creation

Three input modes — the kernel dispatches automatically based on which field is populated:

```cpp
// 1. Pre-compiled SPIR-V bytes (fastest path)
ShaderHandle sh = dev.createShader({
    .spirvCode = std::span<const uint32_t>{ spv_data, spv_size },
    .stage     = ShaderStage::Vertex,
    .debugName = "triangle.vert",
});

// 2. In-memory GLSL source (compiled via glslang at runtime)
ShaderHandle sh = dev.createShader({
    .glslSource = R"(#version 460 core
        layout(location = 0) in vec3 inPos;
        void main() { gl_Position = vec4(inPos, 1.0); }
    )",
    .stage     = ShaderStage::Vertex,
    .debugName = "inline.vert",
});

// 3. File path (.spv loaded raw, .glsl compiled)
ShaderHandle sh = dev.createShader({
    .sourcePath = "shaders/mesh.vert.glsl",
    .stage      = ShaderStage::Vertex,
});
```

#### Pipeline creation

```cpp
// Graphics pipeline (rasterisation)
PipelineHandle pipe = dev.createGraphicsPipeline({
    .vertexShader   = vert,
    .fragmentShader = frag,
    .topology       = Topology::TriangleList,
    .depthTest      = true,
    .depthWrite     = true,
    .blendEnable    = false,
    .debugName      = "MeshOpaque",
});

// Compute pipeline
PipelineHandle csPipe = dev.createComputePipeline({
    .computeShader = cs,
    .debugName     = "Skinning",
});

// Ray-tracing pipeline (requires tier Mid+)
PipelineHandle rtPipe = dev.createRayTracingPipeline({
    .rayGenShader       = rgen,
    .missShaders        = missSh,
    .closestHitShaders  = chitSh,
    .maxRecursionDepth  = 2,
});

// Mesh-shader pipeline (requires tier High+)
PipelineHandle msPipe = dev.createMeshShaderPipeline({
    .taskShader     = task,    // optional
    .meshShader     = mesh,
    .fragmentShader = frag,
});

// Sparse/tiled texture page management (requires caps().tiledResources)
TextureHandle tiledTex = dev.createTexture({
  .extent   = { 256, 256, 1 },
  .format   = Format::R8G8B8A8_Unorm,
  .usage    = TextureUsage::Sampled | TextureUsage::TransferDst,
  .isTiled  = true,
  .debugName = "SparseAtlas",
});

// Commit and evict individual pages (tileX, tileY, mipLevel)
ctx.allocator().commitTile(tiledTex, 0, 0, 0);
ctx.allocator().evictTile (tiledTex, 0, 0, 0);
```

Sparse tile ownership contract:

- `commitTile(texture, x, y, mip)` binds physical memory for a sparse page.
- `evictTile(texture, x, y, mip)` unbinds and releases that page memory.
- Calling commit/evict on non-sparse textures is a no-op.
- Use `caps().tiledResources` to gate sparse paging features at runtime.

Timeline semaphore contract:

- Create via `createTimelineSemaphore(initialValue)`.
- Signal from CPU via `signalSemaphore(sem, value)`.
- Wait from CPU via `waitSemaphore(sem, value, timeoutNs)`.
- Observe progress via `querySemaphoreValue(sem)`.
- For GPU queue orchestration, use `submitWithTimeline(...)` and provide per-submit
  wait/signal value arrays that are positionally matched with wait/signal semaphores.
- `RenderContext::submitComputeWithTimeline(...)` forwards this contract on the
  compute queue for higher-level async workflow code.
- `RenderContext::submitCrossQueueWithTimeline(...)` accepts one descriptor and
  emits ordered timeline submissions across compute/graphics queues.
- Supported dependency directions are `ComputeToGraphics` and `GraphicsToCompute`.
- `RenderPassGraphPlanner` can derive this descriptor directly from declared
  pass nodes and dependency edges, avoiding manual queue-array assembly.
- `RenderPassGraphPlanner::buildSubmissionPlan(...)` topologically orders graph
  passes and assigns timeline wait/signal values for each cross-queue edge.
- `RenderPassSubmissionPlan::terminalSubmitIndices` exposes terminal completion
  submits directly for frame orchestration.
- `RenderPassGraphPlanner::buildCompletionWaitDesc(...)` returns compact terminal
  completion waits (`fences` + `timelineWaitValues`) ready for frame sync code.
- `FencePolicy::AutoFenceTerminalPasses` can assign provided fences to terminal
  passes so frame orchestration can consume completion points directly.
- `FencePolicy::ExplicitFences` enforces exact fence count matching terminal pass
  count, useful for strict frame-graph contracts.
- `RenderContext::submitPlannedTimelineChain(...)` executes that ordered chain
  through `IDevice::submitWithTimeline(...)` per step.
- This enables async compute/graphics dependency chains without CPU stalls.
- Backends without native timeline support may fall back to binary/no-op semantics.

#### Command buffer submission

```cpp
CmdBufHandle cmd = dev.allocateCommandBuffer();
ICommandBuffer& cb = *dev.getCommandBuffer(cmd);

cb.begin();
#### Pipeline State Object (PSO) caching

Pipeline creation is automatically cached by descriptor hash to avoid redundant Vulkan PSO compilations.
When two `createGraphicsPipeline()` / `createComputePipeline()` / etc. calls use descriptors with identical
shader handles, blend states, depth states, and other immutable properties, the second call returns the
**same cached handle** without recompiling.

**Example:**
```cpp
// First call: compiles and caches
PipelineHandle pipe1 = dev.createGraphicsPipeline({
  .vertexShader   = vert,
  .fragmentShader = frag,
  .depthTest      = true,
  // ... other state ...
});

// Second call with identical state: returns cached handle immediately
PipelineHandle pipe2 = dev.createGraphicsPipeline({
  .vertexShader   = vert,
  .fragmentShader = frag,
  .depthTest      = true,
  // ... other state ...
});

// Both are the same resource:
assert(pipe1.id == pipe2.id);
```

This optimization is transparent — no API changes required. Use for frame-to-frame pipeline reuse,
shader-to-material binding tables, and deferred rendering pass setup.

cb.setViewport({ .x=0, .y=0, .width=1920, .height=1080, .minDepth=0, .maxDepth=1 });
cb.setScissor ({ .offset={0,0}, .extent={1920,1080} });

cb.beginRenderPass(renderPass, colorTargets, depthTarget, clearValues, renderArea);
cb.bindPipeline(pipe);
cb.bindVertexBuffer(vbo);
cb.bindIndexBuffer(ibo);
cb.pushConstants(ShaderStage::Vertex | ShaderStage::Fragment, &pc, sizeof(pc));
cb.drawIndexed(indexCount);
cb.endRenderPass();
cb.end();

dev.submit(cmd, QueueType::Graphics, fence);
dev.freeCommandBuffer(cmd);
```

---

### Handle system

All GPU resources are represented as `Handle<Tag>` — a 64-bit integer with a sentinel `kNullHandle = UINT64_MAX`:

```cpp
// Check validity before use
if (!myBuffer.valid()) { /* allocation failed */ }

// Handles are value types — copy freely
BufferHandle copy = myBuffer;

// Null handle constant
BufferHandle empty{}; // empty.valid() == false
```

**Why handles, not pointers**: Handles are safe to copy, compare, store in arrays, and transmit over IPC/RPC to Nexus-Cloud. The concrete GPU resource lives inside the backend's slot-map; the handle is just the key.

---

### `nexus::gfx::ICommandBuffer`

| Category | Methods |
|---|---|
| Lifecycle | `begin()`, `end()`, `reset()` |
| Render pass | `beginRenderPass(…)`, `endRenderPass()` |
| Viewport/scissor | `setViewport()`, `setScissor()` |
| Pipeline | `bindPipeline()` |
| Vertex/index | `bindVertexBuffer()`, `bindIndexBuffer()` |
| Draw | `draw()`, `drawIndexed()`, `drawIndirect()`, `drawIndexedIndirect()` |
| Mesh shaders | `drawMeshTasks()`, `drawMeshTasksIndirect()` |
| Compute | `dispatch()`, `dispatchIndirect()` |
| Ray tracing | `traceRays()` |
| Push constants | `pushConstants(stages, data, sizeBytes)` |
| Barriers | `globalMemoryBarrier()` |
| Debug | `beginDebugLabel()`, `endDebugLabel()`, `insertDebugLabel()` |

---

### `nexus::gfx::ISwapchain`

```cpp
// Acquire the next frame image
AcquiredFrame frame = swapchain->acquire();
if (frame.result == AcquireResult::OutOfDate) {
    swapchain->resize(newExtent);
    return;
}

// … record commands targeting frame.colorTarget …

// Present
PresentResult pr = swapchain->present(frame);
if (pr == PresentResult::OutOfDate) swapchain->resize(newExtent);
```

**`PresentMode`** options:

| Mode | Tearing | Latency | Use case |
|---|---|---|---|
| `Immediate` | Yes | Lowest | Benchmarking, no-vsync dev |
| `Fifo` | No | 1 frame | Default |
| `FifoRelaxed` | Possible if late | 1 frame | Smoother on GPU spikes |
| `Mailbox` | No | Low | Triple-buffered gaming |

---

### `nexus::gfx::IGPUAllocator`

Backed by **VMA 3.1.0** on Vulkan.

```cpp
// Tiled / sparse textures (streaming large textures)
bool supported = allocator.supportsTiledResources();

// Memory budget query
MemoryBudget budget = allocator.queryBudget();
// budget.totalBytes / budget.usedBytes
```

Direct buffer/texture allocation happens through `IDevice` — `IGPUAllocator` provides supplementary operations for tiled resources and budget monitoring.

---

### Hardware Tier capabilities

Query at runtime via `IDevice::caps()`:

```cpp
const DeviceCapabilities& caps = dev.caps();

if (caps.rayTracingTier >= 1) { /* use RT pipeline */ }
if (caps.meshShaders)         { /* use mesh shader path */ }
if (caps.asyncCompute)        { /* submit to async compute queue */ }
```

| Field | Type | Meaning |
|---|---|---|
| `rayTracingTier` | `uint8_t` | 0=none, 1=ray queries, 2=full RT pipeline |
| `meshShaders` | `bool` | VK_EXT_mesh_shader available |
| `asyncCompute` | `bool` | Separate compute queue available |
| `descriptorIndexing` | `bool` | Bindless descriptor arrays |
| `bufferDeviceAddress` | `bool` | GPU pointer arithmetic |
| `timelineSemaphores` | `bool` | Fine-grained async sync |
| `vram` | `uint64_t` | Dedicated VRAM in bytes |
| `unifiedMemory` | `bool` | Integrated / shared-memory architecture |

---

### Neural renderer (`nexus::neural`)

```cpp
#include <nexus/neural/NeuralRenderer.h>

// Factory selects best available implementation at runtime
auto nr = nexus::neural::NeuralRenderer::create(device);

nr->denoise(colorTarget, albedoTarget, normalTarget, outputTarget);
nr->upscale(lowResTarget, outputTarget, jitter, motionVectors);
```

The factory probes (in order): **DLSS4** (NVIDIA) → **XeSS** (Intel/any) → **OIDN** (CPU fallback). All are loaded at runtime via `dlopen`/`LoadLibrary` — no hard link dependency.

---

## Implementation status

| System | Status | Notes |
|---|---|---|
| `IDevice` interface | ✅ Complete | All virtual methods defined |
| Null backend | ✅ Complete | All methods stubbed, CI-safe |
| Vulkan backend — init | ✅ Complete | Instance, device, queues |
| Vulkan buffer/texture | ✅ Complete | VMA-backed |
| Vulkan command buffer | ✅ Complete | Full `vkCmd*` implementation |
| Vulkan swapchain | ✅ Complete | Wayland/XCB/X11/Win32/Metal surfaces, HDR |
| Vulkan allocator | ✅ Complete | VMA 3.1 with budget extension |
| Vulkan ray tracing | ✅ Complete | BLAS/TLAS build helpers |
| Neural renderer | ✅ Complete | DLSS4/XeSS/OIDN factory |
| Shader compilation | ✅ Complete | glslang runtime compile, .spv/.glsl file load |
| Graphics pipeline | ✅ Complete | VkPipelineLayout + VkGraphicsPipeline, dynamic rendering |
| Compute pipeline | 🔲 Stub | `createComputePipeline` returns empty handle |
| Ray-tracing pipeline | 🔲 Stub | `createRayTracingPipeline` returns empty handle |
| Mesh-shader pipeline | 🔲 Stub | `createMeshShaderPipeline` returns empty handle |
| Texture upload | 🔲 Stub | `vkCmdCopyBufferToImage` path not yet wired |
| Render loop | 🔲 Planned | Acquire → Record → Submit → Present frame scheduler |
| Frame synchronisation | 🔲 Planned | Timeline semaphores, triple-buffer fence pool |
| GPU timestamps | 🔲 Planned | `vkCmdWriteTimestamp2` profiling layer |
