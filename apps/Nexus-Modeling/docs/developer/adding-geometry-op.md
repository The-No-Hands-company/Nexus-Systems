# Nexus Modeling — Adding a Geometry Operation

*How to add a new mesh operation to the kernel and wire it through to the editor.*

---

## 1. Where Operations Live

```
src/kernel/src/geometry/
├── MeshBoolean.cpp          // CSG
├── MeshRemesh.cpp           // voxel / quad remesh
├── MeshDecimator.cpp        // decimation
├── SubdivisionSurface.cpp   // Catmull-Clark / Loop
├── MeshLaplacian.cpp        // smoothing, fairing
├── DirectModeling.cpp       // push/pull, inset, poke, edge loops
├── BevelChamfer.cpp         // (stubs)
├── FaceFillet.cpp           // (stubs)
└── ...                      // ~60 files
```

Every operation follows the same pattern:
1. **Input validation** — check mesh validity, selection, parameters
2. **Core algorithm** — pure geometry, no UI deps
3. **Result construction** — build new `Mesh` with attributes
4. **Command wrapping** — `TransformCommand` for undo/redo

---

## 2. Adding a New Mesh Operation

### Step 1: Core Algorithm (`.cpp` in `src/geometry/`)

```cpp
// src/kernel/src/geometry/MyNewOp.cpp
#include <nexus/geometry/MyNewOp.h>
#include <nexus/geometry/HalfEdgeMesh.h>

namespace nexus::geometry {

struct MyNewOpOptions {
    float strength = 1.0f;
    bool preserveBoundary = true;
};

struct MyNewOpReport {
    bool ok = true;
    std::string error;
    Mesh result;
};

[[nodiscard]] MyNewOpReport myNewOp(const Mesh& input, const MyNewOpOptions& opts)
{
    if (!input.isValid()) return {false, "Invalid input mesh", {}};

    // 1. Convert to half-edge for topological ops
    auto hemOpt = HalfEdgeMesh::fromMesh(input);
    if (!hemOpt) return {false, "Failed to build half-edge structure", {}};
    HalfEdgeMesh hem = std::move(*hemOpt);

    // 2. Core algorithm — e.g., Laplacian smoothing
    // ... your algorithm here ...

    // 3. Convert back
    Mesh output = hem.toMesh();
    if (!output.isValid()) return {false, "Output mesh invalid", {}};

    return {true, {}, std::move(output)};
}
} // namespace nexus::geometry
```

### Header (`include/nexus/geometry/MyNewOp.h`)

```cpp
#pragma once
#include <nexus/geometry/Mesh.h>
#include <optional>
#include <string>

namespace nexus::geometry {

struct MyNewOpOptions { float strength = 1.0f; bool preserveBoundary = true; };
struct MyNewOpReport { bool ok = true; std::string error; Mesh result; };

[[nodiscard]] MyNewOpReport myNewOp(const Mesh&, const MyNewOpOptions&);
```

### Register in CMake

```cmake
# src/kernel/CMakeLists.txt
set(GEOMETRY_SOURCES
    ...
    src/geometry/MyNewOp.cpp
    ...
)
```

---

## 2. Command Wrapper (for Undo/Redo)

```cpp
// src/kernel/src/cad/CadCommand.h
class MyNewOpCommand : public CadCommand {
public:
    MyNewOpCommand(FeatureId fid, Mesh savedMesh)
        : m_fid(fid), m_savedMesh(std::move(savedMesh)) {}

    bool execute(CadDocument& doc) override {
        auto* node = doc.history().node(m_fid);
        if (!node || !node->mesh) return false;
        m_newMesh = *node->mesh;  // capture post-op state
        *node->mesh = std::move(m_savedMesh);
        return true;
    }
    bool undo(CadDocument& doc) override {
        auto* node = doc.history().node(m_fid);
        if (!node || !node->mesh) return false;
        *node->mesh = std::move(m_newMesh);
        return true;
    }
private:
    FeatureId m_fid;
    Mesh m_savedMesh, m_newMesh;
};
```

---

## 3. Editor Integration

### EditorUI: Add Toolbar Button

```cpp
// src/kernel/src/app/EditorUI.cpp
void EditorUI::renderToolbar(AppContext& ctx, TransformGizmo& gizmo)
{
    // ...
    if (ImGui::Button("My New Op")) {
        auto& ctx = *g_state.appContext;
        if (auto* node = ctx.document.history().node(ctx.activeSelectedFeature)) {
            if (node && node->mesh) {
                auto saved = *node->mesh;
                auto report = myNewOp(*node->mesh, MyNewOpOptions{1.0f, true});
                if (report.ok) {
                    auto cmd = std::make_unique<MyNewOpCommand>(ctx.activeSelectedFeature, std::move(saved));
                    ctx.document.executeCommand(std::move(cmd));
                }
            }
        }
    }
}
```

### Add to CMake

```cmake
# src/kernel/CMakeLists.txt
target_link_libraries(nexus_modeling PRIVATE nexus_gfx_core nexus_app)
target_compile_features(nexus_modeling PRIVATE cxx_std_26)
```

---

## 4. Tests

```cpp
// tests/kernel/test_MyNewOp.cpp
#include <gtest/gtest.h>
#include <nexus/geometry/MyNewOp.h>

TEST(MyNewOp, BasicSmoothing) {
    Mesh cube = primitives::makeBox(2,2,2);
    auto report = myNewOp(cube, MyNewOpOptions{0.5f, true});
    ASSERT_TRUE(report.ok);
    EXPECT_GT(report.result.vertexCount(), 0);
    // Check specific invariants...
}

TEST(MyNewOp, InvalidInput) {
    Mesh bad; // empty
    auto report = myNewOp(bad, MyNewOpOptions{});
    EXPECT_FALSE(report.ok);
    EXPECT_FALSE(report.error.empty());
}
```

Add to `tests/CMakeLists.txt`:
```cmake
add_executable(nexus_kernel_tests ... test_MyNewOp.cpp)
target_link_libraries(nexus_kernel_tests PRIVATE nexus_gfx_core gtest_main)
```

---

## 5. Run Tests

```bash
cmake --build build -j$(nproc)
ctest --test-dir build -R MyNewOp --output-on-failure
```

---

## 6. Documentation Update

1. Add entry to `docs/developer/kernel-api.md`
2. Add entry to `docs/developer/adding-geometry-op.md`
3. Update `docs/modeling-parity-index.md` (add feature to L1 matrix)

---

## Checklist

- [ ] Core algorithm in `src/kernel/src/geometry/`
- [ ] Header in `include/nexus/geometry/`
- [ ] CMake registration
- [ ] Command wrapper for undo/redo
- [ ] EditorUI toolbar integration
- [ ] Unit tests (valid + invalid inputs)
- [ ] Property tests (invariants preserved)
- [ ] CMake registration
- [ ] Documentation updates
- [ ] CI passes

---

*Template: `docs/developer/adding-geometry-op.md`*