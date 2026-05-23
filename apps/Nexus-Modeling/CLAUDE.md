# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

Nexus-Modeling is the geometry/rendering DCC application — a Vulkan-first **C++23**
kernel — and is one app inside the larger `Nexus-Systems` monorepo (repo root is
`../../`, sibling apps live under `apps/`). Treat this directory as the unit of
work: **commit only `apps/Nexus-Modeling/` paths**, never the local `.claude/`
dir or sibling apps. See `AGENTS.md` for the contributor/agent contract and
`docs/` for design, shader, and testing-strategy docs.

## Build & test

All commands run from `apps/Nexus-Modeling/`. There is a pre-configured `build/`.

```bash
cmake -S . -B build                                          # configure (first time)
cmake build                                                  # RE-configure after editing any CMakeLists.txt
cmake --build build --target nexus_kernel_tests -j$(nproc)   # fast iteration: build only the test binary
cmake --build build -j$(nproc)                               # full build
ctest --test-dir build --output-on-failure                   # run all tests + perf smoke via ctest
./build/tests/nexus_kernel_tests                             # run the gtest binary directly (faster)
./build/tests/nexus_kernel_tests --gtest_filter='SimulationCore.*'   # one suite
./build/tests/nexus_kernel_tests --gtest_filter='SimulationDriver.FrameRate*'  # one test
```

- The default build type is `RelWithDebInfo`; the Null (headless) backend is built
  by default so the full suite runs without a GPU. Vulkan backend is on by default
  but RT/mesh-shader paths only execute on capable hardware (a few tests skip).
- `tools/release_gate_alpha.sh` is the alpha release/signoff gate (see README).

## Non-obvious rules that will bite you

- **`-ffast-math` is enabled** (and `-march=native`). `std::isfinite` / `std::isnan`
  are therefore unreliable (the compiler assumes operands are finite), and `1/sqrt`
  is an approximation. Detect non-finite values via IEEE-754 **bit inspection** and
  short-circuit exact endpoints in normalization — see the helpers in
  `src/kernel/src/sim/SimulationCore.cpp` and `SimulationDriver.cpp`.
- **Warnings are errors**: `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang), `/W4 /WX`
  (MSVC). A warning fails the build.
- **API freeze audit**: the `ApiFreezeAudit.PublicHeaderManifestMatchesWorkspace`
  test enforces an exact match between every header under
  `src/kernel/include/nexus/**/*.h` and the manifest at
  `tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt`. Adding/removing a
  public header **requires** updating that fixture or the suite fails.
- **Public vs internal headers**: `src/kernel/include/nexus/` is the public API
  surface; headers under `src/kernel/src/` are internal — never expose them publicly.
- **Input hardening**: public API entry points reject non-finite float inputs
  (pervasive convention; see the `harden:` git history). New numeric APIs should too.
- **Determinism**: behavior must be reproducible on the Null backend. Snapshot /
  serialization byte formats are **versioned with backward-compatible reads** (e.g.
  `serializeSimState` v2 still decodes v1) — bump the version, don't break old blobs.
- **Reversed-Z** depth convention is assumed throughout the renderer.

## Adding code (wiring checklist)

- New kernel `.cpp` → add to the relevant target in `src/kernel/CMakeLists.txt`
  (`nexus_gfx_core` for core kernel), then re-run `cmake build`.
- New public header → also add its path to the API-freeze manifest fixture.
- New test file → add to `tests/CMakeLists.txt` (`nexus_kernel_tests` sources).
  Tests are GoogleTest; `NEXUS_TESTS_ROOT` is passed as a compile def so tests can
  locate `tests/kernel/fixtures/`.

## Architecture (the big picture)

Three build targets form the kernel:
- **`nexus_gfx_core`** (STATIC) — the whole CPU-side kernel: geometry, render,
  sim, parametric, eval, scene, asset, animation, automation, memory, and the
  backend-agnostic `gfx/` abstraction (`Device`, `RenderContext`, `CommandBuffer`).
- **`nexus_backend_vulkan`** (STATIC) — Vulkan 1.4 implementation of the `gfx/`
  abstraction, with VMA and glslang (GLSL→SPIR-V) fetched at configure time.
- **`nexus_backend_null`** (INTERFACE) — headless no-op backend; the CI/test path.
  Plus `nexus_neural` (DLSS4/XeSS/OIDN glue, optional).

**Backend abstraction.** `gfx/` defines abstract `Device`/`CommandBuffer`/etc.;
backends implement them. A backend is selected via `RenderContextDesc::preferredBackend`
(`Backend::Null` for tests). `caps().rayTracingTier` / hardware tier gate feature paths.

**Render path.** Rendering is **scheduler-driven and deferred** (Shadow → Geometry/
GBuffer → Composite, with an optional RayTracing pass). `RenderGraphValidator`
checks per-pass texture layout transitions; `FrameCaptureExporter` records pass
metadata for diagnostics. The non-scheduler path is a minimal compatibility
fallback, **not** feature-parity — land new rendering features on the scheduler
path first, with explicit transitions and tests (see README "Render path policy").

**Scene.** `render::SceneGraph` holds `Node`s with a decomposed `Transform`
(translation `Vec3`, rotation `Vec4` quaternion xyzw, scale `Vec3`), mesh refs
(vertex/index/meshlet buffers, BLAS), and a TLAS for RT.

**Simulation (`sim/`).** `RigidBodySolver` is a deterministic fixed-step integrator
(explicit Euler; gravity + force; now also orientation + angular velocity + torque)
with snapshot/replay (`captureState`/`restoreState`/`serializeSimState`). Higher-level
pieces: `FluidSolver`, `ClothSolver`; `SimulationSceneCoupling` writes solver body
state onto bound scene nodes; `SimulationDriver` decouples the fixed solver rate
from the render frame rate via a fixed-timestep accumulator + state interpolation
(lerp position, nlerp orientation) — the canonical "Fix Your Timestep" pattern,
**not** a per-frame `step → apply`.

Public entry points are per-subsystem headers under `include/nexus/` plus
`include/nexus/Kernel.h`.
