# Contributing to Nexus Modeling

Thanks for helping build Nexus Modeling. This project is a C++26 Vulkan-first geometry/rendering kernel with strict correctness and determinism requirements.

## Before You Start

1. Read [README.md](README.md) and [docs/README.md](docs/README.md).
2. Check [ROADMAP.md](ROADMAP.md) for current milestone priorities.
3. Pick or create a GitHub Issue before opening a pull request.

## Development Rules

- Keep changes focused and reversible.
- Do not mix unrelated refactors into feature or bug-fix PRs.
- Preserve public API stability under `src/kernel/include/nexus`.
- Keep Null backend parity for interface and behavior changes.
- Treat synchronization, transitions, and lifetimes as first-class concerns.
- Follow the **Clear Separation of Concerns** rule: every visual, interactive, or behavioral unit gets its own header+source pair (see AGENTS.md).

## Build and Test (Required)

Run from repository root (`apps/Nexus-Modeling/`):

```bash
# Configure (first time or after CMakeLists changes)
cmake -S . -B build -DNEXUS_BACKEND_VULKAN=ON -DNEXUS_BACKEND_NULL=ON

# Build
cmake --build build -j$(nproc)

# Test (all 2026 tests, headless via Null backend)
ctest --test-dir build --output-on-failure
```

If you cannot run a required gate, state exactly what was skipped and why in the PR.

### Quality Gates (Every Commit)

```bash
# Must pass locally before pushing
cmake --build build -j$(nproc)          # -Werror, no warnings
ctest --test-dir build --output-on-failure  # 2026+ tests, 0 regressions
```

Pre-existing allowed failure: `ApiFreezeAudit.PublicHeaderManifestMatchesWorkspace`.

### Fast Iteration

```bash
# Build only test binary
cmake --build build --target nexus_kernel_tests -j$(nproc)

# Run specific test suite
./build/tests/nexus_kernel_tests --gtest_filter="MeshBoolean.*"

# Run specific test
./build/tests/nexus_kernel_tests --gtest_filter="MeshBoolean.CubeUnionCube_Watertight"
```

## Definition of Done

A contribution is done when **all** of the following are true:

1. Behavior is implemented and scoped to a single concern.
2. Tests are added or updated for behavior/regression coverage (unit + property-based).
3. Build and test gates pass locally, or skips are documented.
4. Relevant documentation is updated (code comments, docs/, README).
5. No secrets/tokens/credentials are introduced.

## Pull Request Checklist

- [ ] Describe user-visible behavior changes.
- [ ] Call out synchronization/resource-lifetime implications.
- [ ] Link the issue and tests covering the change.
- [ ] Include residual risks and untested paths.
- [ ] Complete the required `Kernel Capability Declaration` section in `.github/PULL_REQUEST_TEMPLATE.md`.
- [ ] Update documentation (AGENTS.md, CLAUDE.md, docs/) if architecture changes.
- [ ] Run `clang-format` on changed files.

## Review Style

Reviews prioritize findings in this order:

1. **Correctness regressions** — logic errors, edge cases, numerical stability
2. **Synchronization and resource-lifetime risks** — Vulkan barriers, lifetimes, memory leaks
3. **API contract breaks** — public header changes, serialization format changes
4. **Missing or weak tests** — property tests, determinism, regression coverage

Use concrete file/line references and keep summaries secondary to findings.

## Code Style

- **C++26** (modules, concepts, contracts where available)
- **clang-format** (LLVM style, see `.clang-format`)
- **clang-tidy** (modernize, performance, bugprone)
- **Warnings as errors** (-Werror)
- **No vendor names** in code, comments, filenames (no Parasolid, ACIS, SolidWorks, etc.)
- **No marketing buzzwords** (no "Smart", "Advanced", "Production" in filenames)
- **No abbreviations** in public API (spell out: `CadProductManufacturingInfo`, not `CadMBD`)
- **Files named by domain**, not perceived quality (`CadAutoConstraintSketch`, not `SmartSketch`)
- **One header → one concern**. Avoid multi-group junk-drawer headers.
- `[[nodiscard]]` everywhere — cast with `(void)` when intentionally ignoring

## Architecture Constraints

### Layer Dependency Rule
```
nexus_modeling (app) → nexus::app → nexus::cad → nexus::geometry + nexus::parametric → nexus::gfx
```
No upward dependencies.

### Public API Boundary
- Public: `src/kernel/include/nexus/<module>/`
- Internal: `src/kernel/src/<module>/`
- Never expose internal headers publicly

### Determinism Requirements
- Same input → bit-identical output
- No `-ffast-math`, strict IEEE-754
- Fixed iteration order, stable sorts, deterministic hashing (xxHash)
- Null backend = CI reference

### Null Backend Parity
All interface changes must work on Null backend. Feature flags gate.

## Testing Standards

- **Unit tests**: GoogleTest in `tests/kernel/`
- **Property tests**: RapidCheck-style invariants (Euler ops, Boolean commutativity)
- **Integration**: Renderer offscreen, viewport picking
- **Perf regression**: `nexus_kernel_perf_smoke` benchmarks
- **Headless CI**: All tests run via `xvfb-run` + llvmpipe

### Test Naming
```cpp
TEST(Component, Scenario_ExpectedBehavior) { }
TEST(HalfEdgeMesh, InsertEdgeVertex_IntegrityClean) { }
TEST(MeshBoolean, CubeUnionCube_Watertight) { }
```

## Documentation

Update in same PR:
- `docs/developer/` — architecture, kernel internals, renderer, adding ops/features
- `docs/GEOMETRY_KERNEL_COMPENDIUM.md` — technology bible
- `README.md` — if user-facing change
- `ROADMAP.md` — if milestone changes

## Getting Help

- Check existing issues/PRs
- Read `AGENTS.md` for architecture rules
- Read `CLAUDE.md` for build/test workflow
- Ask in issue comments or Discord (#nexus-modeling)

---

*Thanks for contributing to the alternative to Blender, Maya, 3ds Max, Modo, Houdini, Plasticity, and Rhino.*