# ─────────────────────────────────────────────────────────────────────────────
#  Nexus Graphics Kernel
# ─────────────────────────────────────────────────────────────────────────────

# ── Core abstraction library ──────────────────────────────────────────────────
add_library(nexus_gfx_core STATIC
    src/gfx/RenderContext.cpp
    src/gfx/Device.cpp
    src/memory/GPUAllocator.cpp
    src/animation/AnimationBlend.cpp
    src/animation/AnimationCore.cpp
    src/animation/AnimationSerialization.cpp
    src/animation/InverseKinematics.cpp
    src/animation/SkeletonRetargeter.cpp
    src/animation/VideoEditor.cpp
    src/automation/AutomationScriptCommands.cpp
    src/automation/AutomationScriptUtilities.cpp
    src/cad/CadAssembly.cpp
    src/cad/CadAutoConstraintSketch.cpp
    src/cad/CadCommand.cpp
    src/cad/CadDesignRule.cpp
    src/cad/CadDimension.cpp
    src/cad/CadDocument.cpp
    src/cad/CadDrawing.cpp
    src/cad/CadExpression.cpp
    src/cad/CadFeatureTree.cpp
    src/cad/CadInfrastructure.cpp
    src/cad/CadModeling.cpp
    src/cad/CadOperations.cpp
    src/cad/CadRenderBridge.cpp
    src/cad/CadSelection.cpp
    src/cad/CadViewer.cpp
    src/geometry/BevelChamfer.cpp
    src/geometry/BezierCurve.cpp
    src/geometry/BlendNetwork.cpp
    src/geometry/AnalyticBRep.cpp
    src/geometry/BRepSurfaceIntersect.cpp
    src/geometry/BRepBody.cpp
    src/geometry/BooleanOperation.cpp
    src/geometry/ConstrainedDelaunay2D.cpp
    src/geometry/CurveCurveIntersect.cpp
    src/geometry/CurveExtrude.cpp
    src/geometry/CurveFairing.cpp
    src/geometry/Delaunay2D.cpp
    src/geometry/DirectModeling.cpp
    src/geometry/EdgeBridge.cpp
    src/geometry/EvaluationGraph.cpp
    src/geometry/ExtrudeOperation.cpp
    src/geometry/FaceFillet.cpp
    src/geometry/FeatureLineExtractor.cpp
    src/geometry/FeatureRecognition.cpp
    src/geometry/GeodesicPath.cpp
    src/geometry/GeometryKernel.cpp
    src/geometry/GordonSurface.cpp
    src/geometry/HalfEdgeMesh.cpp
    src/geometry/HardSurfaceWorkflow.cpp
    src/geometry/InsetFacesOperation.cpp
    src/geometry/KnifeTool.cpp
    src/geometry/Mesh.cpp
    src/geometry/MeshAmbientOcclusion.cpp
    src/geometry/MeshAnalysis.cpp
    src/geometry/MeshBVH.cpp
    src/geometry/MeshBoolean.cpp
    src/geometry/MeshBoundaryLoop.cpp
    src/geometry/MeshCageDeform.cpp
    src/geometry/MeshClosestPoint.cpp
    src/geometry/MeshConvexHull.cpp
    src/geometry/MeshCrossSection.cpp
    src/geometry/MeshDecimator.cpp
    src/geometry/MeshDiagnosticOverlay.cpp
    src/geometry/MeshDisplace.cpp
    src/geometry/MeshFillet.cpp
    src/geometry/MeshGeodesicDistance.cpp
    src/geometry/MeshHarmonicField.cpp
    src/geometry/MeshHausdorffDistance.cpp
    src/geometry/MeshIO.cpp
    src/geometry/MeshInstancer.cpp
    src/geometry/MeshIslandDecomposition.cpp
    src/geometry/MeshLaplacian.cpp
    src/geometry/MeshMassProperties.cpp
    src/geometry/MeshMorphTarget.cpp
    src/geometry/MeshNormals.cpp
    src/geometry/MeshPointSample.cpp
    src/geometry/MeshPoissonDisk.cpp
    src/geometry/MeshProcessing.cpp
    src/geometry/MeshRepair.cpp
    src/geometry/MeshSelfIntersection.cpp
    src/geometry/MeshSignedDistanceField.cpp
    src/geometry/MeshSimplify.cpp
    src/geometry/MeshSkinning.cpp
    src/geometry/MeshTopologyValidation.cpp
    src/geometry/MeshSurfaceNets.cpp
    src/geometry/MeshTangentSpace.cpp
    src/geometry/MeshThicken.cpp
    src/geometry/MeshTopologyAnalysis.cpp
    src/geometry/MeshTransform.cpp
    src/geometry/ModifierStack.cpp
    src/geometry/MeshUVUnwrap.cpp
    src/geometry/MeshVertexWeld.cpp
    src/geometry/MeshVoxelize.cpp
    src/geometry/MeshWindingNumber.cpp
    src/geometry/Meshlet.cpp
    src/geometry/ModelingShell.cpp
    src/geometry/NexusFormat.cpp
    src/geometry/NurbsCurve.cpp
    src/geometry/NurbsCurveArcLength.cpp
    src/geometry/NurbsCurveClosestPoint.cpp
    src/geometry/NurbsCurveFit.cpp
    src/geometry/NurbsCurveOffset.cpp
    src/geometry/NurbsCurveSplit.cpp
    src/geometry/NurbsIntersection.cpp
    src/geometry/NurbsKnotRefinement.cpp
    src/geometry/NurbsSurface.cpp
    src/geometry/NurbsSurfaceArea.cpp
    src/geometry/NurbsSurfaceClosestPoint.cpp
    src/geometry/NurbsSurfaceFit.cpp
    src/geometry/NurbsSurfaceOffset.cpp
    src/geometry/NurbsSurfaceSplit.cpp
    src/geometry/NurbsToMesh.cpp
    src/geometry/PointCloudNormals.cpp
    src/geometry/QuadRemesh.cpp
    src/geometry/RailLoft.cpp
    src/geometry/RemeshOperation.cpp
    src/geometry/RobustPredicates.cpp
    src/geometry/TriTriIntersect.cpp
    src/geometry/TriangleRetriangulate.cpp
    src/geometry/MeshCut.cpp
    src/geometry/MeshBooleanRobust.cpp
    src/geometry/SnappingEngine.cpp
    src/geometry/SectionTool.cpp
    src/geometry/SolidOperations.cpp
    src/geometry/SubdivisionSurface.cpp
    src/geometry/SurfaceBlending.cpp
    src/geometry/SurfaceContinuity.cpp
    src/geometry/SurfaceCurvature.cpp
    src/geometry/SurfaceCurveProjection.cpp
    src/geometry/SurfaceIntegration.cpp
    src/geometry/SurfaceOffset.cpp
    src/geometry/SurfacePatchNetwork.cpp
    src/geometry/SurfacePrimitives.cpp
    src/geometry/SurfaceSurfaceIntersect.cpp
    src/geometry/SurfaceTrim.cpp
    src/geometry/SweepLoft.cpp
    src/geometry/ProfileRevolve.cpp
    src/geometry/ProfileTool.cpp
    src/geometry/TrimBoolean.cpp
    src/geometry/TetDelaunay3D.cpp
    src/geometry/TransformOrientation.cpp
    src/geometry/Voronoi2D.cpp
    src/asset/SceneAsset.cpp
    src/asset/SceneAssetImporter.cpp
    src/asset/SceneAssetTextAdapter.cpp
    src/asset/AssetDependencyGraph.cpp
    src/material/Material.cpp
    src/procedural/Noise3D.cpp
    src/sculpt/Brush.cpp
    src/sculpt/SculptSession.cpp
    src/softrast/SoftwareRasterizer.cpp
    src/render/Camera.cpp
    src/render/SceneGraph.cpp
    src/render/Renderer.cpp
    src/render/RenderGraphValidator.cpp
    src/render/FrameCaptureExporter.cpp
    src/render/ReflectionCapture.cpp
    src/parametric/ConstraintGraph.cpp
    src/parametric/ParametricSolver.cpp
    src/parametric/ParametricSerialization.cpp
    src/parametric/ParametricSamples.cpp
    src/parametric/ParametricSketchProfile.cpp
    src/parametric/ParametricFeature.cpp
    src/parametric/FeatureHistory.cpp
    src/parametric/ParametricSceneBridge.cpp
    src/eval/EvalGraph.cpp
    src/scene/NodeScene.cpp
    src/sim/SimulationCore.cpp
    src/sim/FluidSolver.cpp
    src/sim/ClothSolver.cpp
    src/sim/SimulationCoupling.cpp
    src/sim/SimulationDriver.cpp
    src/gfx/GaussianSplatting.cpp
    src/groom/GroomSystem.cpp
    src/render/TemporalAccumulation.cpp
    src/automation/AutomationScript.cpp
    src/template/TemplateManager.cpp
)

