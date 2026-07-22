# Nexus Modeling — Renderer Internals

*Deep dive into the Vulkan renderer architecture, frame scheduler, and render graph.*

---

## Overview

The renderer (`nexus::render`) is a **scheduler-driven, deferred rendering system** built on the `nexus::gfx` abstraction layer. It supports both Vulkan and Null (headless) backends.

**Key Properties:**
- **Scheduler-driven** (production path)
- **Deferred rendering** with G-Buffer
- **Dynamic rendering** (no render passes)
- **Explicit synchronization** (Vulkan sync2)
- **Ray tracing support** (optional, hardware-gated)
- **Frame capture** for diagnostics

---

## Architecture

```
nexus::render::Renderer
├── RenderGraph (scheduler)
│   ├── Shadow Pass
│   ├── G-Buffer Pass (Geometry)
│   ├── Lighting/Composite Pass
│   └── Ray Tracing Pass (optional)
├── FrameScheduler
│   ├── Acquire → Record → Submit → Present
├── SceneGraph (CPU-side)
│   ├── Nodes (transform, mesh refs, BLAS)
│   └── TLAS (ray tracing)
├── Resource Management
│   ├── BufferPool (vertex/index/meshlet)
│   ├── TextureAtlas
│   └── DescriptorAllocator
└── Pipeline Cache (PSO)
    ├── Graphics Pipelines
    ├── Compute Pipelines
    └── Ray Tracing Pipelines
```

---

## Render Graph (Scheduler)

The render graph is a **DAG of render passes** with explicit resource dependencies.

```cpp
struct RenderPass {
    virtual ~RenderPass() = default;
    virtual void setup(RenderGraphBuilder& builder) = 0;
    virtual void execute(const RenderContext& ctx, CommandBuffer& cmd) = 0;
    virtual std::string name() const = 0;
};

class RenderGraph {
    std::vector<std::unique_ptr<RenderPass>> passes;
    ResourceRegistry resources;
    
    void addPass(std::unique_ptr<RenderPass> pass);
    void compile();  // Topological sort, transition planning
    void execute(CommandBuffer& cmd);
};
```

### Pass Resource Declaration

```cpp
void ShadowPass::setup(RenderGraphBuilder& builder) {
    // Read
    builder.read<Texture>("scene.depth");
    
    // Write
    builder.write<Texture>("shadow.map")
        .format(VK_FORMAT_D32_SFLOAT)
        .usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT)
        .dimensions(4096, 4096);
    
    // Side-effect: updates global light view-proj
    builder.sideEffect("shadow.lightVP");
}

void GBufferPass::setup(RenderGraphBuilder& builder) {
    builder.read<Texture>("shadow.map");
    builder.read<Texture>("shadow.lightVP");
    
    builder.write<Texture>("gbuffer.position", VK_FORMAT_R16G16B16A16_SFLOAT, ...);
    builder.write<Texture>("gbuffer.normal",   VK_FORMAT_R16G16B16A16_SFLOAT, ...);
    builder.write<Texture>("gbuffer.albedo",   VK_FORMAT_R8G8B8A8_UNORM, ...);
    builder.write<Texture>("gbuffer.material", VK_FORMAT_R8G8B8A8_UNORM, ...);
    builder.write<Texture>("gbuffer.depth",    VK_FORMAT_D32_SFLOAT, ...);
}

void LightingPass::setup(RenderGraphBuilder& builder) {
    builder.read<Texture>("gbuffer.position");
    builder.read<Texture>("gbuffer.normal");
    builder.read<Texture>("gbuffer.albedo");
    builder.read<Texture>("gbuffer.material");
    builder.read<Texture>("shadow.map");
    
    builder.write<Texture>("swapchain.image")
        .format(VK_FORMAT_B8G8R8A8_SRGB)
        .usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
}
```

### Transition Planning

