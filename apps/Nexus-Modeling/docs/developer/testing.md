# Nexus Modeling — Testing Strategy

*Comprehensive testing approach: unit, integration, property-based, performance, and deterministic CI.*

---

## Test Philosophy

1. **Determinism First** — All tests must pass on Null backend (headless, no GPU)
2. **Property-Based** — Test invariants, not just examples
3. **Regression Gates** — Performance baselines, API freeze checks
3. **Fast Feedback** — Unit tests < 1s, full suite < 30s
4. **Real Coverage** — Test actual kernel code paths, not mocks

---

## Test Categories

### 1. Unit Tests (GoogleTest)

**Location:** `tests/kernel/`
**Target:** `nexus_kernel_tests`
**Count:** 1921+ tests

```cpp
// tests/kernel/test_HalfEdgeMesh.cpp
TEST(HalfEdgeMesh, InsertEdgeVertex_IntegrityClean) {
    Mesh cube = primitives::makeBox(2,2,2);
    auto hem = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(hem.has_value());
    
    uint32_t edgeId = 0;  // Pick an edge
    uint32_t newVert = hem->insertEdgeVertex(edgeId, 0.5f);
    
    auto report = hem->checkIntegrity();
    EXPECT_TRUE(report.ok) << report.reason;
    EXPECT_EQ(report.euler, 2);  // Cube: V=8,E=12,F=6 → 8-12+6=2
}

TEST(HalfEdgeMesh, CollapseEdge_PreservesManifold) {
    // Property test: collapseEdge on manifold mesh → manifold mesh
}

TEST(MeshBoolean, CubeUnionCube_Watertight) {
    Mesh a = primitives::makeBox(2,2,2);
    Mesh b = primitives::makeBox(2,2,2);
    b.translate({1,0,0});  // Overlap
    
    auto result = MeshBoolean::compute(a, b, BooleanOp::Union);
    ASSERT_TRUE(result.ok);
    
    auto analysis = MeshAnalysis::analyze(result.result);
    EXPECT_TRUE(analysis.isWatertight);
    EXPECT_TRUE(analysis.isManifold);
    EXPECT_EQ(analysis.eulerCharacteristic, 2);
}
```

### 2. Property-Based Tests (RapidCheck-style)

```cpp
// tests/kernel/test_PropertyBased.cpp
#include <rapidcheck.h>

RC_GTEST_PROP(HalfEdgeMesh, EulerInvariantHolds, ()) {
    // Generate random mesh
    Mesh mesh = *rc::gen::arbitrary<Mesh>();
    
    // Convert to half-edge
    auto hem = HalfEdgeMesh::fromMesh(mesh);
    RC_PRE(hem.has_value());
    
    // Apply random sequence of Euler ops
    for (int i = 0; i < 100; ++i) {
        auto op = *rc::gen::element(
            rc::gen::just([]{ return "insertEdgeVertex"; }),
            rc::gen::just([]{ return "splitEdge"; }),
            rc::gen::just([]{ return "flipEdge"; }),
            rc::gen::just([]{ return "pokeFace"; })
        );
        // ... apply op ...
        
        // Invariant: integrity always clean
        auto report = hem->checkIntegrity();
        RC_ASSERT(report.ok);
    }
}

RC_GTEST_PROP(MeshBoolean, UnionCommutative, ()) {
    Mesh a = *rc::gen::arbitrary<Mesh>();
    Mesh b = *rc::gen::arbitrary<Mesh>();
    
    auto r1 = MeshBoolean::compute(a, b, BooleanOp::Union);
    auto r2 = MeshBoolean::compute(b, a, BooleanOp::Union);
    
    RC_PRE(r1.ok && r2.ok);
    
    // Volumes should be equal (within tolerance)
    RC_ASSERT(std::abs(r1.result.volume() - r2.result.volume()) < 1e-4f);
}
```

### 3. Integration Tests

