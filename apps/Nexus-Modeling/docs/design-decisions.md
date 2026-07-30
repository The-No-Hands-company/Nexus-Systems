# Kernel Design Decisions

This document records the *why* behind each major technical choice in the graphics kernel. Future contributors and the Nexus-Cloud integration team should read this before proposing changes to the core.

---

## Vulkan-first, not cross-platform-first

**Decision**: The primary backend is Vulkan 1.4. Metal and DX12 backends exist in the layer map but are not implemented during the scaffold phase.

**Rationale**:
- Nexus Modeling's primary platform is Linux/Windows. Vulkan covers both natively.
- Vulkan 1.4 gives access to every modern GPU feature needed: mesh shaders, ray tracing, timeline semaphores, descriptor indexing, dynamic rendering.
- Vulkan's explicit model forces correct resource tracking up front — bugs are caught at validation layer time, not silently on some platforms.
- SPIR-V as the universal shader IR means Metal/DX12 backends can transpile from the same compiled shaders via SPIRV-Cross/DXC — no duplicate HLSL/MSL authoring needed.
- Apple's Metal has first-class Vulkan via MoltenVK, so iOS/macOS reach is not blocked.

**What this is not**: The `IDevice` abstraction is intentionally thin and backend-agnostic. Nothing in application code should depend on Vulkan types.

---

## Dynamic Rendering — no VkRenderPass

**Decision**: All Vulkan rendering uses `VK_KHR_dynamic_rendering` (`vkCmdBeginRenderingKHR`). No `VkRenderPass` objects are created at runtime.

**Rationale**:
- Render passes in Vulkan 1.0/1.1 were tile-based-GPU metadata. On all modern discrete GPUs, the tiling benefit is marginal; the cost is subpass/dependency boilerplate.
- Dynamic rendering is core in Vulkan 1.3+ and promoted from extension in 1.2.
- This simplifies the `IDevice` interface: `RenderPassHandle` in `GraphicsPipelineDesc` is a format-only descriptor, not a heavyweight Vulkan object.
- RenderDoc, Nsight, and all major capture tools fully support dynamic rendering.

**Trade-off**: Tile-based GPUs (mobile, some ARM) lose some implicit bandwidth optimisations. When a mobile Vulkan backend is added, a render-pass fallback path should be offered for VK_QCOM_render_pass_transform.

---

## Reversed-Z depth

**Decision**: Depth buffer uses reversed-Z — far plane maps to 0.0, near plane to 1.0. Compare op is `VK_COMPARE_OP_GREATER_OR_EQUAL`.

**Rationale**:
- Float32 depth precision is distributed logarithmically from 0.0. Reversed-Z places the dense precision region at distance (far), not at the camera (near), which is where z-fighting actually occurs.
- CAD and architectural modelling commonly have scenes spanning metres to millimetres in the same view — standard-Z z-fight visibly at even moderate camera distances.
- NVIDIA, AMD, and Intel all recommend reversed-Z in their developer guides.
- Cost: zero at runtime. Projection matrix is negated once in `Camera::projection()`.

---

## Typed handle system instead of pointers

**Decision**: All GPU resources are returned as `Handle<Tag>` — a 64-bit integer keyed into a backend slot-map. No raw pointers or `std::shared_ptr` cross the `IDevice` boundary.

**Rationale**:
- Handles are trivially copyable, storable in arrays, hashable, and safe to pass to Nexus-Cloud over IPC/JSON without pointer fixup.
- The slot-map (fixed-size array with free-list) gives O(1) alloc/free and pointer-stable storage with no heap fragmentation.
- Incorrect use (use-after-destroy, double-free) is caught at the slot-map level in debug builds — far cleaner than dangling pointer UB.
- `kNullHandle = UINT64_MAX` — safe sentinel distinct from index 0, no ambiguity.

**Pattern**: `handle.valid()` before any use. The backend checks in debug; in release it trusts the caller (no overhead).

---

## VMA for GPU memory management

**Decision**: `VulkanMemoryAllocator` (VMA) 3.1.0 is the only allocator used by the Vulkan backend.

