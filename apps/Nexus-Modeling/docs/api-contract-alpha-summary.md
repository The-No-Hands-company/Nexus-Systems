# Alpha API Contract Summary (1.0-alpha)

Date: 2026-05-09

This page summarizes the 1.0-alpha public API contract using the enforced manifest and the domain ownership model.

## Sources of Truth

1. Public API manifest: tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt
2. Ownership map: docs/kernel-capability-map.md
3. API freeze policy: docs/api-freeze-alpha.md
4. Enforcement test: tests/kernel/test_ApiFreezeAudit.cpp

## Contract Scope

The 1.0-alpha contract is all public headers under:

1. src/kernel/include/nexus

Manifest status at this baseline:

1. Total public headers: 39
2. Audit enforcement: required in CI via ApiFreezeAudit.PublicHeaderManifestMatchesWorkspace
3. Drift policy: any add/remove requires manifest update in the same change

## Domain Ownership Mapping

Domain assignment is rooted in folder/API boundaries and ownership rules from docs/kernel-capability-map.md.

| Domain | Primary Owner | Secondary Owner | API Roots Covered | Header Count |
|---|---|---|---|---:|
| Geometry and Parametric | Geometry Core | Runtime and Evaluation | src/kernel/include/nexus/geometry, src/kernel/include/nexus/parametric | 12 |
| Rendering and GFX | Render Core | Platform Backend | src/kernel/include/nexus/gfx, src/kernel/include/nexus/render | 12 |
| Animation | Evaluation and Animation Core | Geometry Core | src/kernel/include/nexus/animation | 3 |
| Simulation | Simulation Core | Evaluation and Animation | src/kernel/include/nexus/sim | 1 |
| Asset and Data | Asset and Data Core | Integration and Cloud | src/kernel/include/nexus/asset | 3 |
| Scripting and Automation | Scripting and Integration Core | Domain owners per API | src/kernel/include/nexus/automation | 1 |
| Procedural Evaluation Graph | Evaluation and Animation Core | Geometry Core | src/kernel/include/nexus/eval | 1 |
| Neural Integrations | Render Core | Platform Backend | src/kernel/include/nexus/neural | 1 |
| Kernel Entry and Cross-Domain Root | Asset and Data Core | Domain owners per API | src/kernel/include/nexus/Kernel.h | 1 |
| Vulkan Public Compiler Interface | Render Core | Platform Backend | src/kernel/include/nexus/gfx/vulkan | 1 |

Total: 39 headers.

## Frozen Public Header Inventory

1. src/kernel/include/nexus/Kernel.h
2. src/kernel/include/nexus/animation/AnimationCore.h
3. src/kernel/include/nexus/animation/AnimationSerialization.h
4. src/kernel/include/nexus/animation/SkeletonRetargeter.h
5. src/kernel/include/nexus/asset/AssetDependencyGraph.h
6. src/kernel/include/nexus/asset/SceneAsset.h
7. src/kernel/include/nexus/asset/SceneAssetImporter.h
8. src/kernel/include/nexus/automation/AutomationScript.h
9. src/kernel/include/nexus/eval/EvalGraph.h
10. src/kernel/include/nexus/geometry/BevelChamfer.h
11. src/kernel/include/nexus/geometry/BooleanOperation.h
12. src/kernel/include/nexus/geometry/ExtrudeOperation.h
13. src/kernel/include/nexus/geometry/GeometryKernel.h
14. src/kernel/include/nexus/geometry/InsetFacesOperation.h
15. src/kernel/include/nexus/geometry/Mesh.h
16. src/kernel/include/nexus/geometry/MeshDiagnosticOverlay.h
17. src/kernel/include/nexus/geometry/MeshIO.h
18. src/kernel/include/nexus/geometry/RemeshOperation.h
19. src/kernel/include/nexus/gfx/Allocator.h
20. src/kernel/include/nexus/gfx/CommandBuffer.h
21. src/kernel/include/nexus/gfx/Device.h
22. src/kernel/include/nexus/gfx/FrameScheduler.h
23. src/kernel/include/nexus/gfx/GaussianSplatting.h
24. src/kernel/include/nexus/gfx/RenderContext.h
25. src/kernel/include/nexus/gfx/Swapchain.h
26. src/kernel/include/nexus/gfx/Types.h
27. src/kernel/include/nexus/gfx/vulkan/ShaderCompiler.h
28. src/kernel/include/nexus/neural/NeuralRenderer.h
29. src/kernel/include/nexus/parametric/ConstraintGraph.h
30. src/kernel/include/nexus/parametric/ParametricSamples.h
31. src/kernel/include/nexus/parametric/ParametricSerialization.h
32. src/kernel/include/nexus/parametric/ParametricSolver.h
33. src/kernel/include/nexus/render/Camera.h
34. src/kernel/include/nexus/render/FrameCaptureExporter.h
35. src/kernel/include/nexus/render/RenderGraphValidator.h
36. src/kernel/include/nexus/render/Renderer.h
37. src/kernel/include/nexus/render/SceneGraph.h
38. src/kernel/include/nexus/render/TemporalAccumulation.h
39. src/kernel/include/nexus/sim/SimulationCore.h

## Change-Control Rules for Alpha Tag Window

1. Additive-only changes are allowed when domain ownership, behavior tests, and docs updates are included.
2. Removals/renames require explicit compatibility rationale and migration notes.
3. Any manifest drift without intentional update fails API audit.
4. Cross-domain API changes require reviewers from each impacted domain.

## Verification Commands

1. cmake --build build -j$(nproc)
2. ./build/tests/nexus_kernel_tests --gtest_filter="ApiFreezeAudit.*"
3. ./build/tests/nexus_kernel_tests
