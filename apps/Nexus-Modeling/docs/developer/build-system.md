# Nexus Modeling — Build System

*Complete CMake configuration, dependency management, and cross-platform build guide.*

---

## Requirements

| Tool | Minimum Version | Recommended |
|------|----------------|-------------|
| CMake | 3.28 | 4.3.4 |
| C++ Compiler | GCC 13 / Clang 17 / MSVC 19.40 | Latest |
| Ninja | 1.11 | Latest |
| Vulkan SDK | 1.3.250 | Latest |
| Python | 3.10 | 3.12 (for scripts) |

### Linux (Fedora/RHEL/CentOS)

```bash
sudo dnf install cmake gcc-c++ ninja-build vulkan-devel glfw-devel \
    vulkan-memory-allocator-devel glslang-devel spirv-tools-devel \
    python3-devel
```

### Linux (Ubuntu/Debian)

```bash
sudo apt update && sudo apt install -y \
    cmake g++ ninja-build libvulkan-dev libglfw3-dev \
    libvma-dev glslang-dev spirv-tools-dev \
    python3-dev
```

### Windows (MSVC)

```powershell
# Install via Chocolatey
choco install cmake ninja vulkansdk glfw

# Or vcpkg
vcpkg install vulkan glfw3 vma glslang spirv-tools
```

### macOS

```bash
brew install cmake ninja vulkan-sdk glfw vulkan-memory-allocator glslang spirv-tools
```

---

## Quick Start

```bash
# Clone
git clone https://github.com/nexus-systems/nexus-modeling.git
cd nexus-modeling/apps/Nexus-Modeling

# Configure (first time)
cmake -S . -B build -G Ninja \
    -DNEXUS_BACKEND_VULKAN=ON \
    -DNEXUS_BACKEND_NULL=ON \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Build
cmake --build build -j$(nproc)

# Test
ctest --test-dir build --output-on-failure

# Run app
./build/src/kernel/nexus_modeling
```

---

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `NEXUS_BACKEND_VULKAN` | ON | Build Vulkan backend |
| `NEXUS_BACKEND_NULL` | ON | Build Null (headless) backend |
| `NEXUS_BUILD_TESTS` | ON | Build test suite |
| `NEXUS_BUILD_TOOLS` | ON | Build CLI tools |
| `NEXUS_ENABLE_ASAN` | OFF (Debug) | AddressSanitizer |
| `NEXUS_ENABLE_UBSAN` | OFF (Debug) | UndefinedBehaviorSanitizer |
| `NEXUS_ENABLE_TSAN` | OFF | ThreadSanitizer |
| `NEXUS_ENABLE_MSAN` | OFF | MemorySanitizer (Clang only) |
| `NEXUS_ENABLE_COVERAGE` | OFF | Code coverage (gcov/llvm-cov) |
| `NEXUS_API_FREEZE` | ON | Enforce API freeze audit |
| `CMAKE_CXX_STANDARD` | 26 | C++ standard version |

### Example Configurations

**Release Optimized:**
```bash
cmake -S . -B build_release -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DNEXUS_BACKEND_VULKAN=ON \
    -DNEXUS_BACKEND_NULL=ON \
    -DNEXUS_BUILD_TESTS=OFF
```

**Debug with Sanitizers:**
```bash
cmake -S . -B build_asan -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DNEXUS_ENABLE_ASAN=ON \
    -DNEXUS_ENABLE_UBSAN=ON
```

**Coverage:**
```bash
cmake -S . -B build_cov -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DNEXUS_ENABLE_COVERAGE=ON
```

---

## Project Structure

```
nexus-modeling/
├── CMakeLists.txt                 # Root
├── cmake/
│   ├── FindVulkan.cmake
│   ├── FindVMA.cmake
│   ├── FindGLFW.cmake
│   ├── FindGLSLang.cmake
│   ├── FindSPIRVTools.cmake
│   └── FindSPIRVCross.cmake
├── src/
│   └── kernel/
│       ├── CMakeLists.txt         # Kernel library
│       ├── include/nexus/         # Public headers
│       │   ├── geometry/
│       │   ├── cad/
│       │   ├── render/
│       │   ├── gfx/
│       │   ├── app/
│       │   ├── animation/
│       │   ├── sim/
│       │   ├── sculpt/
│       │   ├── parametric/
│       │   └── Kernel.h
│       └── src/                   # Private implementation
│           ├── geometry/
│           ├── cad/
│           ├── render/
│           ├── gfx/
│           │   └── vulkan/
│           ├── app/
│           ├── animation/
│           ├── sim/
│           ├── sculpt/
│           └── parametric/
├── app/
│   └── CMakeLists.txt             # nexus_modeling executable
│   └── main.cpp
├── tests/
│   └── CMakeLists.txt             # Test suite
├── tools/
│   └── CMakeLists.txt             # CLI tools
└── docs/                          # Documentation
```