**Rationale**:
- VMA is the de-facto standard for Vulkan memory management — used by Godot, Unreal, Filament, and hundreds of shipping titles.
- It implements sub-allocation, defragmentation, budget tracking (`VK_EXT_memory_budget`), and DEDICATED_ALLOCATION automatically.
- `VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT` enables BDA on all allocations, required for ray tracing and mesh shaders.
- A single `VMA_IMPLEMENTATION` translation unit (`VulkanAllocator.cpp`) prevents multiple-definition ODR issues.

**Configuration**:
```cpp
// Dynamic Vulkan function loading — no static link to libvulkan needed
VMA_STATIC_VULKAN_FUNCTIONS  0
VMA_DYNAMIC_VULKAN_FUNCTIONS 1
```

---

## Push constants over descriptor sets for per-draw data

**Decision**: `createGraphicsPipeline` always creates a `VkPipelineLayout` with a 128-byte push constant range covering all stages. Descriptor sets are used only for large/persistent data.

**Rationale**:
- 128 bytes covers a `mat4` (64B) + `vec4` + a few `uint` indices — sufficient for most per-draw data (transform, material index, draw ID).
- Push constants are register-backed on all modern Vulkan drivers — zero overhead, no descriptor pool management.
- `ICommandBuffer::pushConstants()` exposes this cleanly: `cb.pushConstants(stages, &data, sizeof(data))`.
- 128 bytes is the guaranteed minimum push constant space in Vulkan spec (many drivers give 256+).

---

## glslang runtime shader compilation

**Decision**: glslang is a FetchContent dependency that compiles GLSL to SPIR-V at runtime when `.glsl` source is provided.

**Rationale**:
- Artists and shader developers iterate fastest with hot-reload from source files.
- Pre-compiled `.spv` is the release path — zero runtime compile cost in shipping builds.
- `ENABLE_OPT=0` (no SPIRV-Tools): removes the SPIRV-Tools transitive dependency, which is large and has filesystem cleanup issues on some mount types (EXFAT). GLSL correctness validation is sufficient in glslang alone.
- The compiler is wrapped behind `nexus::gfx::vkshader::compileGlslToSpirv()` — callers never touch glslang headers directly.

**Warning**: Do not enable `ENABLE_OPT=1` without first verifying SPIRV-Tools archives build cleanly on your filesystem. The parallel `ar` tool corrupts archives on EXFAT/NTFS mounts.

---

## Neural plugins via runtime dlopen

**Decision**: DLSS4, XeSS, and OIDN are loaded at runtime via `dlopen`/`LoadLibrary`. None are linked at compile time.

**Rationale**:
- DLSS requires NVIDIA hardware. XeSS requires Intel hardware. OIDN runs on CPU. Zero reason to hard-link all three everywhere.
- Runtime loading means the binary ships without SDK redistributables and falls back gracefully on hardware that lacks a given feature.
- Build options `NEXUS_ENABLE_DLSS`, `NEXUS_ENABLE_XESS`, `NEXUS_ENABLE_OIDN` only control *whether to attempt loading* — they do not create link-time dependencies.
- The `nexus::neural::NeuralRenderer::create(device)` factory encapsulates all probing; application code sees only `INeuralRenderer`.

---

## Hardware tier system

**Decision**: Four tiers (Low / Mid / High / Ultra) gate feature activation at runtime. No features are hard-disabled at build time.

**Rationale**:
- A modelling suite must run on integrated graphics (students, low-cost workstations) as well as flagship workstation GPUs.
- Compile-time feature gates create multiple binaries to maintain. Runtime gates keep one binary with graceful degradation.
- The tier is computed once during `RenderContext::create()` from `DeviceCapabilities::vram` and feature presence.
- All code paths degrade cleanly — e.g., if `caps.meshShaders == false`, `IDevice::createMeshShaderPipeline` returns an invalid handle and the renderer falls back to indexed triangle lists.

| Tier | VRAM threshold | Key feature unlocks |
|---|---|---|
| Low | < 4 GB | Rasterisation only, bilinear upscale, OIDN CPU denoising |
| Mid | 4–8 GB | Ray queries, FSR3 |
| High | 8–16 GB | Full RT pipeline, mesh shaders, XeSS/DLSS |
| Ultra | > 16 GB | Neural texture decode, DLSS4 Ray Reconstruction |

