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
    kernel/test_AlgorithmDepth.cpp
    kernel/test_AnimationBlend.cpp
    kernel/test_AnimationCore.cpp
    kernel/test_AnimationSerialization.cpp
    kernel/test_ApiFreezeAudit.cpp
    kernel/test_AppLayer.cpp
    kernel/test_AssetDependencyGraph.cpp
    kernel/test_AutomationScript.cpp
    kernel/test_BevelChamfer.cpp
    kernel/test_BRepBody.cpp
    kernel/test_BooleanOperation.cpp
    kernel/test_Brush.cpp
    kernel/test_CadDocument.cpp
    kernel/test_CadEndToEnd.cpp
    kernel/test_BattleTest.cpp
    kernel/test_Camera.cpp
    kernel/test_CameraExtended.cpp
    kernel/test_ConstrainedDelaunay2D.cpp
    kernel/test_CurveCurveIntersect.cpp
    kernel/test_CurveFairing.cpp
    kernel/test_DeepHardening.cpp
    kernel/test_Delaunay2D.cpp
    kernel/test_DirectModelingFaceFillet.cpp
    kernel/test_EdgeBridge.cpp
    kernel/test_EvalGraph.cpp
    kernel/test_ExtrudeOperation.cpp
    kernel/test_FeatureLineExtractor.cpp
    kernel/test_GPUAllocator.cpp
    kernel/test_Integrity.cpp
    kernel/test_InverseKinematics.cpp
    kernel/test_KnifeTool.cpp
    kernel/test_GeodesicPath.cpp
    kernel/test_GeometryKernel.cpp
    kernel/test_GordonSurface.cpp
    kernel/test_Hardening.cpp
    kernel/test_HalfEdgeMesh.cpp
    kernel/test_HEMOperations.cpp
    kernel/test_HEMEulerIntegrity.cpp
    kernel/test_Tolerance.cpp
    kernel/test_ManifoldEnforcement.cpp
    kernel/test_AnalyticBRep.cpp
    kernel/test_BRepSurfaceIntersect.cpp
    kernel/test_BRepImprint.cpp
    kernel/test_BRepImprintCircle.cpp
    kernel/test_BRepInnerLoop.cpp
    kernel/test_BRepMassProperties.cpp
    kernel/test_BRepOpenBox.cpp
    kernel/test_BRepPointPlaneSideSoS.cpp
    kernel/test_BRepSegmentTriangleSoS.cpp
    kernel/test_BRepSurfaceArea.cpp
    kernel/test_BRepExtrude.cpp
    kernel/test_BRepLoft.cpp
    kernel/test_BRepPyramid.cpp
    kernel/test_BRepRevolve.cpp
    kernel/test_BRepTube.cpp
    kernel/test_BRepTwistExtrude.cpp
    kernel/test_BRepClassifyPoint.cpp
    kernel/test_BRepClassifyFace.cpp
    kernel/test_BRepTransform.cpp
    kernel/test_BRepMutualImprint.cpp
    kernel/test_BRepBoolean.cpp
    kernel/test_BRepBooleanBody.cpp
    kernel/test_BRepBooleanCoincident.cpp
    kernel/test_BRepFacetedBoolean.cpp
    kernel/test_BRepFacetedSphere.cpp
    kernel/test_BRepBooleanInvariant.cpp
    kernel/test_BRepBooleanTorture.cpp
    kernel/test_BRepNearTangentTorture.cpp
    kernel/test_BRepChamfer.cpp
    kernel/test_BRepFillet.cpp
    kernel/test_BRepHollow.cpp
    kernel/test_BRepFeatureStack.cpp
    kernel/test_BRepMergeCoplanar.cpp
    kernel/test_BRepSimplify.cpp
    kernel/test_BRepSerialize.cpp
    kernel/test_HardSurfaceWorkflow.cpp
    kernel/test_InsetFacesOperation.cpp
    kernel/test_Material.cpp
    kernel/test_Mesh.cpp
    kernel/test_TriTriIntersect.cpp
    kernel/test_TriangleRetriangulate.cpp
    kernel/test_MeshCut.cpp
    kernel/test_MeshBooleanRobust.cpp
    kernel/test_MeshBooleanExactClassify.cpp
    kernel/test_MeshBooleanSeamCharacterization.cpp
    kernel/test_KernelFuzz.cpp
    kernel/test_ModifierStack.cpp
    kernel/test_MeshAmbientOcclusion.cpp
    kernel/test_MeshBVH.cpp
    kernel/test_MeshBoolean.cpp
    kernel/test_MeshBoundaryLoop.cpp
    kernel/test_MeshCageDeform.cpp
    kernel/test_MeshClosestPoint.cpp
    kernel/test_MeshConvexHull.cpp
    kernel/test_MeshCrossSection.cpp
    kernel/test_MeshDecimator.cpp
    kernel/test_MeshDiagnosticOverlay.cpp
    kernel/test_MeshDisplace.cpp
    kernel/test_MeshGeodesicDistance.cpp
    kernel/test_MeshHarmonicField.cpp
    kernel/test_MeshHausdorffDistance.cpp
    kernel/test_MeshIO.cpp
    kernel/test_MeshInstancer.cpp
    kernel/test_MeshIslandDecomposition.cpp
    kernel/test_MeshLaplacian.cpp
    kernel/test_MeshMassProperties.cpp
    kernel/test_MeshMorphTarget.cpp
    kernel/test_MeshNormals.cpp
    kernel/test_MeshPointSample.cpp
    kernel/test_MeshPoissonDisk.cpp
    kernel/test_MeshRepair.cpp
    kernel/test_MeshSelfIntersection.cpp
    kernel/test_MeshSignedDistanceField.cpp
    kernel/test_MeshSimplify.cpp
    kernel/test_MeshSkinning.cpp
    kernel/test_MeshSurfaceNets.cpp
    kernel/test_MeshTangentSpace.cpp
    kernel/test_MeshThicken.cpp
    kernel/test_MeshTopologyValidation.cpp
    kernel/test_MeshTopologyAnalysis.cpp
    kernel/test_MeshTransform.cpp
    kernel/test_MeshUVUnwrap.cpp
    kernel/test_MeshVertexWeld.cpp
    kernel/test_MeshWindingNumber.cpp
    kernel/test_Meshlet.cpp
    kernel/test_ModelingShell.cpp
    kernel/test_NeuralRenderer.cpp
    kernel/test_NodeScene.cpp
    kernel/test_Noise3D.cpp
    kernel/test_NullDevice.cpp
    kernel/test_NullDeviceExtended.cpp
    kernel/test_NurbsCurve.cpp
    kernel/test_NurbsCurveArcLength.cpp
    kernel/test_NurbsCurveClosestPoint.cpp
    kernel/test_NurbsCurveFit.cpp
    kernel/test_NurbsCurveOffset.cpp
    kernel/test_NurbsCurveSplit.cpp
    kernel/test_NurbsIntersection.cpp
    kernel/test_NurbsKnotRefinement.cpp
    kernel/test_NurbsSurface.cpp
    kernel/test_NurbsSurfaceArea.cpp
    kernel/test_NurbsSurfaceClosestPoint.cpp
    kernel/test_NurbsSurfaceFit.cpp
    kernel/test_NurbsSurfaceOffset.cpp
    kernel/test_NurbsSurfaceSplit.cpp
    kernel/test_NurbsToMesh.cpp
    kernel/test_ParametricFoundation.cpp
    kernel/test_ParametricEndToEnd.cpp
    kernel/test_PointCloudNormals.cpp
    kernel/test_Production.cpp
    kernel/test_QuadRemesh.cpp
    kernel/test_RailLoft.cpp
    kernel/test_RemeshOperation.cpp
    kernel/test_RenderContext.cpp
    kernel/test_RenderContextExtended.cpp
    kernel/test_RenderPipelineV1.cpp
    kernel/test_Renderer.cpp
    kernel/test_RendererBehavior.cpp
    kernel/test_RobustPredicates.cpp
    kernel/test_SceneAsset.cpp
    kernel/test_SceneAssetImporter.cpp
    kernel/test_SceneAssetTextAdapter.cpp
    kernel/test_SceneGraph.cpp
    kernel/test_SnappingEngine.cpp
    kernel/test_SkeletonRetargeter.cpp
    kernel/test_SceneGraphExtended.cpp
    kernel/test_SculptSession.cpp
    kernel/test_SimulationCore.cpp
    kernel/test_SimulationCoupling.cpp
    kernel/test_SimulationDriver.cpp
    kernel/test_ClothSolver.cpp
    kernel/test_FluidSolver.cpp
    kernel/test_SolidOperations.cpp
    kernel/test_SubdivisionSurface.cpp
    kernel/test_SurfaceContinuity.cpp
    kernel/test_SurfaceCurvature.cpp
    kernel/test_SurfaceCurveProjection.cpp
    kernel/test_SurfaceIntegration.cpp
    kernel/test_SurfaceOffset.cpp
    kernel/test_SurfacePatchNetwork.cpp
    kernel/test_SurfaceSurfaceIntersect.cpp
    kernel/test_SurfaceTrim.cpp
    kernel/test_SweepLoft.cpp
    kernel/test_ProfileRevolve.cpp
    kernel/test_TopologyGolden.cpp
    kernel/test_TransformOrientation.cpp
    kernel/test_TrimBoolean.cpp
    kernel/test_Types.cpp
    kernel/test_TypesExtended.cpp
    kernel/test_Voronoi2D.cpp
    kernel/test_GaussianSplatting.cpp
)

