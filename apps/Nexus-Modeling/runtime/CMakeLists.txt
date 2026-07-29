# ─────────────────────────────────────────────────────────────────────────────
#  Nexus Runtime Viewer
# ─────────────────────────────────────────────────────────────────────────────

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED glfw3)
pkg_check_modules(GLU REQUIRED glu)
find_package(OpenGL REQUIRED)

add_executable(nexus_runtime_viewer
    main.cpp
)

target_include_directories(nexus_runtime_viewer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/kernel/include
    ${GLFW3_INCLUDE_DIRS}
    ${GLU_INCLUDE_DIRS}
)

target_link_libraries(nexus_runtime_viewer PRIVATE
    nexus_gfx_core
    ${GLFW3_LIBRARIES}
    ${GLU_LIBRARIES}
    OpenGL::GL
)

target_compile_features(nexus_runtime_viewer PRIVATE cxx_std_26)

# Same floating-point policy as the kernel (see src/kernel/CMakeLists.txt): no fast-math,
# no FP contraction, no host-specific ISA. This target compiles kernel headers — the
# constexpr Tolerance predicates and Vec3 math among them — so a laxer regime here would
# mean the same inline comparison answering differently than it does inside the kernel.
target_compile_options(nexus_runtime_viewer PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -ffp-contract=off
)
# Inherited from the root CMakeLists when built as part of the tree; detected here when
# this directory is configured on its own.
if(NOT DEFINED NEXUS_ARCH_BASELINE)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag("-march=x86-64-v2" NEXUS_HAS_X86_64_V2)
    if(NEXUS_HAS_X86_64_V2)
        set(NEXUS_ARCH_BASELINE "-march=x86-64-v2")
    endif()
endif()
if(NEXUS_ARCH_BASELINE)
    target_compile_options(nexus_runtime_viewer PRIVATE ${NEXUS_ARCH_BASELINE})
endif()

# Propagate definitions
if(NEXUS_BACKEND_VULKAN)
    target_compile_definitions(nexus_runtime_viewer PRIVATE NEXUS_BACKEND_VULKAN)
endif()
if(NEXUS_BACKEND_NULL)
    target_compile_definitions(nexus_runtime_viewer PRIVATE NEXUS_BACKEND_NULL)
endif()