---

## Async compute on a separate queue

**Decision**: Denoising, upscaling, and skinning compute workloads submit to a dedicated async compute queue when `caps.asyncCompute == true`.

**Rationale**:
- On AMD and NVIDIA, the async compute engine runs concurrently with the graphics pipeline — denoising overlaps with the next frame's geometry stage.
- Timeline semaphores (`VK_KHR_timeline_semaphore`) sequence the async compute queue relative to the graphics queue without CPU round-trips.
- When async compute is unavailable (single-queue devices), work falls back to the graphics queue inline — same `ICommandBuffer` API, different `QueueType` parameter.

---

## Single-pass GBuffer vs deferred

**Decision**: The high-level frame pipeline is designed for **deferred rendering** with a GBuffer pass, but the kernel itself is pipeline-agnostic.

**Rationale**:
- Nexus Modeling scenes are geometry-heavy (millions of mesh faces, hundreds of materials). Deferred shading allows arbitrary material complexity with a single geometry pass.
- The `Renderer` class composes the frame: GBuffer → lighting → post (TAA, denoise, upscale). The kernel (`IDevice`, `ICommandBuffer`) has no opinion on the composition.
- Forward+ is a valid alternative for lower-tier hardware — the abstraction layer does not prevent it.

---

# Geometry kernel — the trade-offs, and what each one costs

Everything below was chosen deliberately, and each entry names the alternative that was not
taken and the price of the one that was. Where a cost has actually been paid — a real defect
that traces back to the choice — it is recorded, because a trade-off with a measured price is
worth more than one described in the abstract.

## Exact topological decisions, floating-point geometric constructions

**Decision**: *Which side* is answered by exact predicates. *Where* is computed in floating
point against a tolerance.

**The alternative**: exact geometry throughout — rational or algebraic arithmetic on the
constructions themselves, so an intersection point is represented exactly rather than
approximated.

**Why not**: the degree explodes. The intersection of two exact quadric surfaces is an
algebraic number; feeding that result into another boolean compounds it, and after a few
operations the representation is unusable in both size and speed. Parasolid and ACIS made the
same call for the same reason.

**What it costs**: every constructed point carries error, so coincidence is a *judgement*
(the tolerance) rather than a fact. Most of the hard bugs in the boolean arc live exactly
there — two operands computing the same physical seam point independently and disagreeing.

**Price paid**: `-ffast-math` was enabled for years, which silently disabled the exact half
by folding the error-free transformations that Shewchuk's predicates are built from. Six
wrong signs in 5,675 cases, every one on exactly-coplanar input where the predicate must
return zero for Simulation of Simplicity to have a tie to break. This choice only works if
the exact half is genuinely exact, and nothing was checking.

## `float` coordinates — reversed, and executed

**Original decision**: positions, curves and surfaces were stored as 32-bit float. **This was
reversed.** The B-rep is now double throughout — storage, curve and surface parameters, all
fourteen primitive builders, the crossing solvers and the file format. The analysis below is
kept because it is the record of *why*, and because the measurement in it is what ruled out
the cheap alternative. See "What the migration actually cost" at the end for the outcome.

**The alternative**: double throughout, which is what Parasolid, ACIS, OCCT and Rhino all do.

**Why float was chosen**: half the memory on the largest arrays in the system, and it is the
format the GPU consumes, so mesh data reaches a vertex buffer without conversion.

**What it costs**: about six usable decimal digits. A part one kilometre across resolves to
roughly 0.06 mm and no finer, and cannot carry a micron feature at all. The tolerance model
is built correctly *for* float — `kDefaultRelative = 1e-6` is about 8 ULP at unit scale — but
that number is the ceiling, not a tuning choice.

**Measured, so the cheap fix can be ruled out**: computing constructions in double and
rounding once to float was implemented and measured against a long-double reference over
200,000 random circle evaluations:

