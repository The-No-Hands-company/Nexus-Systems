# Nexus Modeling — Adding a CAD Feature

*How to add a new parametric CAD feature (Sketch, Extrude, Revolve, Fillet, etc.) to the CAD kernel and editor.*

---

## 1. Feature Architecture

```
CadDocument (history)
    └── vector<CadFeature> features
             │
             ├─ Sketch
             ├─ Extrude
             ├─ Revolve
             ├─ Fillet
             ├─ Chamfer
             └─ Boolean (Union/Diff/Intersect)
```

Each feature:
- **Inputs**: parameters + references to parent features
- **Execution**: `regenerate()` → produces `Mesh` + metadata
- **Serialization**: JSON for `.nxm` files
- **UI**: Inspector panel, gizmos, context

---

## 1. Feature Base Class

```cpp
// include/nexus/cad/CadFeature.h
class CadFeature {
public:
    enum class Kind { Sketch, Extrude, Revolve, Fillet, Chamfer, Boolean, Pattern, Mirror };

    FeatureId id = kInvalidFeatureId;
    Kind kind;
    std::string name;
    std::vector<FeatureId> parents;      // upstream dependencies
    std::unordered_map<std::string, Property> params;  // JSON-serializable

    virtual ~CadFeature() = default;
    virtual bool regenerate(CadDocument& doc) = 0;  // recompute mesh
    virtual void serialize(json&) const;
    virtual void deserialize(const json&);
};
```

---

## 2. Adding a New Feature Type

### Step 1: Define the Feature Class

```cpp
// include/nexus/cad/CadFeature.h (add enum)
enum class Kind { ..., MyNewFeature };

// src/kernel/src/cad/CadMyNewFeature.h
#pragma once
#include <nexus/cad/CadFeature.h>

namespace nexus::cad {

class MyNewFeature : public CadFeature {
public:
    struct Params {
        float depth = 10.0f;
        bool symmetric = false;
        // ... your parameters
    };

    Params params;

    bool regenerate(CadDocument& doc) override;
    void serialize(json&) const override;
    void deserialize(const json&) override;
};
```

### Step 2: Implement Regeneration

```cpp
// src/kernel/src/cad/CadMyNewFeature.cpp
#include <nexus/cad/CadMyNewFeature.h>
#include <nexus/geometry/primitives.h>

namespace nexus::cad {

bool MyNewFeature::regenerate(CadDocument& doc) {
    // 1. Resolve parent feature meshes
    if (parents.empty()) return false;
    auto* parent = doc.history().node(parents[0]);
    if (!parent || !parent->mesh) return false;

    // 2. Apply operation using geometry kernel
    Mesh result;
    // ... your algorithm here ...
    
    if (!result.isValid()) return false;

    // 3. Store result
    mesh = std::move(result);
    dirty = false;
    return true;
}

void MyNewFeature::serialize(json& j) const {
    j = json{
        {"kind", "MyNewFeature"},
        {"id", id},
        {"name", name},
        {"parents", parents},
        {"params", {
            {"depth", params.depth},
            {"symmetric", params.symmetric}
        }}
    };
}

void MyNewFeature::deserialize(const json& j) {
    id = j["id"];
    name = j["name"];
    parents = j["parents"].get<std::vector<FeatureId>>();
    params.depth = j["params"]["depth"];
    params.symmetric = j["params"]["symmetric"];
}
```

### Step 3: Register Feature Factory

```cpp
// src/kernel/src/cad/CadFeatureFactory.cpp
std::unique_ptr<CadFeature> CadFeatureFactory::create(Kind kind) {
    switch (kind) {
        // ... existing cases
        case Kind::MyNewFeature:
            return std::make_unique<MyNewFeature>();
    }
    return nullptr;
}
```

### Step 4: Register in CMake

```cmake
# src/kernel/CMakeLists.txt
set(CAD_SOURCES
    ...
    src/cad/CadMyNewFeature.cpp
)
```

