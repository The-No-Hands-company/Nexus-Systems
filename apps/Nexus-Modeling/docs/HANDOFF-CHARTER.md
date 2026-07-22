# Nexus-Modeling — Session Handoff & Development Charter

> Purpose: let ANY AI session resume kernel/foundation work with zero context loss.
> Read top-to-bottom once, then use as reference. Everything here is verified from
> real work in this repo unless explicitly marked "inferred" or "NEEDS USER INPUT".
>
> Maintainer: Eric Håkansson (`erichakansson84@gmail.com`).
> Last updated by an AI session during the foundation-first loop (~increment 67).

---

## 0. THE 30-YEAR VISION — where Nexus-Modeling is going (OWNED BY ERIC — NOT YET WRITTEN)

**This is the single most important section and it is deliberately blank.**

Eric's actual answer to *"where do I see Nexus-Modeling in 30+ years?"* — the
decades-long plan, the end-state it is being built toward — **is not yet recorded
anywhere in this repo or in AI memory.** Do NOT invent it. Do NOT substitute the
app-UX roadmap or the parity checklist for it — those are near-term execution docs,
not the vision. When Eric is available, ask him to dictate the 30-year vision and
transcribe it here verbatim. Until then this section stays an explicit TODO so no
session mistakes a roadmap for the North Star.

```
[ 30-YEAR VISION — to be dictated by Eric and pasted here verbatim ]
```

Everything below (roadmaps, phases, gap audits) is *execution in service of* whatever
that vision turns out to be. If the vision as written ever contradicts a roadmap
below, the vision wins and the roadmap gets rewritten.

**Inferred long-term direction (INFERENCE ONLY — a placeholder to confirm/replace,
NOT Eric's stated plan):** one coherent creative suite (model → sculpt → paint →
engine) sharing selection/history/UI grammar; a credible alternative to
Blender/Maya/3ds Max/Modo/Houdini/Plasticity that matches the standard on the core
and wins on onboarding + the seams between tools; zero-tutorial context-sensitive UX;
an AI visual-development loop as a first-class capability; a kernel carrying all four
geometry paradigms (B-rep / half-edge / voxel-SDF / brush-plane-CSG).

---

## 1. What Nexus-Modeling is

A **Vulkan-first, C++26 geometry/rendering DCC kernel** — one app inside the
`Nexus-Systems` monorepo. Repo root is two levels up; sibling apps live under
`apps/`. **Treat `apps/Nexus-Modeling/` as the unit of work.**

Three build targets form the kernel:
- `nexus_gfx_core` (STATIC) — the whole CPU-side kernel (geometry, render, sim,
  parametric, eval, scene, asset, animation, automation, memory, and the
  backend-agnostic `gfx/` abstraction: `Device`, `RenderContext`, `CommandBuffer`).
- `nexus_backend_vulkan` (STATIC) — Vulkan 1.3+ impl (VMA + glslang, fetched at
  configure time).
- `nexus_backend_null` (INTERFACE) — headless no-op backend; the CI/test path.
- `nexus_neural` — DLSS4/XeSS/OIDN glue (optional).

The CSG spine (the pipeline most active work touches):
`TriTriIntersect → TriangleRetriangulate (→ ConstrainedDelaunay2D) → MeshCut → robustMeshBoolean`

---

## 2. Build & test (from `apps/Nexus-Modeling/`)

```bash
cmake -S . -B build                                          # first-time configure
cmake build                                                  # RE-configure after ANY CMakeLists edit
cmake --build build --target nexus_gfx_core -j$(nproc)       # just the core lib (fast probe loop)
cmake --build build --target nexus_kernel_tests -j$(nproc)   # just the test binary
./build/tests/nexus_kernel_tests                             # run all tests (fast path)
./build/tests/nexus_kernel_tests --gtest_filter='Suite.*'    # one suite
ctest --test-dir build --output-on-failure                   # tests + perf smoke
```
- Default build type `RelWithDebInfo`; Null backend built by default (full suite runs
  GPU-less; a few Vulkan RT/mesh-shader tests SKIP without hardware).
- `lld` fast linker is available and worth using; large test-binary links are slow.
- Scratch/probes go in `$CLAUDE_JOB_DIR/tmp`, NOT `/tmp` (parallel jobs clobber it).
- Compile a probe against the built lib:
  `g++ -std=c++26 -I src/kernel/include -I src/kernel/src probe.cpp build/src/kernel/libnexus_gfx_core.a -o probe`