```cpp
struct ResourceTransition {
    TextureHandle texture;
    VkImageLayout oldLayout, newLayout;
    VkPipelineStageFlags2 srcStage, dstStage;
    VkAccessFlags2 srcAccess, dstAccess;
};

std::vector<ResourceTransition> RenderGraph::planTransitions() {
    // For each resource, track last writer and next reader
    // Insert barriers at pass boundaries
    // Use sync2: VkImageMemoryBarrier2
}
```

---

## Frame Scheduler

```cpp
class FrameScheduler {
public:
    struct FrameData {
        CommandBuffer cmd;
        std::vector<Semaphore> waitSemaphores, signalSemaphores;
        Fence fence;
        uint64_t frameIndex;
    };
    
    FrameData acquire();           // Acquire swapchain image
    void record(FrameData&, RenderGraph&);  // Record commands
    void submit(FrameData&);       // Submit to queue
    void present(FrameData&);      // Present to swapchain
    
    // In-flight frame management (triple buffering)
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
};
```

### Frame Timeline

```
CPU Timeline                    GPU Timeline
─────────────────────────────────────────────────────────
Frame N:   Acquire ──────► Record ──────► Submit ──────► Present
                │            │             │             │
                ▼            ▼             ▼             ▼
Frame N-1:              [GPU executing Frame N-1 commands]
Frame N-2:                                                 [GPU executing Frame N-2]
```

### Synchronization

```cpp
void FrameScheduler::submit(FrameData& frame) {
    VkSubmitInfo2 submit = {};
    submit.waitSemaphoreInfos = frame.waitSemaphores;
    submit.signalSemaphoreInfos = frame.signalSemaphores;
    submit.commandBufferInfos = {frame.cmd};
    
    // Timeline semaphores for fine-grained sync
    vkQueueSubmit2(queue, 1, &submit, frame.fence);
}
```

---

## Deferred G-Buffer Pipeline

### Vertex Shader

```glsl
// gbuffer.vert
#version 460
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inTangent;

layout(set = 0, binding = 0) uniform CameraData {
    mat4 viewProj;
    mat4 view;
    mat4 proj;
    vec3 cameraPos;
} camera;

layout(set = 1, binding = 0) uniform InstanceData {
    mat4 model;
    mat4 normalMatrix;
    uint materialId;
} instance;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUV;
layout(location = 3) out vec4 vTangent;

void main() {
    vec4 worldPos = instance.model * vec4(inPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = normalize((instance.normalMatrix * vec4(inNormal, 0.0)).xyz);
    vUV = inUV;
    vTangent = inTangent;
    gl_Position = camera.viewProj * worldPos;
}
```

### Fragment Shader

```glsl
// gbuffer.frag
#version 460
layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec4 vTangent;

layout(set = 2, binding = 0) uniform sampler2D albedoMap;
layout(set = 2, binding = 1) uniform sampler2D normalMap;
layout(set = 2, binding = 2) uniform sampler2D materialMap;

layout(location = 0) out vec4 outPosition;  // R16G16B16A16_SFLOAT
layout(location = 1) out vec4 outNormal;    // R16G16B16A16_SFLOAT
layout(location = 2) out vec4 outAlbedo;    // R8G8B8A8_UNORM
layout(location = 3) out vec4 outMaterial;  // R8G8B8A8_UNORM

void main() {
    vec3 albedo = texture(albedoMap, vUV).rgb;
    vec3 normal = texture(normalMap, vUV).xyz * 2.0 - 1.0;
    
    // Tangent space normal mapping
    vec3 T = normalize(vTangent.xyz);
    vec3 B = cross(vNormal, T) * vTangent.w;
    mat3 TBN = mat3(T, B, vNormal);
    normal = normalize(TBN * normal);
    
    outPosition = vec4(vWorldPos, 1.0);
    outNormal = vec4(normal, 0.0);
    outAlbedo = vec4(albedo, 1.0);
    outMaterial = texture(materialMap, vUV);
}
```

---

## Lighting/Composite Pass

### Clustered Forward+ (Tiled/Clustered)