target_include_directories(nexus_gfx_core
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
)

target_compile_features(nexus_gfx_core PUBLIC cxx_std_26)

# Propagate backend selection into Device.cpp (which lives in nexus_gfx_core)
if(NEXUS_BACKEND_VULKAN)
    target_compile_definitions(nexus_gfx_core PUBLIC NEXUS_BACKEND_VULKAN)
endif()
if(NEXUS_BACKEND_NULL)
    target_compile_definitions(nexus_gfx_core PUBLIC NEXUS_BACKEND_NULL)
endif()

# Aggressive optimisation flags per-platform
if(MSVC)
    target_compile_options(nexus_gfx_core PRIVATE /W4 /WX /O2 /arch:AVX2)
else()
    target_compile_options(nexus_gfx_core PRIVATE
        -Wall -Wextra -Wpedantic -Werror
        -march=native          # host-side frequency-bound code benefits from native tuning
        -ffast-math            # safe for non-IEEE geometry math
    )
endif()

# ── Vulkan backend ────────────────────────────────────────────────────────────
if(NEXUS_BACKEND_VULKAN)
    find_package(Vulkan REQUIRED)

    add_library(nexus_backend_vulkan STATIC
        src/backend/vulkan/VulkanDevice.cpp
        src/backend/vulkan/VulkanSwapchain.cpp
        src/backend/vulkan/VulkanBuffer.cpp
        src/backend/vulkan/VulkanTexture.cpp
        src/backend/vulkan/VulkanShader.cpp
        src/backend/vulkan/VulkanPipeline.cpp
        src/backend/vulkan/VulkanCommandBuffer.cpp
        src/backend/vulkan/VulkanFrameScheduler.cpp
        src/backend/vulkan/VulkanQueue.cpp
        src/backend/vulkan/VulkanSync.cpp
        src/backend/vulkan/VulkanAllocator.cpp
        src/backend/vulkan/VulkanRayTracing.cpp
        src/backend/vulkan/VulkanMeshShader.cpp
    )

    target_include_directories(nexus_backend_vulkan
        PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

    target_link_libraries(nexus_backend_vulkan
        PUBLIC nexus_gfx_core Vulkan::Vulkan GPUOpen::VulkanMemoryAllocator
               glslang::glslang glslang::SPIRV glslang-default-resource-limits
    )

    target_compile_definitions(nexus_backend_vulkan PUBLIC NEXUS_BACKEND_VULKAN)
    # Request Vulkan 1.4
    target_compile_definitions(nexus_backend_vulkan PUBLIC VK_API_VERSION=VK_MAKE_API_VERSION\(0,1,4,0\))
endif()

# ── Null / headless backend ───────────────────────────────────────────────────
if(NEXUS_BACKEND_NULL)
    add_library(nexus_backend_null INTERFACE)

    target_include_directories(nexus_backend_null
        INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
    )

    target_link_libraries(nexus_backend_null INTERFACE nexus_gfx_core)
    target_compile_definitions(nexus_backend_null INTERFACE NEXUS_BACKEND_NULL)
endif()

# ── Neural rendering module (DLSS4 / XeSS / OIDN) ────────────────────────────
add_library(nexus_neural STATIC
    src/neural/OIDNDenoiser.cpp
    src/neural/DLSSPlugin.cpp
    src/neural/XeSSPlugin.cpp
    src/neural/NeuralRenderer.cpp
)

target_include_directories(nexus_neural
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(nexus_neural PUBLIC nexus_gfx_core)

if(NEXUS_BACKEND_VULKAN)
    target_link_libraries(nexus_neural PRIVATE nexus_backend_vulkan)
endif()

# ── NVIDIA Aftermath (GPU crash resilience) ───────────────────────────────────
if(NEXUS_ENABLE_AFTERMATH)
    find_package(Aftermath REQUIRED)
    target_compile_definitions(nexus_gfx_core PUBLIC NEXUS_ENABLE_AFTERMATH)
    target_link_libraries(nexus_gfx_core PUBLIC Aftermath::Aftermath)
endif()

# ── Intel Open Image Denoise (CPU fallback denoiser) ─────────────────────────
if(NEXUS_ENABLE_OIDN)
    find_package(OpenImageDenoise QUIET)
    if(OpenImageDenoise_FOUND)
        target_compile_definitions(nexus_neural PUBLIC NEXUS_ENABLE_OIDN)
        target_link_libraries(nexus_neural PUBLIC OpenImageDenoise)
    else()
        message(WARNING "OpenImageDenoise not found — OIDN denoiser disabled")
    endif()
endif()

# ── DLSS 4 (runtime plugin — no link-time dep) ───────────────────────────────
if(NEXUS_ENABLE_DLSS)
    target_compile_definitions(nexus_neural PUBLIC NEXUS_ENABLE_DLSS)
endif()

# ── XeSS (runtime plugin — no link-time dep) ─────────────────────────────────
if(NEXUS_ENABLE_XESS)
    target_compile_definitions(nexus_neural PUBLIC NEXUS_ENABLE_XESS)
endif()

# ── Nexus Modeling application executable ────────────────────────────────────
find_package(PkgConfig)
pkg_check_modules(GLFW3 REQUIRED glfw3)
pkg_check_modules(GLU REQUIRED glu)
find_package(OpenGL REQUIRED)

# ── Editor / application layer (ImGui-based UI) ───────────────────────────────
#  Split out of nexus_gfx_core so the geometry/render/sim kernel — and the
#  headless tools (nxrender) and tests that link only the kernel — build
#  independently of the editor UI. Dependency flows one way: app -> kernel.
add_library(nexus_app STATIC
    src/app/AppMode.cpp
    src/app/BooleanMode.cpp
    src/app/DimensionMode.cpp
    src/app/EdgeEditMode.cpp
    src/app/ExtrudeMode.cpp
    src/app/FaceEditMode.cpp
    src/app/FilletMode.cpp
    src/app/MirrorMode.cpp
    src/app/ModelingApplication.cpp
    src/app/PatternMode.cpp
    src/app/PrimitiveModelingMode.cpp
    src/app/RevolveMode.cpp
    src/app/SceneRenderer.cpp
    src/app/SelectMode.cpp
    src/app/SelectionManager.cpp
    src/app/SelectionHighlight.cpp
    src/app/SketchMode.cpp
    src/app/SketchPreview.cpp
    src/app/TransformGizmo.cpp
    src/app/Viewport.cpp
    src/app/VertexEditMode.cpp
    src/app/ViewportManager.cpp
    src/app/ViewportGrid.cpp
)
target_include_directories(nexus_app
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
            ${GLFW3_INCLUDE_DIRS}
)
target_link_libraries(nexus_app PUBLIC nexus_gfx_core)
if(NEXUS_BACKEND_VULKAN)
    # Viewport creates the window surface via glfwCreateWindowSurface, which needs
    # the Vulkan + GLFW-Vulkan headers (link is resolved by the app exe).
    target_link_libraries(nexus_app PRIVATE Vulkan::Vulkan)
endif()
target_compile_features(nexus_app PUBLIC cxx_std_26)
if(MSVC)
    target_compile_options(nexus_app PRIVATE /W4 /WX /O2 /arch:AVX2)
else()
    target_compile_options(nexus_app PRIVATE
        -Wall -Wextra -Wpedantic -Werror -march=native -ffast-math)
endif()

add_executable(nexus_modeling
    ../../app/main.cpp
)
target_include_directories(nexus_modeling PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${GLFW3_INCLUDE_DIRS}
    ${GLU_INCLUDE_DIRS}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)
target_sources(nexus_modeling PRIVATE
    # EditorUI is the only ImGui-consuming app file and is currently mid-refactor;
    # it is built directly into the app exe (not nexus_app) so the mode-system lib
    # and the kernel tests that link it stay green while the editor is finished.
    src/app/EditorUI.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
)
target_link_libraries(nexus_modeling PRIVATE
    nexus_app
    nexus_gfx_core
    ${GLFW3_LIBRARIES}
    ${GLU_LIBRARIES}
    OpenGL::GL
)
target_compile_features(nexus_modeling PRIVATE cxx_std_26)
if(NEXUS_BACKEND_VULKAN)
    target_link_libraries(nexus_modeling PRIVATE nexus_backend_vulkan)
    target_compile_definitions(nexus_modeling PRIVATE NEXUS_BACKEND_VULKAN)
endif()
if(NEXUS_BACKEND_NULL)
    target_compile_definitions(nexus_modeling PRIVATE NEXUS_BACKEND_NULL)
endif()

# ── Install targets ───────────────────────────────────────────────────────────
install(TARGETS nexus_gfx_core nexus_neural ARCHIVE DESTINATION lib)
# ── nxrender: headless software-raster preview tool (AI visual dev-loop) ─────
#  Renders a mesh (primitive or .nxm) to a PNG via the CPU SoftwareRasterizer.
#  Links only the core; no GPU/window deps so it builds and runs headless in CI.
add_executable(nxrender
    ${CMAKE_CURRENT_SOURCE_DIR}/../../tools/nxrender/main.cpp
)
target_include_directories(nxrender PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_link_libraries(nxrender PRIVATE nexus_gfx_core)
target_compile_features(nxrender PRIVATE cxx_std_26)

install(DIRECTORY include/ DESTINATION include)