```cpp
// tests/integration/test_RenderPipeline.cpp
TEST(VulkanRenderer, OffscreenClearAndReadback) {
    RenderContext ctx = createHeadlessContext();
    Renderer renderer(ctx);
    
    // Render a frame
    renderer.beginFrame();
    renderer.clear({0.1f, 0.2f, 0.3f, 1.0f});
    renderer.endFrame();
    
    // Read back
    auto pixels = renderer.readbackColor();
    EXPECT_NEAR(pixels[0], 0.1f * 255, 2);
    EXPECT_NEAR(pixels[1], 0.2f * 255, 2);
    EXPECT_NEAR(pixels[2], 0.3f * 255, 2);
}

TEST(VulkanRenderer, GBufferMrtGeometryPipelineRunsThroughRenderer) {
    // Full G-buffer pass with mesh
}
```

### 4. Performance Regression Tests

```cpp
// tests/perf/test_BooleanPerf.cpp
TEST(KernelPerf, BooleanCubeUnder1ms) {
    Mesh a = primitives::makeBox(2,2,2);
    Mesh b = primitives::makeBox(2,2,2);
    b.translate({1,0,0});
    
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        auto result = MeshBoolean::compute(a, b, BooleanOp::Union);
        EXPECT_TRUE(result.ok);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LT(ms, 100);  // 100 iterations < 100ms → <1ms each
}

TEST(KernelPerf, ExtrudeUnder1ms) {
    // ...
}

TEST(KernelPerf, Decimate100kTri50PercentUnder100ms) {
    // ...
}
```

### 5. API Freeze Audit

```cpp
// tests/kernel/test_ApiFreeze.cpp
TEST(ApiFreezeAudit, PublicHeaderManifestMatchesWorkspace) {
    // Scans src/kernel/include/nexus/**/*.h
    // Compares against tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt
    // Fails if mismatch (added/removed public API)
}
```

---

## Test Infrastructure

### Fixtures

```cpp
// tests/kernel/fixtures/TestFixtures.h
namespace nexus::test {

Mesh makeCube(float size = 1.0f) {
    return primitives::makeBox(size, size, size);
}

Mesh makeSphere(float radius = 1.0f, int seg = 16) {
    return primitives::makeSphere(radius, seg, seg);
}

Mesh makeTorus(float R = 2.0f, float r = 0.5f) {
    return primitives::makeTorus(R, r, 32, 16);
}

CadDocument makeSimpleDocument() {
    CadDocument doc;
    // Add box feature
    // Add extrude feature
    return doc;
}

} // namespace nexus::test
```

### Test Utilities

```cpp
// tests/kernel/test_Utils.h
namespace nexus::test {

bool meshesEqual(const Mesh& a, const Mesh& b, float tol = 1e-5f);
bool meshIsWatertight(const Mesh& m);
bool meshIsManifold(const Mesh& m);
float meshVolume(const Mesh& m);
float meshSurfaceArea(const Mesh& m);
void checkMeshIntegrity(const Mesh& m);

std::string meshToString(const Mesh& m);
void dumpMesh(const Mesh& m, const std::string& path);

} // namespace nexus::test
```

---

## Running Tests

### All Tests (CI)

```bash
# Configure
cmake -S . -B build -DNEXUS_BACKEND_VULKAN=ON -DNEXUS_BACKEND_NULL=ON

# Build
cmake --build build -j$(nproc)

# Run all tests (headless via Null backend)
ctest --test-dir build --output-on-failure

# With verbose output
ctest --test-dir build --output-on-failure -V
```

### Filtered Tests

```bash
# Run specific pattern
ctest --test-dir build -R "HalfEdge" --output-on-failure

# Run single test suite
./build/tests/nexus_kernel_tests --gtest_filter="MeshBoolean.*"

# Run single test
./build/tests/nexus_kernel_tests --gtest_filter="MeshBoolean.CubeUnionCube_Watertight"

# List all tests
./build/tests/nexus_kernel_tests --gtest_list_tests
```

### Performance Tests

```bash
# Run perf tests
./build/tests/nexus_kernel_perf_smoke

# With benchmark output
./build/tests/nexus_kernel_perf_smoke --benchmark_format=json --benchmark_out=perf.json
```

### Debug Build with Sanitizers

```bash
cmake -S . -B build_asan -DCMAKE_BUILD_TYPE=Debug
cmake --build build_asan -j$(nproc)
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build_asan --output-on-failure
```