| coordinate scale | float throughout (worst) | double then round (worst) | gain |
|---:|---:|---:|---:|
| 1 | 1.02e-07 | 8.31e-08 | 1.2× |
| 100 | 1.25e-05 | 8.81e-06 | 1.4× |
| 10,000 | 1.10e-03 | 6.87e-04 | 1.6× |

Under 2×, because rounding the *result* to float is the floor. **The ceiling is the storage,
not the arithmetic**, so the mitigation is not worth shipping and was not shipped. The only
real fix is double storage.

**Migration, sized rather than hand-waved.** The B-rep is separable from the renderer, which
is what makes this tractable:

- ~400 `Vec3` uses across five B-rep sources (`AnalyticBRep` 292, `BRepSurfaceIntersect` 32,
  `BRepBoolean` 26, `BRepFeatureStack` 9) plus 36 in the public header.
- Exactly **one** kernel source outside those files uses `brep::` types. 48 test files do,
  and most of their literals (`{1.f, 2.f, 3.f}`) convert to double implicitly.
- `toMesh` is already the render boundary: it produces a `Mesh` whose attributes are
  `render::Vec3`, so the float conversion happens in one place that already exists.
- Serialization is already versioned at v3 with a documented read-earlier-versions rule, so
  a v4 that writes doubles and still reads v1–v3 floats is the established pattern, not a
  new one.

Staged: introduce the scalar alias and the double vector; migrate `Surface` and `Curve`
(few instances, and they are the ground truth vertices are validated against); then
`Vertex`; then serialization v4; then re-tighten `Tolerance`'s defaults, which are currently
sized to float and are the visible symptom of the whole decision.

### What the migration actually cost

**Status: executed**, across three increments, at 2499/2499. The staging above held, with two
corrections the plan did not anticipate.

**The lever was one line.** `nexus::geometry` pulled `Vec3` in from the renderer with a
using-declaration, so pointing that declaration at a double-width vector migrated every
declaration in the namespace at once. `Vec3d` widens implicitly from `render::Vec3` and
narrows only through an explicit `toFloat()`, which makes the compiler enumerate the
geometry-to-renderer boundaries instead of letting them convert silently.

**Primitives migrate together or not at all.** Widening `makeCylinder` alone as a cautious
first step improved the cylinder (5.9e-8 → 4.4e-8) and made the *box worse* (≈6e-8 → 1.24e-7).
Two solids that must agree along a seam had been built at different widths, and agreement
between two approximations does not improve by fixing one of them. All fourteen went in one
commit. The same trap caught `Body::transform`, which multiplied double points through a
float `Mat4`: an identity translation perturbed every coordinate, and a boolean of two
identical cylinders returned empty.

**A relabelled current blob is not a compatibility fixture.** Backward-read had been checked
by taking a current blob and stamping an older version byte on it, which only ever worked
because every version shared one payload layout. Across a width change that produces a v4
payload with a v1 header, and decoding it as v1 reads doubles as floats. A genuine v3 blob is
now committed as bytes (`tests/kernel/fixtures/brep_v3_box.nxb`); it cannot drift with the
code, which is the point.

**The ceiling was not in any declaration.** With storage, parameters, primitives and the
format all double, a vertex still disagreed with its own curve by 4.371e-08 — single
precision — at every scale. The number identified itself: 0.5·sin(float π) is 4.3711390e-08.
The narrowing was in *return values and locals* three calls down — `float dot()`, `float
length()`, and a set of `float` locals holding arc parameters and crossing fractions. Every
arc parameter is `atan2(dot(w, bi), dot(w, ref))`, and `atan2` cannot return a double angle
from float arguments, so each was rounded on the way out of a three-line helper and then
stored into a double field where it looked wide. Twenty lines of fix, eight orders of
magnitude:

| scale | box | cylinder | relative |
|---:|---:|---:|---:|
| 1 | 4.371e-08 → 2.001e-16 | 4.371e-08 → 1.531e-16 | 2.0e-16 |
| 100 | 4.371e-06 → 1.589e-14 | 4.371e-06 → 1.531e-14 | 1.6e-16 |
| 10,000 | 4.371e-04 → 2.417e-12 | 4.371e-04 → 1.531e-12 | 2.4e-16 |

