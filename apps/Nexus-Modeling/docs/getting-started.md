# Getting Started — Nexus Modeling

## Prerequisites

| Requirement | Minimum version | Notes |
|---|---|---|
| C++ compiler | GCC 13 / Clang 17 / MSVC 19.38 | C++23 required |
| CMake | 3.28 | `cmake --version` |
| Vulkan SDK | 1.3 (1.4 recommended) | `vulkaninfo` to verify |
| Python | 3.11+ | Only needed with `NEXUS_ENABLE_PYTHON=ON` |

### Linux (Ubuntu 24.04 / Arch / Fedora)

```bash
# Ubuntu/Debian
sudo apt install build-essential cmake libvulkan-dev vulkan-tools glslang-tools

# Arch
sudo pacman -S base-devel cmake vulkan-devel glslang

# Fedora
sudo dnf install gcc-c++ cmake vulkan-devel glslang
```

### Windows

1. Install [Vulkan SDK](https://vulkan.lunarg.com/sdk/home) — ensure `VULKAN_SDK` env variable is set.
2. Install [CMake](https://cmake.org/download/) ≥ 3.28.
3. Use Visual Studio 2022 (v143 toolset) or LLVM Clang 17+.

### macOS

```bash
brew install cmake glslang
# Install Vulkan SDK from https://vulkan.lunarg.com/sdk/home (MoltenVK included)
```

---

## Clone and build

```bash
git clone https://github.com/The-No-hands-Company/Nexus-Modeling.git
cd Nexus-Modeling

# Configure
cmake -B build \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DNEXUS_BACKEND_VULKAN=ON \
      -DNEXUS_ENABLE_TESTS=ON

# Build (parallel)
cmake --build build -j$(nproc)
```

### CMake options

| Option | Default | Description |
|---|---|---|
| `NEXUS_BACKEND_VULKAN` | `ON` | Build Vulkan backend |
| `NEXUS_BACKEND_NULL` | `ON` | Build headless Null backend (always keep ON for tests) |
| `NEXUS_ENABLE_TESTS` | `ON` | Build unit and integration tests |
| `NEXUS_ENABLE_PYTHON` | `OFF` | Build Python bindings |
| `NEXUS_ENABLE_OIDN` | `ON` | Intel Open Image Denoise (CPU denoiser fallback) |
| `NEXUS_ENABLE_DLSS` | `OFF` | NVIDIA DLSS 4 (requires DLSS SDK) |
| `NEXUS_ENABLE_XESS` | `OFF` | Intel XeSS (requires XeSS SDK) |
| `NEXUS_ENABLE_AFTERMATH` | `OFF` | NVIDIA Aftermath crash breadcrumbs |
| `CMAKE_BUILD_TYPE` | `RelWithDebInfo` | `Debug` / `Release` / `RelWithDebInfo` |

---

## Running tests

```bash
# Run all tests
ctest --test-dir build --output-on-failure

# Run only a specific test suite
ctest --test-dir build -R VulkanShaderCompiler --output-on-failure

# Verbose output
ctest --test-dir build -V
```

### Test suites

| Suite | Count | Notes |
|---|---|---|
| `Types.*` | 4 | Handle defaults, usage flag bitwise ops |
| `NullDeviceTest.*` | 6 | Null backend: buffer, texture, fence, cmd buf, wait-idle |
| `Camera.*` | 5 | Perspective, look-at, jitter, TAA tick |
| `SceneGraph.*` | 6 | Node hierarchy, frustum culling, traversal |
| `RenderContext.*` | 4 | Context creation, device/allocator/swapchain access |
| `VulkanShaderCompiler.*` | 2 | GLSL → SPIR-V compile, .spv file load |
| `VulkanPipeline.*` | 1 | Graphics pipeline from inline GLSL (skipped if no ICD) |

Tests that need a physical Vulkan ICD (GPU or software renderer) are **skip-safe** — they emit `[  SKIPPED ]` instead of failing in headless/CI environments.

### Running on headless CI without a GPU

```bash
# Option 1 — Software renderer (Mesa lavapipe)
sudo apt install mesa-vulkan-drivers
MESA_VK_DEVICE_SELECT=llvmpipe ctest --test-dir build --output-on-failure

# Option 2 — Force Null backend only (skips Vulkan tests)
ctest --test-dir build -R "Types|NullDevice|Camera|SceneGraph|RenderContext"
```

---

## Using the kernel API

A minimal C++ application linking against `nexus_gfx_core`:

### CMakeLists.txt

```cmake
find_package(NexusModeling REQUIRED)  # or add_subdirectory

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE nexus_gfx_core nexus_backend_vulkan)
```

### `main.cpp`

```cpp
#include <nexus/gfx/RenderContext.h>
#include <nexus/gfx/Device.h>
#include <nexus/gfx/CommandBuffer.h>

using namespace nexus::gfx;

int main() {
    // 1. Create context
    auto ctx = RenderContext::create({
        .preferredBackend = Backend::Vulkan,
        .validation       = ValidationLevel::Core,
    });

    IDevice& dev = ctx->device();

    // 2. Create resources
    ShaderHandle vert = dev.createShader({
        .sourcePath = "shaders/triangle.vert.glsl",
        .stage      = ShaderStage::Vertex,
    });
    ShaderHandle frag = dev.createShader({
        .sourcePath = "shaders/triangle.frag.glsl",
        .stage      = ShaderStage::Fragment,
    });

    PipelineHandle pipe = dev.createGraphicsPipeline({
        .vertexShader   = vert,
        .fragmentShader = frag,
        .topology       = Topology::TriangleList,
        .depthTest      = true,
    });

    // 3. Record and submit commands
    CmdBufHandle cmd = dev.allocateCommandBuffer();
    ICommandBuffer& cb = *dev.getCommandBuffer(cmd);

    cb.begin();
    cb.bindPipeline(pipe);
    cb.draw(3);  // full-screen triangle
    cb.end();

    FenceHandle fence = dev.createFence();
    dev.submit(cmd, QueueType::Graphics, fence);
    dev.waitForFence(fence);

    // 4. Cleanup
    dev.freeCommandBuffer(cmd);
    dev.destroyFence(fence);
    dev.destroyPipeline(pipe);
    dev.destroyShader(vert);
    dev.destroyShader(frag);
}
```

---

## Shader files

Place GLSL shaders anywhere reachable at runtime and pass the path to `ShaderDesc::sourcePath`. The kernel dispatches based on extension:

| Extension | Action |
|---|---|
| `.spv` | Loaded as raw SPIR-V — no compilation |
| `.glsl` | Compiled at runtime via glslang |
| `.vert`, `.frag`, `.comp` | Same as `.glsl` — compiled |

For pre-compilation (recommended in release), use `glslangValidator` or `glslc`:

```bash
glslangValidator -V triangle.vert.glsl -o triangle.vert.spv
glslc -fshader-stage=vert triangle.vert.glsl -o triangle.vert.spv
```

Then supply the `.spv` path — zero compile cost at runtime.

---

## Debug tools

| Tool | How to enable |
|---|---|
| Vulkan validation layers | `ValidationLevel::Core` or `Full` in `RenderContextDesc` |
| RenderDoc | Attach to process — works transparently with dynamic rendering |
| NVIDIA Nsight | Attach to process |
| GPU crash breadcrumbs | `NEXUS_ENABLE_AFTERMATH=ON` + `ValidationLevel::Aftermath` |
| Debug labels | `cb.beginDebugLabel("GBuffer Pass")` / `cb.endDebugLabel()` |