---

## Test Organization

```
tests/
├── kernel/
│   ├── test_AnalyticBRep.cpp
│   ├── test_BooleanOperation.cpp
│   ├── test_HalfEdgeMesh.cpp
│   ├── test_MeshBoolean.cpp
│   ├── test_MeshDecimator.cpp
│   ├── test_MeshLaplacian.cpp
│   ├── test_MeshRemesh.cpp
│   ├── test_SubdivisionSurface.cpp
│   ├── test_Delaunay2D.cpp
│   ├── test_TetDelaunay3D.cpp
│   ├── test_RobustPredicates.cpp
│   ├── test_Tolerance.cpp
│   ├── test_MeshIO.cpp
│   ├── test_Primitives.cpp
│   ├── test_SceneGraph.cpp
│   ├── test_Camera.cpp
│   ├── test_Frustum.cpp
│   ├── test_Picking.cpp
│   ├── test_Transform.cpp
│   ├── test_Quaternion.cpp
│   ├── test_Mat4.cpp
│   ├── test_Vec3.cpp
│   ├── test_CadDocument.cpp
│   ├── test_CadFeature.cpp
│   ├── test_CadSketch.cpp
│   ├── test_CadSolver.cpp
│   ├── test_ConstraintSolver.cpp
│   ├── test_Animation.cpp
│   ├── test_Simulation.cpp
│   ├── test_Sculpt.cpp
│   ├── test_Parameterization.cpp
│   ├── test_Integrity.cpp
│   ├── test_ApiFreeze.cpp
│   ├── test_PropertyBased.cpp
│   ├── test_Utils.h
│   └── fixtures/
│       ├── api_surface_manifest_alpha_v1.txt
│       ├── test_models/
│       └── reference_images/
└── integration/
    ├── test_RenderPipeline.cpp
    ├── test_HeadlessRender.cpp
    └── test_ViewportPicking.cpp
```

---

## Test Naming Conventions

```cpp
// Suite: Component/Feature
// Test: Scenario_ExpectedBehavior

TEST(MeshBoolean, CubeUnionCube_Watertight) { }
TEST(HalfEdgeMesh, InsertEdgeVertex_IntegrityClean) { }
TEST(CadSketch, ConstraintCoincident_SolvesCorrectly) { }
TEST(SubdivisionSurface, CatmullClark_CreasePreserved) { }
TEST(BooleanOperation, Difference_CoplanarFaces_Handled) { }
```

---

## Coverage Goals

| Component | Target | Current |
|-----------|--------|---------|
| Geometry Kernel | 95% | 92% |
| CAD Layer | 90% | 85% |
| Renderer | 80% | 70% |
| Simulation | 85% | 60% |
| Animation | 80% | 50% |
| Overall | 90% | 82% |

```bash
# Generate coverage report
cmake -S . -B build_cov -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
cmake --build build_cov -j$(nproc)
ctest --test-dir build_cov
lcov --capture --directory build_cov --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

---

## Determinism Requirements

All tests **must** produce identical results across:
- Debug/Release builds
- Different compilers (GCC/Clang/MSVC)
- Different optimization levels
- Vulkan/Null backends

**Enforced by:**
- `-ffp-contract=off -fno-fast-math`
- Fixed iteration order in algorithms
- Stable sorts with deterministic tie-breaking
- xxHash for hashing (not std::hash)
- Fixed RNG seeds (SplitMix64)

---

## Continuous Integration

```yaml
# .github/workflows/test.yml
jobs:
  test:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        compiler: [gcc-13, clang-17]
        build_type: [Debug, Release]
    steps:
      - uses: actions/checkout@v4
      - name: Install deps
        run: sudo apt-get install -y libvulkan-dev libglfw3-dev
      - name: Build
        run: |
          cmake -S . -B build -DCMAKE_CXX_COMPILER=${{ matrix.compiler }} -DCMAKE_BUILD_TYPE=${{ matrix.build_type }}
          cmake --build build -j$(nproc)
      - name: Test
        run: xvfb-run ctest --test-dir build --output-on-failure
```

---

*Kernel v0.1.0-dev | 2026 tests passing | C++26 | Vulkan 1.3*