add_executable(nexus_kernel_perf_smoke
    kernel/perf_smoke.cpp
)

if(NEXUS_BACKEND_VULKAN)
    target_sources(nexus_kernel_tests PRIVATE
        kernel/test_VulkanShaderCompiler.cpp
        kernel/test_VulkanPipeline.cpp
        kernel/test_VulkanFrameScheduler.cpp
        kernel/test_VulkanSwapchainStorageUsage.cpp
        kernel/test_VulkanSwapchainFallback.cpp
        kernel/test_VulkanRegression.cpp
        kernel/test_VulkanShaderBindingTable.cpp
        kernel/test_VulkanRayTracingDispatch.cpp
        kernel/test_VulkanDeferredDraw.cpp
        kernel/test_VulkanRendererOffscreen.cpp
    )
    # SBT layout math lives in the backend's internal headers; expose them to the
    # test target so the pure (GPU-free) arithmetic can be unit-tested directly.
    target_include_directories(nexus_kernel_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/kernel/src/backend/vulkan
    )
endif()

target_link_libraries(nexus_kernel_tests
    PRIVATE nexus_gfx_core nexus_app nexus_backend_null nexus_neural GTest::gtest_main
    $<$<BOOL:${NEXUS_BACKEND_VULKAN}>:nexus_backend_vulkan>
)

target_compile_features(nexus_kernel_tests PRIVATE cxx_std_26)
target_compile_features(nexus_kernel_perf_smoke PRIVATE cxx_std_26)

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