**Test precision claims at more than one scale.** A float in the chain is a *fixed relative*
error whose absolute size grows with the model; genuine double is a flat ~2e-16 at every
scale. A single-scale test cannot distinguish them, which is how this survived the whole
migration. `test_BRepPrecisionCeiling` measures rather than inspects types (the defect was
invisible in the types), asserts both operands (they were rounded by different helpers —
fixing one left the other entirely single-precision), asserts relatively across four orders
of scale, and keeps the π fingerprint, which is what caught the second half.

**`Tolerance` did not come with it, and the reason is not what it first appeared to be.** The
defaults were sized to float at ~8 ULP and the plan was to re-tighten them last. At 1e-6/1e-7
six tests fail. Five pin the constants themselves and would move with them. The sixth —
random box-through-sphere booleans across five tessellations — was written up here as a
float-storage problem: the mesh boolean runs on float positions, so the loose weld band
looked like a precision workaround that a mesh-side migration would remove.

**That was wrong, and probing it is what showed so.** At a tightened 1e-6, exactly one
configuration out of 300 leaks, under all three operators, and the points it cannot weld are
3.19e-06 apart — about 25× float epsilon at unit scale, far too large to be rounding. The
geometry is unambiguous:

```
v64  (-0.126088738, 0.526467443, 1.000000000)   seam crossing, on the box face and the sphere
v66  (-0.126070619, 0.526456952, 1.000000000)   seam crossing, on the box face and the sphere
v204 (-0.126087785, 0.526467562, 0.999996960)   a SPHERE VERTEX, 3.04e-06 below the face
```

All three lie on the sphere to within a micron of its 1.2 radius. At that random offset the
sphere grazes the box's top face closely enough that its tessellation drops a vertex three
microns under the plane, while the seam crossing for the same region lands exactly on it.
Whether those are one point or two is not a precision question — it is the coincidence
decision itself, and the tolerance is the parameter that makes it.

So the band is a **modeling scale, not a numerical workaround**. Widening the mesh boolean's
arithmetic would not move it by one bit, because the distance being bridged is real.
Tightening it does not clean anything up either; it redefines what "the same point" means,
and a grazing sphere then cannot close its seam. That is a deliberate decision about what
this kernel considers coincident at a given model size — available to make, but it belongs
to whoever is willing to say that three-micron features in a two-unit model are distinct,
and it buys nothing on its own. `test_MeshBooleanGrazingCoincidence` pins the configuration
and the measurement so the reason survives.

> Verify that an epsilon's problem is *reachable and fixable* before migrating it. This one
> is neither: the tolerance is doing exactly the job it exists for.

## Watertight-or-empty as a hard contract

**Decision**: `booleanToBody` returns a valid closed solid or an empty body. Never a leaky or
corrupt one.

**The alternative**: best-effort output, with a diagnostic — what most mesh booleans do.

**Why**: a corrupt solid is silent and travels. It becomes the input to the next operation,
and the eventual failure is arbitrarily far from the cause. An empty result is loud, local,
and testable, and it makes "did this configuration work" a property a test battery can assert
over thousands of cases.

**What it costs**: a great many configurations return nothing rather than something nearly
right — tangent cylinders, chained-left booleans, and every arc-bite seam until recently.
That is a real usability cost and it is chosen deliberately.

## Two geometry worlds — analytic B-rep and mesh — with converters

**Decision**: the analytic solid and the triangle mesh are separate representations joined by
`toMesh` / `fromMesh`, not a single unified one.

**The alternative**: one representation, or a representation-agnostic trait that both
implement.

**Why**: they answer different questions. A B-rep answers *exactly* and must be watertight; a
mesh answers *approximately* and must be fast and GPU-shaped. Neither Parasolid nor OCCT has
a universal representation trait either — a dominant B-rep with mesh output is the industry
shape, and premature abstraction across two things that differ this much is its own failure
mode.

**What it costs**: duplicated concepts, and the conversion is lossy in one direction. Worth
blessing explicitly as intentional rather than leaving as an unexamined default.

