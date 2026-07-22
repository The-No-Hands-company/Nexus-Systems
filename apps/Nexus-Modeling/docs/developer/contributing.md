# Nexus Modeling — Contributing Guide

*How to contribute code, documentation, and ideas to Nexus Modeling.*

---

## Quick Start

```bash
# 1. Fork the repo
git clone https://github.com/YOUR_FORK/nexus.git
cd nexus/apps/Nexus-Modeling

# 2. Create feature branch
git checkout -b feat/your-feature-name

# 3. Make changes, test, commit
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DNEXUS_SANITIZERS=ON
cmake --build build -j$(nproc)
ctest --test-dir build

# 4. Push + PR
git push origin feat/your-feature-name
```

---

## Code Standards

### C++ Style
- **Standard**: C++26 (GCC 13+, Clang 17+, MSVC 19.40+)
- **Format**: `clang-format` (`.clang-format` in root)
- **Lint**: `clang-tidy` (`.clang-tidy` in root)
- **Headers**: `#pragma once` + Doxygen comments for public API
- **Naming**: `PascalCase` types, `camelCase` functions/vars, `SCREAMING_SNAKE` constants

### Commit Messages
```
type(scope): brief summary

Longer description if needed. Reference issues.

Fixes #123
```

| Type | Use Case |
|------|----------|
| `feat` | New feature |
| `fix` | Bug fix |
| `refactor` | Code restructuring |
| `perf` | Performance improvement |
| `docs` | Documentation |
| `test` | Test additions |
| `chore` | Build/CI/maintenance |
| `revert` | Reverting a commit |

---

## Pull Request Process

### Before Submitting
- [ ] `clang-format -i` on changed files
- [ ] `clang-tidy` passes (no new warnings)
- [ ] All tests pass: `ctest --test-dir build`
- [ ] New tests added for new functionality
- [ ] Documentation updated (if user-facing)
- [ ] CHANGELOG.md updated (if user-facing)

### PR Template
```markdown
## Summary
Brief description of changes.

## Type
- [ ] Bug fix
- [ ] New feature
- [ ] Refactor
- [ ] Performance
- [ ] Documentation
- [ ] Tests

## Testing
- [ ] All existing tests pass
- [ ] New tests added
- [ ] Manual testing performed

## Screenshots (if UI)
<!-- Drag & drop images -->

## Checklist
- [ ] `clang-format -i` applied
- [ ] `clang-tidy` passes
- [ ] `ctest --test-dir build` passes
- [ ] Documentation updated
- [ ] CHANGELOG.md updated
```

### Review Process
1. **Automated checks** (CI): format, lint, build, test
2. **Code review**: 2 approvals required
3. **Merge**: Squash & merge (default) or rebase

---

## Coding Guidelines

### C++ Best Practices
- **RAII** everywhere (smart pointers, `std::unique_ptr`, `std::optional`)
- **No raw owning pointers** — use `std::unique_ptr`, `std::shared_ptr`
- **`[[nodiscard]]`** on all fallible functions
- **`noexcept`** on non-throwing functions
- **`constexpr`** where possible
- **Structured bindings** for tuple-like returns
- **Range-based for** + structured bindings
- **`std::span`** over pointer+size
- **`std::format`** for formatting (C++26) / `fmt` fallback

### Naming Conventions
| Kind | Convention | Example |
|--------|------------|---------|
| Classes/Structs | `PascalCase` | `MeshBoolean` |
| Functions/Methods | `camelCase` | `computeVertexNormals()` |
| Variables/Params | `camelCase` | `vertexCount` |
| Constants/Enums | `SCREAMING_SNAKE` | `MAX_VERTEX_COUNT` |
| Private members | `m_` prefix | `m_vertexCount` |
| Static constexpr | `k` prefix | `kDefaultTolerance` |
| Template params | `T` / `TArgs...` | `template<typename T>` |

### Header Guards
```cpp
#pragma once
// No traditional include guards
```

### Includes
```cpp
// Standard library first
#include <vector>
#include <string>

// External (alphabetical)
#include <imgui.h>
#include <vulkan/vulkan.h>

// Nexus (project order: kernel → app → external)
#include <nexus/geometry/Mesh.h>
#include <nexus/app/EditorUI.h>
```

---

## Testing Requirements

### New Features
- [ ] Unit test for core logic
- [ ] Integration test (if editor-facing)
- [ ] Property test (for math/geometry)
- [ ] Edge cases: empty, degenerate, extreme values

### Test Naming
```
<Component>_<Feature>_<Scenario>_<Expected>
```
Examples:
- `MeshBoolean_CubeUnionCube_Watertight`
- `HalfEdge_InsertEdgeVertex_IntegrityClean`
- `Tolerance_ScaleInvariance_CoincidenceAt1mmAnd1km`

### Property-Based Testing
```cpp
TEST(HalfEdge, InsertEdgeVertex_IntegrityClean) {
    auto hem = HalfEdgeMesh::fromMesh(mesh);
    ASSERT_TRUE(hem.has_value());
    
    for (uint32_t i = 0; i < 100; ++i) {
        hem.insertEdgeVertex(randomEdge(), randomFloat());
        auto report = hem.checkIntegrity();
        EXPECT_TRUE(report.ok) << report.reason;
    }
}
```

---

## Documentation Standards

### Code Comments
```cpp
/// @brief Brief one-line description.
/// @param[in] mesh Input mesh (must be valid)
/// @param[in] opts Remeshing options
/// @return Remeshed mesh, or empty on failure
/// @throws std::runtime_error on invalid input
[[nodiscard]] std::optional<Mesh> voxelRemesh(const Mesh&, const RemeshOptions&);
```

### Markdown Docs
- Use `#` for H1, `##` for H2, `###` for H3
- Code blocks: ```cpp``` with language
- Tables for options/parameters
- Cross-references: `[Mesh](mesh-operations.md)`

---

## Release Process

### Versioning
```
MAJOR.MINOR.PATCH
```
- **MAJOR**: Breaking API changes
- **MINOR**: New features (backward compatible)
- **PATCH**: Bug fixes only

### Release Checklist
- [ ] All CI checks pass
- [ ] Version bump in `CMakeLists.txt` + `package.json`
- [ ] `CHANGELOG.md` updated
- [ ] Git tag: `git tag -a vX.Y.Z -m "Release X.Y.Z"`
- [ ] GitHub Release with artifacts
- [ ] Docker image pushed (if applicable)
- [ ] Documentation deployed

---

## Communication

| Channel | Purpose |
|---------|---------|
| GitHub Issues | Bugs, feature requests, tasks |
| GitHub Discussions | Design questions, RFCs |
| Discord `#nexus-modeling` | Real-time chat, help |
| Email `security@nexus-modeling.dev` | Security issues only |

---

## License

All contributions are licensed under **Apache-2.0** (see `LICENSE`).

---

*Thank you for contributing to Nexus Modeling!* 🚀