**Baseline to preserve (as of this handoff): 2279 pass / 5 GPU-skip / 0 fail.**

---

## 3. Rules that WILL bite you (non-negotiable)

- **`-ffast-math` and `-march=native` are DISABLED.** `std::isfinite`/`isnan` are
  reliable; the compiler does NOT assume finiteness.
- **Warnings are errors:** `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang), `/W4 /WX`
  (MSVC). One warning fails the build. GCC-15 `-Werror=maybe-uninitialized` is
  aggressive — zero-init float min/max locals; `std::max({...})` needs `<algorithm>`.
- **API-freeze audit:** `ApiFreezeAudit.PublicHeaderManifestMatchesWorkspace` enforces
  an exact match between every header under `src/kernel/include/nexus/**/*.h` and
  `tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt`. The manifest tracks
  **header PATHS only** — adding/removing methods on an existing header needs NO
  fixture change; adding/removing a header FILE requires updating the fixture.
- **Public vs internal:** `src/kernel/include/nexus/` is the public surface;
  `src/kernel/src/` headers are internal — never expose them publicly.
- **Non-finite hardening (pervasive convention):** public numeric APIs reject NaN/±Inf.
  Mesh-returning op → return empty `Mesh{}`; optional-returning → `nullopt`;
  report-returning → decline (`ok=false`). Use `geometry::isFinite` (`Tolerance.h`).
- **Determinism** on the Null backend is mandatory. Serialization byte formats are
  versioned with backward-compatible reads — bump the version, don't break old blobs.
- **Reversed-Z** depth convention throughout the renderer.
- **Vec3 is `nexus::render::Vec3`.** In `geometry`/`cad` sources add
  `using Vec3 = nexus::render::Vec3;` or fully qualify.
- **`[[nodiscard]]` everywhere** — cast intentional ignores with `(void)`.
- `SceneGraph` is non-copyable (holds `unique_ptr`). `CadAutoConstraintSketch` has a
  reference member (use `unique_ptr`). `MeshAttributes::positions()` returns a
  `const&` (copy → mutate → `setPositions()`).

Wiring a new file:
- new kernel `.cpp` → add to `src/kernel/CMakeLists.txt` (alphabetical within its
  module; `nexus_gfx_core` for core) → re-run `cmake build`.
- new public header → also add its path to the API-freeze manifest fixture.
- new test → add to `tests/CMakeLists.txt`. Forgetting causes linker errors with no
  obvious cause.

---

## 4. The operating mode: the FOUNDATION-FIRST LOOP

The session runs a self-paced loop. **Each increment lands ONE proven,
committed+pushed kernel/foundation change, then schedules the next** (ScheduleWakeup
~420s).

### Standing user directives (in force until Eric changes them)
1. **DEFINITIVE DIRECTIVE (2026-07-19):** *kernel/foundation work ONLY; the MATH —
   every combinatorial topological DECISION — must be enterprise-level exact, zero
   room for error. NO editor wiring, NO creation-op breadth.* Prioritize
   exactness/correctness DEPTH.
2. **Close EVERY foundation gap**, governed by the sequenced plan (§7).
3. **Commit ONLY `apps/Nexus-Modeling/` paths YOUR increment changed.** Never sibling
   apps, never `.claude/`, never the concurrent-WIP files. Use
   `git add <specific files>`, **NEVER `-A` / `.`**; verify with
   `git --no-pager diff --cached --stat` BEFORE committing.
4. **Commit trailers (required):**
   ```
   Co-Authored-By: Claude Opus 4.8 (1M context) <noreply@anthropic.com>
   Claude-Session: https://claude.ai/code/session_014cNwNWrGZpjvnMVPsFyKAQ
   ```
5. **THE GATE:** before the loop transitions from foundation work to breadth/feature
   work, STOP and flag Eric so he can request "one more foundation sweep." Never
   silently roll into feature work.

### ⚠️ Concurrent-WIP caution (critical)
A concurrent linter/agent has UNCOMMITTED edits in `CurveCurveIntersect.cpp`,
`CurveFairing.cpp`, `CotangentLaplacian.h`, `BezierCurve.cpp`, `CurveExtrude.cpp`
(+ build-fixes). These are NOT yours. `autosave.nxm` and other untracked files also
exist. **Never `git add` them.** Editing a widely-included header (e.g.
`AnalyticBRep.h`) recompiles those files and can surface their latent build failures —
fix minimally to iterate if needed, but commit ONLY your own files.

---

## 5. The methodology that actually works (hard-won)

- **PROBE-FIRST, always.** Before assuming a mechanism, write a small probe compiled
  against `build/src/kernel/libnexus_gfx_core.a` to pin the EXACT break. This has
  repeatedly overturned wrong assumptions (the "T-junction" misdiagnosis; the "just
  share the seam" framing that was a whole layer too high). Probe DOWN through each
  layer (cut → segments → retriangulate → CDT) — the real crack is often two layers
  below where the symptom shows.
- **Metric vs combinatorial is THE distinction.** *Metric* (distance, coincidence,
  weld) may be tolerant. *Combinatorial* (which-side, inside/outside, does-X-cross-Y,
  which-triangle-shares-which-edge) MUST be EXACT — use `RobustPredicates`
  `orient2D/orient3D/inCircle` + Simulation of Simplicity. **Never introduce a
  tolerance where the decision is combinatorial.** A geometrically-WRONG watertight
  result is worse than an honest hole — forbidden.
- **Build the tool that finds bugs first.** The seeded invariant fuzzer
  (`test_KernelFuzz`, deterministic on Null) has found multiple real bugs (2 HEM
  corruptions, a heap double-free, non-finite leaks, a boolean hang). Fuzz invariants:
  watertight-or-empty, integrity-or-refuse, determinism, non-finite rejection.
- **Prove before you claim.** Every "done" needs a green test. Full suite green,
  determinism preserved, before commit.
- **Scale-aware tolerance:** `Tolerance{}.at(L) = max(1e-5, 1e-6·L)`; make area/length²
  thresholds scale-INVARIANT relative to the primitive's own size, not a fixed
  absolute. VERIFY an epsilon's bug is reachable with a probe BEFORE migrating it.
- **Honest logging.** If a change is leak-neutral churn, don't land it — REVERT. A
  failed attempt often points at the real bug (inc66: the failed overlap-imprint
  attempt is what exposed the CDT constraint-enforcement bug).
- `MeshCut.cpp` / `MeshBooleanRobust.cpp` / `ConstrainedDelaunay2D.cpp` are `.cpp`
  (no header cascade) — safe to edit without triggering wide recompiles.

---

## 6. Architecture: the layers, and the CORRECT wiring

Clean layering (kernel = pure math/topology; core = data structures + dep graph;
engine = tools + IO + presentation):

1. **core / data structures** — `Mesh` (indexed n-gon, SoA attributes),
   `HalfEdgeMesh` (manifold, Euler operators), `AnalyticBRep`. Present but
   partly DOUBLED/fractured.
2. **math** — `RobustPredicates` (adaptive-exact orient/incircle), `TriTriIntersect`,
   NURBS eval. **Strongest layer — the real bedrock, not stubbed.**
3. **spatial** — `MeshBVH` (SAH, parallel build/refit/traverse), snapping. Present.
4. **ops / topology** — booleans, remesh, bevel/extrude/inset/bridge. **Where the
   defects live.**
5. **scene / eval** — `FeatureHistory` / dependency graph, `EvaluationGraph`,
   modifier stack. Regeneration is thin.
6. **render bridge** — `Renderer` + render bridge flatten to GPU buffers; deferred
   scheduler-driven Vulkan path (Shadow → GBuffer → Composite, optional RayTracing).

**Key insight: the foundation gaps are CONNECTIONS between layers that were never
wired, not missing layers.** The three propagation edges:
1. **boolean ↔ intersection** — wired (CSG splits along the intersection curve).
2. **mutators ↔ half-edge** — Bevel/Extrude/Inset/EdgeBridge still operate on raw
   index arrays, not the half-edge; migration pending.
3. **everything ↔ tolerance** — ~180 scale-blind per-file epsilons need routing
   through `Tolerance.h`. This is the true "Layer 0" spine beneath RobustPredicates,
   and the single most important unfinished foundation piece.

---

## 7. The gap audit & phased plan (governing roadmap)

Five axes: robustness, performance, topology guarantees, extensibility, testing.

- **PHASE F — FIND (fuzz harness):** ✅ DONE (`test_KernelFuzz`).
- **PHASE T — TOLERANCE spine:** started; only ~2 of ~180 epsilons migrated. HIGH
  PRIORITY. Migrate module-by-module, highest-traffic first; each with a two-scale
  test (behaves the same at 0.5 mm and 5 km).
- **PHASE M — MESH watertightness (shared-seam):** in progress. See §8/§9.
- **PHASE R — REPAIR + input hardening (non-finite):** ~complete across the mesh-op
  surface.
- **PHASE X — representation-abstraction DESIGN decision:** pending (kernel trait vs.
  blessed dominant-representation + converters). Cheap, high-clarity; a doc, not code.
- **PHASE P — PERFORMANCE:** last (BVH-accelerate point-in-solid & MeshCut broad-phase;
  parallelize classify; voxel/SDF sparse hierarchy pulled earlier only if the sculpt
  world becomes priority).

Also pending: half-edge authoritative core — the audit found 5 of 6 HEM local ops
corrupted topology; fixes ongoing, then migrate the raw-index mutators onto it.

---

## 8. Where PHASE M stands (the live arc)

**Corrected map (probe-proven):** each operand's cut surface is INDIVIDUALLY watertight
— the leak is purely at the kept-A/kept-B seam junction. Residual leaks are HOLES +
non-manifold EDGES (+ formerly bowtie vertices, now eliminated). They are ONE wound:
MeshCut cut the two operands along the seam and the downstream retriangulation couldn't
hold the shared line.

**inc66 (commit `ec738c80`) — the big win:** fixed `ConstrainedDelaunay2D` constraint
enforcement. Old code flipped crossing edges BLINDLY (no convexity check → folded
triangles over each other) and never split a constraint at a vertex lying on it. Fix:
(1) split a constraint at the nearest on-segment vertex, recurse; (2) recover by
flipping ONLY strictly-convex quads whose flip makes progress (Anglada termination);
(3) `flipEdge` emits CCW. All exact `orient2D`. Result: CDT fuzz 0 inverted (was ~14%),
unenforced 3706→34; **mesh-boolean torture leaks HALVED 30→15**; bowtie class gone.

**inc67 (in progress at rate-limit):** retried the symmetric shared-overlap-polygon
imprint in `MeshCut` — now T-junction-free (thanks to the CDT fix) but **leak-neutral
(15→15)**, so it was REVERTED (don't land churn). Pivoting to the `buildDelaunay`
hull-underfill gap (see §9).

Remaining boolean residual (15): extreme-degenerate coplanar (dx≈0, dx≈2) + curved
box-cyl facets + the coplanar-cap/side-wall 3-way junction.

---

## 9. Current baseline & how to resume RIGHT NOW

- Suite: **2279 pass / 5 GPU-skip / 0 fail.**
- Torture (`$CLAUDE_JOB_DIR/tmp/probe_meshbool.cpp`): **15 leaks / 8 empty / 0 nondet.**
- `test_MeshBooleanSeamCharacterization`: leaks ≤ 9, nmVertex == 0, tJunctions == 0.
- Working tree: `MeshCut.cpp` reverted to baseline (inc67's overlap attempt undone).
  The concurrent-WIP Curve*/Bezier/Cotangent files remain uncommitted — leave them.

**Next task (inc67 option B — the `buildDelaunay` hull-underfill gap):**
Probes prove `ConstrainedDelaunay2D::buildDelaunay` leaves ~3% of random inputs
(506/16k) with `Σ|tri area| < convex-hull area` — missing triangles, with NO
constraints involved. A separate, isolated, provable Bowyer-Watson cavity/boundary
bug (suspects: the cavity-boundary edge extraction that keeps edges used exactly once,
around `ConstrainedDelaunay2D.cpp` lines ~111–137, or the super-triangle removal).
Small failing cases are already captured (`probe_gapfind.cpp`, e.g. n=5 pts
`{1.992,-4.040} {-0.151,-2.810} {-0.848,-2.413} {1.387,-0.319} {2.593,-0.406}` → 3
tris, missing≈0.0026). Pin the mechanism, fix it exactly with `orient2D`/`inCircle`,
add a CDT fuzz test asserting `buildDelaunay` always fills the hull. This should also
shrink the residual 34 CDT enforcement failures.

**Then, every increment:** Logbook passage (book format) + parity cell if a number
changed + audit/plan memory update + commit ONLY your files + ScheduleWakeup ~420s.

---

## 10. Logging = BOOK FORMAT (mandatory)

Log each increment as a **Logbook passage**, not a flat timeline:
- `docs/kernel-logbook.md` (markdown) AND `kernel-logbook.html` (in job tmp).
- 📖 Artifact: `https://claude.ai/code/artifact/3dadb64a-fba6-4b97-9c31-1f359a418e8f`
  — republish the SAME file path/url to keep the link stable; use a `.proven` callout.