---

## Kernel Library (`src/kernel/CMakeLists.txt`)

```cmake
# Core target: nexus_gfx_core (STATIC)
add_library(nexus_gfx_core STATIC)

# C++26
target_compile_features(nexus_gfx_core PRIVATE cxx_std_26)

# Include directories
target_include_directories(nexus_gfx_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include/nexus>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Public dependencies
target_link_libraries(nexus_gfx_core PUBLIC
    eigen3::eigen
    fmt::fmt
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    xxhash::xxhash
    Vulkan::Vulkan
    glfw
    VulkanMemoryAllocator::VulkanMemoryAllocator
    glslang::glslang
    SPIRV::SPIRV-Tools
    SPIRV::SPIRV-Cross
)

# Private dependencies
target_link_libraries(nexus_gfx_core PRIVATE
    # Internal only
)

# Sources (alphabetical within module)
set(CORE_SOURCES
    src/core/Assert.cpp
    src/core/Log.cpp
    src/core/Profiler.cpp
    src/core/UUID.cpp
)

set(GEOMETRY_SOURCES
    src/geometry/Aabb.cpp
    src/geometry/AnalyticBRep.cpp
    src/geometry/BooleanOperation.cpp
    src/geometry/ConstrainedDelaunay2D.cpp
    src/geometry/Delaunay2D.cpp
    src/geometry/HalfEdgeMesh.cpp
    src/geometry/Mesh.cpp
    src/geometry/MeshAttributes.cpp
    src/geometry/MeshBVH.cpp
    src/geometry/MeshBoolean.cpp
    src/geometry/MeshDecimator.cpp
    src/geometry/MeshIO.cpp
    src/geometry/MeshLaplacian.cpp
    src/geometry/MeshRemesh.cpp
    src/geometry/MeshTopology.cpp
    src/geometry/Obb.cpp
    src/geometry/Plane.cpp
    src/geometry/Primitives.cpp
    src/geometry/Ray.cpp
    src/geometry/RobustPredicates.cpp
    src/geometry/Segment.cpp
    src/geometry/SubdivisionSurface.cpp
    src/geometry/TetDelaunay3D.cpp
    src/geometry/Tolerance.cpp
    src/geometry/Triangle.cpp
    src/geometry/Vec2.cpp
    src/geometry/Vec3.cpp
)

set(CAD_SOURCES
    src/cad/CadAutoConstraintSketch.cpp
    src/cad/CadCommand.cpp
    src/cad/CadDocument.cpp
    src/cad/CadExport.cpp
    src/cad/CadFeature.cpp
    src/cad/CadFeatureFactory.cpp
    src/cad/CadHistory.cpp
    src/cad/CadImport.cpp
    src/cad/CadSelection.cpp
    src/cad/CadSketch.cpp
    src/cad/CadSolver.cpp
    src/cad/CadViewer.cpp
)

set(RENDER_SOURCES
    src/render/Camera.cpp
    src/render/Environment.cpp
    src/render/Frustum.cpp
    src/render/Light.cpp
    src/render/Material.cpp
    src/render/Mesh.cpp
    src/render/Picking.cpp
    src/render/Renderer.cpp
    src/render/SceneGraph.cpp
    src/render/SceneNode.cpp
    src/render/Viewport.cpp
)

set(GFX_SOURCES
    src/gfx/Buffer.cpp
    src/gfx/CommandBuffer.cpp
    src/gfx/DescriptorSet.cpp
    src/gfx/Device.cpp
    src/gfx/FrameScheduler.cpp
    src/gfx/Pipeline.cpp
    src/gfx/RenderContext.cpp
    src/gfx/Renderer.cpp
    src/gfx/Shader.cpp
    src/gfx/Swapchain.cpp
    src/gfx/Texture.cpp
    src/gfx/Types.cpp
    src/gfx/FrameCapture.cpp
    src/gfx/RenderGraphValidator.cpp
    src/gfx/vulkan/VulkanAllocator.cpp
    src/gfx/vulkan/VulkanBuffer.cpp
    src/gfx/vulkan/VulkanCommandBuffer.cpp
    src/gfx/vulkan/VulkanDescriptorSet.cpp
    src/gfx/vulkan/VulkanDevice.cpp
    src/gfx/vulkan/VulkanFrameScheduler.cpp
    src/gfx/vulkan/VulkanPipeline.cpp
    src/gfx/vulkan/VulkanShader.cpp
    src/gfx/vulkan/VulkanSwapchain.cpp
    src/gfx/vulkan/VulkanTexture.cpp
    src/gfx/null/NullDevice.cpp
    src/gfx/null/NullCommandBuffer.cpp
    src/gfx/null/NullSwapchain.cpp
    src/gfx/null/NullFrameScheduler.cpp
)

set(APP_SOURCES
    src/app/AppContext.cpp
    src/app/AppMode.cpp
    src/app/AssetBrowser.cpp
    src/app/CommandDispatcher.cpp
    src/app/EditorUI.cpp
    src/app/ModeOrchestrator.cpp
    src/app/ModeRegistry.cpp
    src/app/SelectionManager.cpp
    src/app/TransformGizmo.cpp
    src/app/ViewportAxes.cpp
    src/app/ViewportController.cpp
    src/app/ViewportGrid.cpp
    src/app/WorkspaceManager.cpp
)

target_sources(nexus_gfx_core PRIVATE
    ${CORE_SOURCES}
    ${GEOMETRY_SOURCES}
    ${CAD_SOURCES}
    ${RENDER_SOURCES}
    ${GFX_SOURCES}
    ${APP_SOURCES}
)

# Compile definitions
target_compile_definitions(nexus_gfx_core PRIVATE
    NEXUS_KERNEL_BUILD
    NEXUS_BACKEND_VULKAN=$<BOOL:${NEXUS_BACKEND_VULKAN}>
    NEXUS_BACKEND_NULL=$<BOOL:${NEXUS_BACKEND_NULL}>
)

# API Freeze: public header manifest
if(NEXUS_API_FREEZE)
    add_custom_target(ApiFreezeAudit
        COMMAND ${CMAKE_COMMAND} -E echo "Checking API surface..."
        COMMAND python3 ${CMAKE_SOURCE_DIR}/tools/api_freeze_check.py
            --manifest ${CMAKE_SOURCE_DIR}/tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt
            --include-dir ${CMAKE_CURRENT_SOURCE_DIR}/include
        DEPENDS nexus_gfx_core
    )
    add_dependencies(nexus_gfx_core ApiFreezeAudit)
endif()
```