## Manifold-only half-edge

**Decision**: `HalfEdgeMesh` enforces manifoldness and refuses non-manifold input.

**The alternative**: a non-manifold-tolerant structure, which is what Blender's BMesh is.

**Why**: it is what makes the six Euler operators provable and the watertightness guarantees
mean anything.

**What it costs**: polygon editing routinely passes *through* non-manifold states — a wire
edge, three faces on an edge mid-operation, a bowtie the user is about to fix — and this
structure cannot hold them. The answer is not to weaken the manifold core but to be explicit
that there are two tiers with a defined promotion path. Currently the tolerant tier is
absent.

## A separate analytic mass-properties path

**Decision**: `massProperties` integrates curved faces over their parameter domain by
Gauss-Legendre rather than trusting the tessellation, and only falls back to triangles when a
face does not qualify.

**The alternative**: one implementation — measure everything from the mesh.

**Why**: chords cut the corner. Tessellating cost about 0.65% of a sphere's volume and around
1% of its moments, and an analytic modeller should not be approximate about its own analytic
primitives.

**What it costs**: two implementations of the same quantity, which can disagree — normally a
smell. Here it has twice been the most valuable property in the system: the disagreement
between the two is what exposed a reversed-curved-face bug that every topological invariant
passed, and what proved the union was correct when the tessellated oracle said otherwise.
**Independent oracles are worth their duplication.**

## `Surface::normal` means "normal" for a plane and "axis" for a cylinder

**Decision**: one field, two meanings, chosen so the serialised layout did not have to grow
when cones were added.

**The alternative**: separate `normal` and `axis` fields, or a per-kind variant.

**What it costs**: a bug that hid for a long time. The boolean reversed a face by negating
that field — correct for a plane, and for a cylinder it re-parameterises the surface while
reversing nothing. It survived because the tessellator derives its normal as *(axis, flipped
if reversed)*, so a flipped axis with the flag unset and a true axis with the flag set give
the same answer and the two errors cancel exactly. Only the analytic integrator, which needs
the axis and the flag for different purposes, could see it.

**The general lesson**: an overloaded field is a defect that stays hidden until something
reads only one of its meanings. If this is ever revisited, splitting the field is cheap now
and gets more expensive per serialised format version.

## Fan triangulation of a face

**Decision**: a face with a convex boundary is triangulated by a fan from its first vertex.

**Why**: it is the cheapest correct triangulation, and every face *was* convex — a straight
imprint splits convex into convex, and an interior circle takes the hole path.

**What it costs**: the assumption became false the moment an arc bite produced a concave
face, and three separate defects came through that door — a face double-covering the lens cut
from it, zero-area triangles from fanning collinear refined points, and a curved patch whose
interior is never sampled so its volume converges to the wrong number. The fan is also why
`toMesh` currently depends on degenerate triangles for its watertightness. See
`tomesh-curved-tessellation.md`.

## Single-threaded, deliberately

**Decision**: no threads anywhere in the kernel, by choice, until correctness is settled.

**Why the sequence is right**: parallelising code whose predicates were deciding
exactly-degenerate cases by coin flip would have turned a reproducible wrong answer into an
irreproducible one.

**What it costs**: roughly 94% of a modern workstation sits idle. This is a deferral with a
condition attached, not a position — and the condition (exact predicates, verified) is now
met.

## Hand-picked test configurations, plus one fuzzer

**Decision**: batteries of specific, reasoned configurations, with a seeded fuzzer added
later for the unknown-unknowns.

**What it costs**: a test can only see what its author imagined. The exactness battery is the
cautionary case: its coordinate bound was chosen so its *own* reference stayed exact in
64-bit integers, and that same bound kept the determinant below what a double holds — so it
validated the predicate precisely in the regime where any implementation passes. The bound
that made the oracle trustworthy is the bound that made the defect invisible.

**The habit that came out of it**: when an oracle and its subject share a path, the oracle
proves nothing. And for anything that changes how a face is bounded, assert **unsigned area** —
signed volume cancels duplicate triangles, and every topological invariant in this kernel
will certify a face that is geometrically wrong.
