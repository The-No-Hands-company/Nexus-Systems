cmake_minimum_required(VERSION 3.28)

# ── Fetch GoogleTest ──────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)

# ── Test executable ───────────────────────────────────────────────────────────
add_executable(nexus_kernel_tests
    kernel/test_Types.cpp
    kernel/test_TypesExtended.cpp
    kernel/test_AnimationCore.cpp
    kernel/test_AnimationSerialization.cpp
    kernel/test_Mesh.cpp
        kernel/test_BooleanOperation.cpp
    kernel/test_BevelChamfer.cpp
    kernel/test_RemeshOperation.cpp
    kernel/test_MeshDiagnosticOverlay.cpp
    kernel/test_MeshIO.cpp
    kernel/test_ExtrudeOperation.cpp
    kernel/test_InsetFacesOperation.cpp
    kernel/test_SceneAsset.cpp
    kernel/test_SceneAssetImporter.cpp
    kernel/test_AssetDependencyGraph.cpp
    kernel/test_GeometryKernel.cpp
    kernel/test_TopologyGolden.cpp
    kernel/test_RenderPipelineV1.cpp
    kernel/test_ParametricFoundation.cpp
    kernel/test_NullDevice.cpp
    kernel/test_NullDeviceExtended.cpp
    kernel/test_Camera.cpp
    kernel/test_CameraExtended.cpp
    kernel/test_SceneGraph.cpp
    kernel/test_SceneGraphExtended.cpp
    kernel/test_RenderContext.cpp
    kernel/test_RenderContextExtended.cpp
    kernel/test_Renderer.cpp
    kernel/test_RendererBehavior.cpp
    kernel/test_NeuralRenderer.cpp
    kernel/test_EvalGraph.cpp
    kernel/test_SimulationCore.cpp
    kernel/test_GaussianSplatting.cpp
    kernel/test_AutomationScript.cpp
    kernel/test_ApiFreezeAudit.cpp
)

add_executable(nexus_kernel_perf_smoke
    kernel/perf_smoke.cpp
)

if(NEXUS_BACKEND_VULKAN)
    target_sources(nexus_kernel_tests PRIVATE
        kernel/test_VulkanShaderCompiler.cpp
        kernel/test_VulkanPipeline.cpp
        kernel/test_VulkanFrameScheduler.cpp
        kernel/test_VulkanRegression.cpp
    )
endif()

target_link_libraries(nexus_kernel_tests
    PRIVATE nexus_gfx_core nexus_backend_null nexus_neural GTest::gtest_main
    $<$<BOOL:${NEXUS_BACKEND_VULKAN}>:nexus_backend_vulkan>
)

target_compile_features(nexus_kernel_tests PRIVATE cxx_std_23)
target_compile_features(nexus_kernel_perf_smoke PRIVATE cxx_std_23)

target_compile_definitions(nexus_kernel_tests PRIVATE
    NEXUS_TESTS_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(nexus_kernel_perf_smoke
    PRIVATE nexus_gfx_core nexus_backend_null
    $<$<BOOL:${NEXUS_BACKEND_VULKAN}>:nexus_backend_vulkan>
)

gtest_discover_tests(nexus_kernel_tests)
add_test(NAME KernelPerfSmoke.Null COMMAND nexus_kernel_perf_smoke --frames 120)
add_test(NAME KernelPerfSmoke.Determinism COMMAND nexus_kernel_perf_smoke --frames 64 --determinism-runs 3)
