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

target_compile_options(nexus_runtime_viewer PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -march=native
    -ffast-math
)

# Propagate definitions
if(NEXUS_BACKEND_VULKAN)
    target_compile_definitions(nexus_runtime_viewer PRIVATE NEXUS_BACKEND_VULKAN)
endif()
if(NEXUS_BACKEND_NULL)
    target_compile_definitions(nexus_runtime_viewer PRIVATE NEXUS_BACKEND_NULL)
endif()
