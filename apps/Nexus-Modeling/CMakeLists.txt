cmake_minimum_required(VERSION 3.28)
project(NexusModeling VERSION 0.1.0 LANGUAGES CXX C)

# ─────────────────────────────────────────────────────────────────────────────
#  Global C++ standard
# ─────────────────────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─────────────────────────────────────────────────────────────────────────────
#  Build type default
# ─────────────────────────────────────────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "Build type" FORCE)
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  Fast linker (developer iteration speed)
#
#  The unit-test binary links ~180 translation units against several static libs,
#  so the default BFD linker dominates the change-one-test → rebuild cycle. Prefer
#  mold, then lld, when present — a drop-in link-time win (often 3-8×) with no code
#  impact. Opt out with -DNEXUS_FAST_LINKER=OFF; override with -DNEXUS_LINKER=<name>.
# ─────────────────────────────────────────────────────────────────────────────
option(NEXUS_FAST_LINKER "Use mold/lld if available for faster linking" ON)
if(NEXUS_FAST_LINKER AND (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang") AND NOT WIN32)
    if(NOT DEFINED NEXUS_LINKER)
        find_program(NEXUS_MOLD_EXE mold)
        find_program(NEXUS_LLD_EXE ld.lld)
        if(NEXUS_MOLD_EXE)
            set(NEXUS_LINKER "mold")
        elseif(NEXUS_LLD_EXE)
            set(NEXUS_LINKER "lld")
        endif()
    endif()
    if(NEXUS_LINKER)
        add_link_options("-fuse-ld=${NEXUS_LINKER}")
        message(STATUS "Nexus: using '${NEXUS_LINKER}' linker for faster iteration")
    endif()
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  Options
# ─────────────────────────────────────────────────────────────────────────────
option(NEXUS_BACKEND_VULKAN   "Build Vulkan backend"          ON)
option(NEXUS_BACKEND_NULL     "Build null/headless backend"   ON)
option(NEXUS_ENABLE_TESTS     "Build unit tests"              ON)
option(NEXUS_ENABLE_PYTHON    "Build Python bindings"         OFF)
option(NEXUS_ENABLE_AFTERMATH "Enable NVIDIA Aftermath (crash resilience)" OFF)
option(NEXUS_ENABLE_DLSS      "Enable NVIDIA DLSS 4 integration"           OFF)
option(NEXUS_ENABLE_XESS      "Enable Intel XeSS integration"              OFF)
option(NEXUS_ENABLE_OIDN      "Enable Intel Open Image Denoise fallback"   ON)

# ─────────────────────────────────────────────────────────────────────────────
#  Module path
# ─────────────────────────────────────────────────────────────────────────────
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ─────────────────────────────────────────────────────────────────────────────
#  Platform detection
# ─────────────────────────────────────────────────────────────────────────────
if(WIN32)
    add_compile_definitions(NEXUS_PLATFORM_WINDOWS)
    option(NEXUS_BACKEND_DX12  "Build DirectX 12 backend"  ON)
    option(NEXUS_DIRECTSTORAGE "Enable DirectStorage"      ON)
elseif(APPLE)
    add_compile_definitions(NEXUS_PLATFORM_MACOS)
    option(NEXUS_BACKEND_METAL "Build Metal backend"       ON)
elseif(UNIX)
    add_compile_definitions(NEXUS_PLATFORM_LINUX)
endif()

# ─────────────────────────────────────────────────────────────────────────────
#  Subdirectories
# ─────────────────────────────────────────────────────────────────────────────

# ── Vulkan Memory Allocator ───────────────────────────────────────────────────
if(NEXUS_BACKEND_VULKAN)
    include(FetchContent)
    FetchContent_Declare(
        VulkanMemoryAllocator
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG        v3.1.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(VulkanMemoryAllocator)

    # GLSL -> SPIR-V compiler toolchain
    set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
    set(ENABLE_SPVREMAPPER      OFF CACHE BOOL "" FORCE)
    set(ENABLE_HLSL             OFF CACHE BOOL "" FORCE)
    set(ENABLE_OPT              OFF CACHE BOOL "" FORCE)
    set(BUILD_EXTERNAL          OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        glslang
        URL https://github.com/KhronosGroup/glslang/archive/refs/tags/15.1.0.tar.gz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(glslang)
endif()

# ── ImGui (editor UI) ─────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6-docking   # docking branch, same version — dockable panels
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(imgui)

add_subdirectory(src/kernel)

if(NEXUS_ENABLE_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