---

## Application Executable (`app/CMakeLists.txt`)

```cmake
add_executable(nexus_modeling main.cpp)

target_compile_features(nexus_modeling PRIVATE cxx_std_26)

target_link_libraries(nexus_modeling PRIVATE
    nexus_gfx_core
)

# Resources (shaders, fonts)
target_include_directories(nexus_modeling PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
)

# Copy resources to build dir
add_custom_command(TARGET nexus_modeling POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders
        $<TARGET_FILE_DIR:nexus_modeling>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/resources/fonts
        $<TARGET_FILE_DIR:nexus_modeling>/fonts
)
```

---

## Tests (`tests/CMakeLists.txt`)

```cmake
if(NOT NEXUS_BUILD_TESTS)
    return()
endif()

enable_testing()

# Kernel tests
add_executable(nexus_kernel_tests
    kernel/test_AnalyticBRep.cpp
    kernel/test_BooleanOperation.cpp
    kernel/test_HalfEdgeMesh.cpp
    kernel/test_MeshBoolean.cpp
    kernel/test_MeshDecimator.cpp
    kernel/test_MeshLaplacian.cpp
    kernel/test_MeshRemesh.cpp
    kernel/test_SubdivisionSurface.cpp
    kernel/test_Delaunay2D.cpp
    kernel/test_TetDelaunay3D.cpp
    kernel/test_RobustPredicates.cpp
    kernel/test_Tolerance.cpp
    kernel/test_MeshIO.cpp
    kernel/test_Primitives.cpp
    kernel/test_SceneGraph.cpp
    kernel/test_Camera.cpp
    kernel/test_Frustum.cpp
    kernel/test_Picking.cpp
    kernel/test_Transform.cpp
    kernel/test_Quaternion.cpp
    kernel/test_Mat4.cpp
    kernel/test_Vec3.cpp
    kernel/test_CadDocument.cpp
    kernel/test_CadFeature.cpp
    kernel/test_CadSketch.cpp
    kernel/test_CadSolver.cpp
    kernel/test_ConstraintSolver.cpp
    kernel/test_Animation.cpp
    kernel/test_Simulation.cpp
    kernel/test_Sculpt.cpp
    kernel/test_Parameterization.cpp
    kernel/test_Integrity.cpp
    kernel/test_ApiFreeze.cpp
    kernel/test_PropertyBased.cpp
)

target_compile_features(nexus_kernel_tests PRIVATE cxx_std_26)
target_link_libraries(nexus_kernel_tests PRIVATE
    nexus_gfx_core
    gtest_main
    gmock_main
)

# Test fixtures
target_include_directories(nexus_kernel_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/kernel/fixtures
)

# Integration tests
add_executable(nexus_integration_tests
    integration/test_RenderPipeline.cpp
    integration/test_HeadlessRender.cpp
    integration/test_ViewportPicking.cpp
)

target_link_libraries(nexus_integration_tests PRIVATE
    nexus_gfx_core
    gtest_main
)

# Performance tests
add_executable(nexus_kernel_perf_smoke
    perf/test_BooleanPerf.cpp
    perf/test_ExtrudePerf.cpp
    perf/test_DecimatePerf.cpp
    perf/test_SubdivisionPerf.cpp
)

target_link_libraries(nexus_kernel_perf_smoke PRIVATE
    nexus_gfx_core
    benchmark::benchmark
)

# Register with CTest
add_test(NAME nexus_kernel_tests COMMAND nexus_kernel_tests)
add_test(NAME nexus_integration_tests COMMAND nexus_integration_tests)
add_test(NAME nexus_kernel_perf_smoke COMMAND nexus_kernel_perf_smoke)

# Test properties
set_tests_properties(nexus_kernel_tests PROPERTIES
    TIMEOUT 300
    ENVIRONMENT "NEXUS_TESTS_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/kernel/fixtures"
)

set_tests_properties(nexus_integration_tests PROPERTIES
    TIMEOUT 60
)

# Run tests in parallel
set(CTEST_PARALLEL_LEVEL ${PROCESSOR_COUNT})
```