```cpp
struct ClusterData {
    uint32_t lightCount;
    uint32_t lightIndices[MAX_LIGHTS_PER_CLUSTER];
};

struct LightData {
    vec3 position;
    float radius;
    vec3 color;
    float intensity;
    uint32_t type;  // 0=point, 1=spot, 2=directional
    vec3 direction;
    float innerCone, outerCone;
};

// Compute pass: build light clusters
// Fragment pass: shade using cluster data
```

### Lighting Shader

```glsl
// lighting.frag
#version 460
layout(set = 0, binding = 0) uniform CameraData { ... } camera;
layout(set = 1, binding = 0) uniform LightData { LightData lights[MAX_LIGHTS]; } lights;
layout(set = 2, binding = 0) uniform sampler2D positionMap;
layout(set = 2, binding = 1) uniform sampler2D normalMap;
layout(set = 2, binding = 2) uniform sampler2D albedoMap;
layout(set = 2, binding = 3) uniform sampler2D materialMap;
layout(set = 2, binding = 4) uniform sampler2D shadowMap;
layout(set = 2, binding = 5) uniform sampler2D shadowVP;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 pos = texture(positionMap, uv).xyz;
    vec3 normal = texture(normalMap, uv).xyz;
    vec3 albedo = texture(albedoMap, uv).rgb;
    vec4 material = texture(materialMap, uv);
    
    float metallic = material.r;
    float roughness = material.g;
    float ao = material.b;
    
    vec3 viewDir = normalize(camera.cameraPos - pos);
    vec3 lighting = vec3(0.0);
    
    // Clustered light iteration
    // ...
    
    // PBR BRDF
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    // ... GGX + Smith + Schlick
    
    outColor = vec4(lighting, 1.0);
}
```

---

## Shadow Mapping

### Cascade Shadow Maps (CSM)

```cpp
struct ShadowCascade {
    float splitDepth;
    mat4 lightViewProj;
    TextureHandle depthMap;
};

void ShadowPass::execute(const RenderContext& ctx, CommandBuffer& cmd) {
    for (int i = 0; i < NUM_CASCADES; ++i) {
        // Update light view-proj for cascade split
        // Render depth-only
        // Use conservative rasterization for contact hardening
    }
}
```

### Shadow Shader

```glsl
// shadow.vert
#version 460
layout(location = 0) in vec3 inPosition;
layout(set = 0, binding = 0) uniform mat4 cascadeViewProj[MAX_CASCADES];
layout(set = 1, binding = 0) uniform mat4 model;

void main() {
    vec4 worldPos = model * vec4(inPosition, 1.0);
    for (int i = 0; i < MAX_CASCADES; ++i) {
        gl_Position = cascadeViewProj[i] * worldPos;
        gl_Layer = i;  // Layered rendering
        EmitVertex();
        EndPrimitive();
    }
}
```

---

## Ray Tracing (Optional)

### Bottom-Level Acceleration Structure (BLAS)

```cpp
struct BLAS {
    VkAccelerationStructureKHR handle;
    Buffer vertexBuffer, indexBuffer;
    VkDeviceAddress vertexAddr, indexAddr;
};

BLAS Renderer::createBLAS(const Mesh& mesh) {
    VkAccelerationStructureGeometryKHR geom = {};
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geom.geometry.triangles.vertexData.deviceAddress = vertexAddr;
    geom.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geom.geometry.triangles.indexData.deviceAddress = indexAddr;
    geom.geometry.triangles.maxVertex = mesh.vertexCount();
    
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {};
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;
    
    // Build...
}
```

### Top-Level Acceleration Structure (TLAS)

```cpp
struct TLAS {
    VkAccelerationStructureKHR handle;
    Buffer instanceBuffer;
};

TLAS Renderer::updateTLAS(const SceneGraph& scene) {
    // Gather instance data (transform, BLAS handle, instance ID, hit group)
    // Build TLAS with refit if possible
}
```

### Ray Generation Shader