### Step 5: Editor Integration

```cpp
// src/kernel/src/app/EditorUI.cpp - renderMenuBar
if (ImGui::MenuItem("My New Feature")) {
    if (ctx.orchestrator) ctx.orchestrator->switchTo("my-new-feature");
}

// Add mode to orchestrator
// src/kernel/src/app/AppMode.cpp
static const ModeDesc MODES[] = {
    // ...
    {"my-new-feature", "MyNewFeature", []() { /* activate */ }},
};
```

### Step 6: Inspector Panel

```cpp
// src/kernel/src/app/EditorUI.cpp - renderProperties
void EditorUI::renderProperties(AppContext& ctx, FeatureId selectedId) {
    // ...
    if (auto* feat = dynamic_cast<MyNewFeature*>(ctx.document->history().node(selectedId))) {
        ImGui::DragFloat("Depth", &feat->params.depth, 0.1f, 0.1f, 100.0f);
        ImGui::Checkbox("Symmetric", &feat->params.symmetric);
        
        if (ImGui::Button("Regenerate")) {
            feat->regenerate(*ctx.document);
        }
    }
}
```

---

## 3. Command for Undo/Redo

```cpp
// include/nexus/cad/CadCommand.h
class MyNewFeatureCommand : public CadCommand {
public:
    MyNewFeatureCommand(FeatureId fid, Mesh savedMesh)
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

## 4. Tests

```cpp
// tests/kernel/test_MyNewFeature.cpp
#include <gtest/gtest.h>
#include <nexus/cad/CadMyNewFeature.h>
#include <nexus/cad/CadDocument.h>

TEST(MyNewFeature, RegeneratesCorrectly) {
    CadDocument doc;
    // Setup: create parent feature with mesh
    // ...
    
    MyNewFeature feat;
    feat.parents = {parentId};
    feat.params.depth = 5.0f;
    
    EXPECT_TRUE(feat.regenerate(doc));
    
    auto* node = doc.history().node(feat.id);
    ASSERT_NE(node, nullptr);
    ASSERT_TRUE(node->mesh->isValid());
}

TEST(MyNewFeature, SerializationRoundtrip) {
    MyNewFeature feat;
    feat.params.depth = 5.0f;
    feat.params.symmetric = true;
    
    json j;
    feat.serialize(j);
    
    MyNewFeature feat2;
    feat2.deserialize(j);
    
    EXPECT_EQ(feat.params.depth, feat2.params.depth);
    EXPECT_EQ(feat.params.symmetric, feat2.params.symmetric);
}
```

Add to `tests/CMakeLists.txt`:
```cmake
add_executable(nexus_kernel_tests ... test_MyNewFeature.cpp)
target_link_libraries(nexus_kernel_tests PRIVATE nexus_gfx_core gtest_main)
```

---

## 5. Checklist

- [ ] Feature class in `src/kernel/src/cad/`
- [ ] Header in `include/nexus/cad/`
- [ ] CMake registration in `src/kernel/CMakeLists.txt`
- [ ] FeatureFactory registration
- [ ] Orchestrator mode registration
- [ ] EditorUI toolbar/menu integration
- [ ] Inspector panel property editing
- [ ] Command for undo/redo
- [ ] Unit tests (valid + invalid inputs)
- [ ] Serialization round-trip test
- [ ] CMake registration
- [ ] Documentation updates
- [ ] CI passes

---

## 6. Architecture Notes

**Regeneration Order**: Topological sort of feature DAG by parent dependencies. `CadDocument::regenerateAll()` handles this.

**Dirty Flag**: Set when parents change. `CadDocument::markDirty(FeatureId)` propagates downstream.

**Selection Sync**: `AppState::selectedId` ↔ `AppContext::activeSelectedFeature` ↔ Outliner selection.

---

*Template: `docs/developer/adding-cad-feature.md`*