---

## Dependency Management

### Find Modules (cmake/Find*.cmake)

```cmake
# cmake/FindVulkan.cmake
find_package(Vulkan REQUIRED)
if(NOT Vulkan_FOUND)
    message(FATAL_ERROR "Vulkan SDK not found. Install Vulkan SDK.")
endif()

# cmake/FindVMA.cmake
find_path(VMA_INCLUDE_DIR vk_mem_alloc.h
    PATHS ${VULKAN_SDK}/include
)
find_library(VMA_LIBRARY vma
    PATHS ${VULKAN_SDK}/lib
)
if(VMA_INCLUDE_DIR AND VMA_LIBRARY)
    add_library(VulkanMemoryAllocator::VulkanMemoryAllocator INTERFACE IMPORTED)
    set_target_properties(VulkanMemoryAllocator::VulkanMemoryAllocator PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES ${VMA_INCLUDE_DIR}
        INTERFACE_LINK_LIBRARIES ${VMA_LIBRARY}
    )
endif()

# cmake/FindGLSLang.cmake
find_package(glslang REQUIRED)

# cmake/FindSPIRVTools.cmake
find_package(SPIRV-Tools REQUIRED)

# cmake/FindSPIRVCross.cmake
find_package(SPIRV-Cross REQUIRED)
```

### FetchContent (Header-only deps)

```cmake
# Eigen
FetchContent_Declare(eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
)

# nlohmann/json
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.2
)

# fmt
FetchContent_Declare(fmt
    GIT_REPOSITORY https://github.com/fmtlib/fmt.git
    GIT_TAG 10.2.1
)

# spdlog
FetchContent_Declare(spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.13.0
)

# googletest
FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# benchmark
FetchContent_Declare(benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)

# xxHash
FetchContent_Declare(xxhash
    GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
    GIT_TAG v0.8.2
)

FetchContent_MakeAvailable(
    eigen nlohmann_json fmt spdlog googletest benchmark xxhash
)
```

---

## Shader Compilation