```glsl
// raygen.rgen
#version 460
#extension GL_EXT_ray_tracing : require

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlas;
layout(set = 0, binding = 1, rgba16f) uniform image2D outputImage;
layout(set = 1, binding = 0) uniform CameraData { ... } camera;

void main() {
    vec2 pixel = vec2(gl_LaunchIDEXT.xy);
    vec2 resolution = vec2(gl_LaunchSizeEXT.xy);
    vec2 uv = (pixel + 0.5) / resolution;
    
    // Generate ray from camera
    vec3 rayOrigin = camera.cameraPos;
    vec3 rayDir = normalize(...);
    
    // Trace
    traceRayEXT(tlas, RAY_FLAG_NONE, 0xFF, 0, 1, 0, 
                rayOrigin, 0.001, rayDir, 1e30, 0);
}
```

### Closest Hit Shader

```glsl
// chit.rchit
#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec3 hitColor;

void main() {
    // Access geometry data via gl_InstanceCustomIndexEXT
    // Compute PBR lighting
    hitColor = lighting(...);
}
```

---

## Resource Management

### Buffer Pool

```cpp
class BufferPool {
    struct Allocation { Buffer buffer; VkDeviceAddress address; size_t offset, size; };
    
    Allocation allocate(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memUsage);
    void free(Allocation& alloc);
    void reset();  // Frame reset for transient buffers
};
```

### Descriptor Allocator

```cpp
class DescriptorAllocator {
    struct Pool { VkDescriptorPool pool; uint32_t setsAllocated; };
    std::vector<Pool> pools;
    
    VkDescriptorSet allocate(VkDescriptorSetLayout layout);
    void reset();  // Frame reset
};
```

### Pipeline Cache (PSO)

```cpp
class PipelineCache {
    std::unordered_map<size_t, VkPipeline> graphicsCache;
    std::unordered_map<size_t, VkPipeline> computeCache;
    std::unordered_map<size_t, VkPipeline> raytracingCache;
    
    VkPipeline getGraphics(const GraphicsPipelineKey& key);
    VkPipeline getCompute(const ComputePipelineKey& key);
};
```

---

## Frame Capture & Diagnostics

```cpp
struct FrameCapture {
    struct PassInfo {
        std::string name;
        VkRect2D renderArea;
        std::vector<TextureInfo> inputs, outputs;
        double cpuTimeMs, gpuTimeMs;
    };
    
    std::vector<PassInfo> passes;
    uint64_t frameNumber;
    double totalFrameTimeMs;
};

class FrameCaptureExporter {
    void beginFrame(uint64_t frameNumber);
    void recordPass(const PassInfo& info);
    void endFrame();
    void exportJSON(const std::string& path);
    void exportChromeTrace(const std::string& path);
};
```

---

## Render Graph Validator

```cpp
class RenderGraphValidator {
public:
    enum class Severity { Error, Warning, Info };
    
    struct Issue {
        Severity severity;
        std::string passName;
        std::string message;
    };
    
    std::vector<Issue> validate(const RenderGraph& graph) {
        // Check:
        // 1. Resource write-read dependencies satisfied
        // 2. Layout transitions valid
        // 3. No circular dependencies
        // 4. All resources have valid formats/usages
        // 5. Synchronization barriers correct
        // 6. Render area coverage
    }
};
```

---

## Performance Optimizations

| Technique | Implementation |
|-----------|----------------|
| **Meshlets** | 64-triangle clusters, cluster culling |
| **Bindless** | Descriptor indexing, single descriptor set |
| **Async Compute** | Separate queue for compute (ray tracing, culling) |
| **Timeline Semaphores** | Fine-grained GPU-GPU sync |
| **Dynamic Rendering** | No render pass objects, explicit barriers |
| **Shader Compilation** | glslang → SPIR-V offline, pipeline cache online |

---

## Debugging

```bash
# Enable validation layers
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation ./nexus_modeling

# RenderDoc capture
renderdoc capture ./nexus_modeling

# Frame capture
./nexus_modeling --capture-frame 100 --output frame.json

# GPU timers
./nexus_modeling --gpu-timers
```

---

*Kernel v0.1.0-dev | 2026 tests passing | C++26 | Vulkan 1.3*