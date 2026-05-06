# Shader Pipeline

Everything about how shaders are authored, compiled, loaded, and bound in the Nexus kernel.

---

## Overview

```
Author writes GLSL
        │
        ├─ Dev path  ──── .glsl file ─────► glslang (runtime) ──► SPIR-V bytes
        │                  or inline src                            │
        ├─ Release path ── .spv file ─────────────────────────────►│
        │                                                           │
        └─────────────── spirvCode span ─────────────────────────►│
                                                                    ▼
                                                          VkShaderModule
                                                                    │
                                                                    ▼
                                                          VkGraphicsPipeline /
                                                          VkComputePipeline /
                                                          VkRayTracingPipeline
```

The dispatch happens inside `IDevice::createShader()`. The caller never calls glslang directly.

---

## ShaderDesc dispatch rules

`ShaderDesc` has three mutually exclusive source fields. The kernel checks them in priority order:

| Priority | Field | Action |
|---|---|---|
| 1 (highest) | `spirvCode` (non-empty span) | Used directly — no file I/O, no compile |
| 2 | `sourcePath` (non-empty string) | File extension determines action (see table below) |
| 3 | `glslSource` (non-empty string) | Compiled from memory via glslang |

If all three are empty, `createShader` returns an invalid handle and logs an error.

### File extension dispatch (when `sourcePath` is set)

| Extension | Action |
|---|---|
| `.spv` | Raw binary load — no compilation |
| `.glsl` | glslang compile |
| `.vert`, `.frag`, `.comp`, `.geom`, `.tesc`, `.tese` | glslang compile, stage inferred from extension |
| `.rgen`, `.rmiss`, `.rchit`, `.rahit`, `.rint` | glslang compile, ray-tracing stage inferred |
| `.mesh`, `.task` | glslang compile, mesh/task stage |

If the extension is unrecognised, compilation is attempted and will fail with a diagnostic.

---

## GLSL version and target

All shaders compiled by glslang target:

- **GLSL version**: `#version 460 core`
- **Vulkan target**: Vulkan 1.3 (`glslang::EShTargetVulkan_1_3`)
- **SPIR-V version**: 1.6 (`glslang::EShTargetSpv_1_6`)
- **Entry point**: `main` by default — overridden via `ShaderDesc::entryPoint`
- **Messages**: `EShMsgSpvRules | EShMsgVulkanRules`

---

## Low-level API — `nexus::gfx::vkshader`

For code that needs direct access to the compiler (e.g., offline tools, shader cache builders):

```cpp
#include <nexus/gfx/vulkan/ShaderCompiler.h>

using namespace nexus::gfx::vkshader;

// Compile GLSL string to SPIR-V
ShaderCompileOutput out;
bool ok = compileGlslToSpirv(
    glslSource,          // std::string_view
    ShaderStage::Vertex, // stage
    out,                 // output: spirv vector + errorLog string
    "main",              // entry point
    "my_shader.vert"     // debug name (used in error messages)
);
if (!ok) {
    std::cerr << out.errorLog;
}

// Load from file (dispatches on extension)
ShaderCompileOutput spvOut;
bool loaded = loadShaderFileToSpirv("shaders/mesh.frag.glsl", ShaderStage::Fragment, spvOut);

// Create VkShaderModule directly (convenience)
std::string err;
VkShaderModule mod = vkCompileShader(
    device, glslSource, ShaderStage::Fragment, "main", &err
);

VkShaderModule fileMod = vkCreateShaderModuleFromFile(
    device, "shaders/triangle.vert.spv", ShaderStage::Vertex, "main"
);
```

---

## Pre-compiling shaders for release

Runtime glslang compilation adds ~5–50ms per shader at startup. For release builds, pre-compile:

```bash
# Using glslangValidator (from Vulkan SDK)
glslangValidator -V shaders/mesh.vert.glsl    -o shaders/mesh.vert.spv
glslangValidator -V shaders/mesh.frag.glsl    -o shaders/mesh.frag.spv
glslangValidator -V shaders/skinning.comp.glsl -o shaders/skinning.comp.spv

# Using glslc (from shaderc / Vulkan SDK)
glslc shaders/mesh.vert.glsl    -o shaders/mesh.vert.spv
glslc shaders/mesh.frag.glsl    -o shaders/mesh.frag.spv
```

Then point `ShaderDesc::sourcePath` at the `.spv` files — no compile overhead.

### CMake integration (optional)

```cmake
find_program(GLSLANG glslangValidator REQUIRED)

function(compile_shader target glsl_file spv_output)
    add_custom_command(
        OUTPUT  ${spv_output}
        COMMAND ${GLSLANG} -V ${glsl_file} -o ${spv_output}
        DEPENDS ${glsl_file}
        COMMENT "Compiling ${glsl_file}"
    )
    target_sources(${target} PRIVATE ${spv_output})
endfunction()

compile_shader(my_app shaders/mesh.vert.glsl ${CMAKE_BINARY_DIR}/shaders/mesh.vert.spv)
```

---

## Push constants layout convention

By convention, all Nexus shaders use a **single push constant block** at binding 0 covering all stages. The layout is free — each pipeline defines its own struct. The kernel allocates 128 bytes for push constants in every `VkPipelineLayout`.

Example per-draw push constant:

```glsl
// GLSL
layout(push_constant) uniform PushConstants {
    mat4  modelViewProj;   // 64 bytes
    uint  materialIndex;   // 4 bytes
    uint  drawId;          // 4 bytes
    float time;            // 4 bytes
    uint  _pad;            // 4 bytes
} pc;                      // total: 80 bytes ≤ 128
```

```cpp
// C++
struct PushConstants {
    glm::mat4 modelViewProj;
    uint32_t  materialIndex;
    uint32_t  drawId;
    float     time;
    uint32_t  _pad{};
};
cb.pushConstants(ShaderStage::Vertex | ShaderStage::Fragment, &pc, sizeof(pc));
```

---

## Shader debugging

### RenderDoc

RenderDoc intercepts all Vulkan calls and captures SPIR-V bytecode. For best source-level debugging:

1. Compile shaders with `-g` (debug info): `glslangValidator -V -g shader.vert.glsl -o shader.vert.spv`
2. Enable `ValidationLevel::Full` in `RenderContextDesc`.
3. Open RenderDoc and launch the application — shader source appears in the pipeline state viewer.

### Printf in shaders (Vulkan debug utils)

```glsl
#extension GL_EXT_debug_printf : enable

void main() {
    debugPrintfEXT("vertex id %d pos %f %f %f\n",
        gl_VertexIndex, inPos.x, inPos.y, inPos.z);
}
```

Requires `VK_KHR_shader_non_semantic_info` and validation layer `VK_LAYER_KHRONOS_validation` with `printf_enable=1`.

---

## glslang dependency notes

glslang is fetched via CMake `FetchContent` from the official release archive. Key build configuration:

```cmake
set(ENABLE_OPT           0 CACHE BOOL "" FORCE)  # no SPIRV-Tools optimizer required
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER       OFF CACHE BOOL "" FORCE)
```

**CMake targets exposed**:
- `glslang::glslang` — compiler library
- `glslang::SPIRV` — SPIR-V code generator
- `glslang-default-resource-limits` — `GetDefaultResources()` (required for built-in limits)

**Do not** set `ENABLE_OPT=1` in the current scaffold. SPIRV-Tools' parallel `ar` step corrupts archives on EXFAT/NTFS mount points. When SPIRV-Tools optimisation is needed, vendor a pre-built SPIRV-Tools static library instead.