### Offline (Build-time)

```cmake
# Compile GLSL → SPIR-V at build time
function(compile_shader TARGET NAME SRC STAGE)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/shaders/${NAME}.spv
        COMMAND glslangValidator
            -V -S ${STAGE}
            -o ${CMAKE_CURRENT_BINARY_DIR}/shaders/${NAME}.spv
            ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/${SRC}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/resources/shaders/${SRC}
        COMMENT "Compiling ${SRC} to SPIR-V"
        VERBATIM
    )
    target_sources(${TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/shaders/${NAME}.spv)
endfunction()

# Usage
compile_shader(nexus_gfx_core gbuffer_vert gbuffer.vert vert)
compile_shader(nexus_gfx_core gbuffer_frag gbuffer.frag frag)
compile_shader(nexus_gfx_core lighting_frag lighting.frag frag)
compile_shader(nexus_gfx_core shadow_vert shadow.vert vert)
```

### Runtime (Hot Reload)

```cpp
// Shader hot-reload in development
class ShaderManager {
    std::filesystem::file_time_type lastWriteTime;
    
    void checkReload() {
        auto current = std::filesystem::last_write_time(shaderPath);
        if (current > lastWriteTime) {
            recompile();
            lastWriteTime = current;
        }
    }
};
```

---

## Cross-Compilation

### Linux → Windows (MinGW)

```bash
cmake -S . -B build_win -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=cmake/mingw64.cmake \
    -DCMAKE_BUILD_TYPE=Release
```

```cmake
# cmake/mingw64.cmake
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
```

### Linux → macOS (osxcross)

```bash
cmake -S . -B build_mac -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=cmake/osxcross.cmake
```

### WebAssembly (Emscripten)

```bash
cmake -S . -B build_wasm -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=${EMSCRIPTEN}/cmake/Modules/Platform/Emscripten.cmake \
    -DNEXUS_BACKEND_VULKAN=OFF \
    -DNEXUS_BACKEND_NULL=ON
```

---

## Installation

```bash
# Install to system
cmake --install build --prefix /usr/local

# Or package
cpack -G DEB  # Debian package
cpack -G RPM  # RPM package
cpack -G TGZ  # Tarball
```

### CPack Config

```cmake
# CPackConfig.cmake
include(InstallRequiredSystemLibraries)

set(CPACK_PACKAGE_NAME "Nexus-Modeling")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Vulkan-first geometry kernel")
set(CPACK_PACKAGE_VENDOR "Nexus Systems")
set(CPACK_PACKAGE_CONTACT "team@nexus-systems.dev")

set(CPACK_GENERATOR "TGZ;DEB;RPM")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Nexus Team <team@nexus-systems.dev>")
set(CPACK_RPM_PACKAGE_LICENSE "Apache-2.0")

include(CPack)
```

---

## IDE Integration

### VS Code (`.vscode/settings.json`)

```json
{
    "cmake.configureSettings": {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo",
        "NEXUS_BACKEND_VULKAN": "ON",
        "NEXUS_BACKEND_NULL": "ON"
    },
    "cmake.buildArgs": ["-j", "16"],
    "C_Cpp.default.cppStandard": "c++26",
    "C_Cpp.default.compilerPath": "/usr/bin/g++-13",
    "files.associations": {
        "*.h": "cpp",
        "*.cpp": "cpp"
    }
}
```

### CLion

- Open as CMake project
- Set CMake options in Settings → Build → CMake
- Enable "Run configurations" for tests

---

## Troubleshooting

### Vulkan Not Found

```bash
# Check SDK
echo $VULKAN_SDK
ls $VULKAN_SDK/include/vulkan/vulkan.h

# Set manually
cmake -DVULKAN_SDK=/path/to/vulkan/sdk ...
```

### C++26 Not Supported

```bash
# Check compiler
g++ --version  # Need 13+
clang++ --version  # Need 17+

# Use newer compiler
export CC=gcc-13 CXX=g++-13
cmake ...
```

### Linker Errors (Missing Sources)

```bash
# Check CMakeLists.txt for missing .cpp files
# Every new .cpp MUST be added to appropriate SOURCES list
```

### Test Failures (Headless)

```bash
# Use xvfb-run for Vulkan tests without display
xvfb-run -a ctest --test-dir build --output-on-failure
```

---

*Kernel v0.1.0-dev | 2026 tests passing | C++26 | Vulkan 1.3*