- Structure: **Parts → Chapters (feature arcs) → passages (increments).** Narrative,
  literary, honest about what remains. Part III is the mesh/seam arc (Chapter 13).
- Parity doc `docs/modeling-industry-parity.md`: update the relevant CELL only when a
  capability/number changed. No parity-timeline `<li>`.

---

## 11. Artifacts, memory, tooling

**Artifacts (all owned by Eric):**
- 📖 Kernel Logbook (current): `.../artifact/3dadb64a-fba6-4b97-9c31-1f359a418e8f`
- Modeling parity dashboard (~50% shipped; kept live):
  `.../artifact/5f636e76-5d79-4fa3-8b9e-85ebd819790d`
- Roadmap/reference "nexus-roadmap" (STALE, compiled 2026-07-09; says kernel "1921
  tests" — now 2279; editor "broken" — since partly fixed; predates the foundation
  campaign): `.../artifact/bb7ac986-266e-448e-98b7-d7fa7bb33bf9`. Eric wants this and
  the parity dashboard KEPT TRACKED/updated. NOTE: refreshing this roadmap is the
  *app-UX* vision layer — it is NOT the 30-year vision of §0.

**Memory** (`~/.claude/projects/.../memory/`, indexed by `MEMORY.md`): kernel layout,
DCC domain-expert expectations, parity reference, the foundation gap audit + plan
(governing roadmap), the logbook convention, the kernel foundation audit (per-increment
log), the four geometry representation worlds, windowed-present status, a voxel-sandbox
backlog note. Update the audit + plan memories every increment.

**Tooling:** `dhts/` (`.../Nexus-Systems/dhts`) is Eric's curated AI-dev toolbox.
Eric grants permission to install any tool you need. Headless visual testing uses
Xvfb + lavapipe (Mesa software Vulkan) + vkcube + ImageMagick + xwininfo. A headless
render-to-PNG path (`nxrender` / `--shot`) works and lets the AI *see* geometry.

---

## 12. Known traps & standing gotchas

- Windowed present is UNFINISHED: headless render-to-PNG works; the live window shows
  only in the taskbar with no visible surface (suspects: missing
  `ImGui_ImplVulkan_Init`, unwired real-window swapchain acquire→present loop, or
  lavapipe can't present on-screen). App-layer, not foundation.
- `size()`-vs-`loop.size()` mismatches when a prior pass skips entries = a recurring
  HEM OOB bug shape (caused the `insertEdgeLoop` heap corruption).
- Two different structs sharing a name across headers (VoxelGrid) caused a redefinition
  error — watch for header collisions.
- Deferred/breadth ideas parked for AFTER the foundation gate: the **Sandbox voxel
  editor** (box-only palette + grid-snap boolean union/subtract + size slider,
  implemented as a switchable LAYOUT/THEME, not a separate editor — reuses viewport /
  camera / selection / save / undo; a friendly UI wrapper over the boolean + tolerance
  spine and a great stress-test of them).

---

## 13. One-paragraph TL;DR for a cold-start session

You are running a foundation-first loop on a C++26 Vulkan geometry kernel. Kernel/
foundation ONLY; combinatorial decisions must be EXACT (RobustPredicates + SoS), never
tolerant. Probe-first (compile against `libnexus_gfx_core.a`) to pin the real break
before changing code. Land ONE proven, tested, committed+pushed increment; log it as a
book passage in `docs/kernel-logbook.md` + the 📖 artifact; update the audit/plan
memories; commit ONLY your files (`git diff --cached --stat` to verify); schedule the
next. Current front: PHASE M mesh watertightness — inc66 halved boolean leaks 30→15 via
a CDT constraint-enforcement fix; inc67 is fixing the `buildDelaunay` hull-underfill
gap. Baseline: 2279 pass / 0 fail / 15 torture leaks / 0 nondet. Before any breadth
work, STOP and let Eric run one more foundation sweep. And ask Eric to dictate the real
30-year vision for §0 — it is not written down yet, and nothing else substitutes for it.
