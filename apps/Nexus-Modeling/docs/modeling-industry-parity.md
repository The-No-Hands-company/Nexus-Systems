# Nexus-Modeling — Industry Modeling Standards & Parity Reference

**Status:** living document · **Owner:** modeling · **Purpose:** the governing checklist for reaching
feature parity with the industry-standard 3D modeling toolset *before* net-new modeling features are added.

> **Parity-first policy.** Nexus-Modeling's near-term modeling goal is not novelty — it is to ship
> *exactly what every professional DCC already ships*, done correctly. This document enumerates that
> standard toolset layer by layer (geometry engine → user-facing feature/visual), records where Nexus
> stands today, and prioritizes the gaps. **No new modeling capability is greenlit while a P0 parity gap
> in the same layer is open.**

## How to read the parity tables

Each layer below is a table with a Nexus status per feature:

| Symbol | Meaning |
|---|---|
| ✅ **Have** | Real, working capability (backed by implementation + tests). |
| 🟡 **Partial** | Exists but limited, or an API *façade* whose generative core is thin (see note). |
| ❌ **Missing** | Not present. |

"Who ships it" lists representative apps that treat the feature as table-stakes.
Nexus status is grounded in the July 2026 kernel + app inventories, **not** aspirational.

## Reference DCCs surveyed

| App | Signature strength (what it defines the standard for) |
|---|---|
| **Blender** | Poly modeling breadth, Geometry Nodes (procedural), sculpt, all-in-one FOSS baseline |
| **Maya** | Poly modeling + rigging/animation pipeline, industry film/games standard |
| **3ds Max** | The **modifier stack** paradigm, arch-viz, game-asset modeling |
| **Houdini** | Procedural node modeling (SOPs), the gold standard for non-destructive/parametric |
| **Modo** | Workflow ergonomics, MeshFusion (live booleans), poly/sub-d |
| **ZBrush** | Digital sculpting: dyntopo/dynamesh, multires, sculpt brushes, VDMs |
| **Plasticity** | Artist-friendly CAD/NURBS solids, the "CAD for concept artists" bar |
| **Rhino / Fusion / SolidWorks** | NURBS + parametric solid CAD, sketches/constraints, drawings, assemblies |
| **Silo / Wings3D** | Pure, focused subdivision poly-modeling ergonomics |

---

## Foundation & Robustness (the depth axis)

The layer tables below measure **breadth** — is a feature *present*? This section measures **depth** — is the kernel foundation *production-robust*? A cell can be ✅ present yet ❌ robust (the boolean is the clearest case). **Foundation gaps outrank breadth features** in the backlog: a house needs its cement poured before more rooms.

| Foundation pillar | Status | Reality (with evidence) |
|---|---|---|
| Exact geometric predicates | ✅ Solid | Adaptive-exact `orient2D/orient3D` (Shewchuk-style expansion arithmetic, float-fast-path→exact-fallback) in `RobustPredicates.cpp`; genuinely used by boolean/Delaunay/CDT/Voronoi |
| Robust mesh boolean / CSG | ❌ Cosmetic *(foundation in progress)* | `BooleanOperation.cpp` still classifies whole triangles by centroid → jagged seams on coarse meshes. **Rebuild ~done, awaiting swap:** ① tri-tri **segment** ✓ · ② per-triangle **retriangulation** ✓ (also fixed a broken `ConstrainedDelaunay2D`) · ③ **whole-mesh cut** ✓ · ④ **classify+assemble+stitch** (`robustMeshBoolean`) ✓ — **PROVEN on coarse boxes**: union/intersection/difference give exact volumes (15/1/7) and watertight 2-manifold shells (Euler χ=2). Final step: swap `BooleanOperation` onto it (then this cell → Have) |
| Unified topological core + Euler operators | 🟡 Fractured | Real half-edge (`HalfEdgeMesh`, twin/next/prev, `isManifold`) but an **island** — Bevel/Extrude/Inset/EdgeBridge run on raw indexed `Mesh`; **no Euler operators** (split/collapse/flip) |
| Tolerance / units model | ❌ Missing | No central model; a dozen scattered scale-blind epsilons (`1e-10`×114, `1e-8`×37, `1e-12`×32, …) — same op misbehaves on a 0.5 mm part vs a 5 km terrain |
| Manifold / degenerate handling | 🟡 Partial | `isManifold` + non-manifold gates + Euler-Poincaré exist, but not enforced through every op |
| Stable element IDs across ops | 🟡 Partial | Present in several ops; not universal |

**Foundation-first track (outranks breadth P0s):** ① robust boolean that splits along the intersection curve (assemble existing tri-tri intersect + CDT + exact predicates) → ② promote the half-edge to the authoritative core + Euler operators, migrate the edit ops onto it → ③ a Tolerance/units module every op consults.

## L0 — Geometry representations (the engine substrate)

| Feature | Industry-standard behavior | Who ships it | Nexus | Gap |
|---|---|---|---|---|
| Indexed polygon mesh (n-gon) | Positions/normals/UV/tangents, stable IDs, skin weights | all | ✅ | — |
| Half-edge / BMesh topology | Adjacency for interactive edit ops | Blender, Maya, Modo | ✅ | — |
| Boundary representation (B-rep) | Typed vertex/edge/face/shell with exact topology | CAD apps, Plasticity | 🟡 | Mesh-backed B-rep; not analytic NURBS-trimmed solid |
| NURBS curves & surfaces | Trimmed NURBS, fit/offset/intersect | Rhino, Maya, CAD | ✅ | — |
| Subdivision surfaces (Catmull-Clark) | Levels + edge/vertex creasing (OpenSubdiv-grade) | all | ✅ | Verify crease + UV-boundary rules vs OpenSubdiv |
| Implicit / SDF | Signed-distance fields, surface nets, winding number | Houdini, ZBrush, nTop | ✅ | — |
| Voxel | Voxelize / VDB-style volumes | Houdini, ZBrush | ✅ | No sparse-VDB hierarchy |
| Point cloud / Gaussian splat | Import, normals, render | modern DCCs | ✅ | — |

**Verdict:** L0 is a genuine strength — Nexus has *more* representation families than most DCCs. Main gap is a true analytic solid B-rep (vs mesh-backed).

## L1 — Mesh direct-modeling toolset (edit mode)

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| Extrude (region/individual/along normal) | all | ✅ | — |
| Inset faces | all | ✅ | — |
| Bevel / Chamfer (edge & vertex) | all | ✅ | — |
| Fillet (mesh & face) | all | ✅ | — |
| Bridge edge loops | all | ✅ | — |
| Edge slide / vertex slide | all | ✅ | — |
| Knife / cut / section | all | ✅ | — |
| Merge / weld / remove-doubles | all | ✅ | — |
| Boolean (union/diff/intersect) + tolerant | all | 🟡 | **cosmetic** — whole-triangle centroid classify, no intersection-curve split → jagged seams, fails on coarse meshes (see Foundation track) |
| Solidify / thicken / shell | all | ✅ | — |
| Displace | all | ✅ | — |
| Loop cut / ring | all | 🟡 | App mode exists; confirm kernel loop-cut op + n-cuts/slide |
| Dissolve / collapse / limited-dissolve | all | 🟡 | Decimate/simplify present; explicit edit-mode dissolve not surfaced |
| Spin / screw / duplicate-along | Blender, Max | ❌ | No spin/screw op |
| Rip / rip-fill / vert-rip | Blender, Maya | ❌ | — |
| Separate / join / split by loose parts | all | 🟡 | Island decomposition exists; artist-facing separate/join TBD |
| Grid/subdivide/poke/triangulate/quadrangulate | all | 🟡 | Subdiv + tri/quad remesh exist; per-selection subdivide/poke TBD |

**Verdict:** the destructive toolset is broad and mature; missing pieces are ergonomic edit-mode ops (spin, rip, dissolve, poke) that artists expect.

## L2 — Retopology & remeshing

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| Quad remesh (auto-retopo) | ZBrush, Blender, Modo | ✅ | Validate quad-flow quality |
| Voxel remesh (dynamesh) | ZBrush, Blender | ✅ | — |
| Decimate / simplify (ratio, planar) | all | ✅ | — |
| Manual retopo tools (poly-draw, relax, snap-to-surface) | Maya, Modo, 3dCoat | 🟡 | Snapping + closest-point exist; dedicated retopo brush/tool UX TBD |
| Shrinkwrap / project to surface | all | ❌ | No shrinkwrap modifier/op |

## L3 — Sculpting

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| Core brushes (draw/clay/smooth/inflate/flatten/pinch/crease/grab) | ZBrush, Blender | 🟡 | 7 brushes present; missing clay-strips, scrape, snake-hook, pose, cloth, elastic |
| Masking (+ invert/flood/blur) | all | ✅ | Blur/gradient masks TBD |
| Symmetry (planar + radial) | all | ✅ | Radial/tiling TBD |
| Dynamic topology (dyntopo) | ZBrush, Blender | ❌ | Static-mesh sculpt only |
| Multiresolution | ZBrush, Blender, Mudbox | ❌ | — |
| Sculpt layers | ZBrush, Mudbox | ❌ | — |
| In-session remesh / dynamesh | ZBrush, Blender | ❌ | Remesh exists in kernel; not wired into stroke loop |
| Vector-displacement / alpha brushes | ZBrush, Mudbox | ❌ | — |

**Verdict:** competent basic vertex-displacement sculptor; a real gap vs ZBrush/Blender-sculpt (dyntopo/multires/layers is the table-stakes trio).

## L4 — Curves & surfaces

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| NURBS/Bezier curves (edit, fit, offset, split) | Rhino, Maya | ✅ | — |
| NURBS surfaces (fit, offset, trim, intersect) | Rhino, CAD | ✅ | — |
| Sweep / loft / revolve / rail / network (Gordon) | Rhino, Maya | ✅ | — |
| Blend / patch network | Rhino, CAD | ✅ | — |
| Continuity control G0–G3 + curvature comb / zebra | Class-A CAD | ✅ | — |

**Verdict:** surfacing is a standout — near Rhino-class breadth already.

## L5 — Parametric / procedural / non-destructive

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| Feature history / regeneration | CAD apps | 🟡 | Regenerates only Sketch/Extrude/Revolve; the CAD façade outruns this core |
| Modifier stack (ordered, non-destructive) | 3ds Max, Blender | 🟡 | Kernel `ModifierStack`: 7 modifiers, non-destructive re-eval + result caching + versioned serialization (save/IO round-trip); editor wiring + more ops (Subdivision/Bevel/Boolean) next |
| Procedural node graph (Geometry Nodes / SOPs) | Houdini, Blender | ❌ | `EvaluationGraph` exists for ops but no user-facing node modeling |
| Expression/parameter-driven dimensions | CAD apps | 🟡 | `CadExpressionEngine`/equation sketch API present; end-to-end wiring TBD |

**Verdict:** **the single most important structural gap.** Non-destructive/procedural modeling (modifier stack *or* node graph) is table-stakes in every modern DCC, and the CAD feature tree only regenerates two feature types. Closing this unlocks the whole CAD façade.

## L6 — CAD / solid modeling

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| 2D sketch + constraint solver | all CAD | ✅ | Missing tangent/fix/collinear/midpoint constraint types |
| Sketch auto-constraint inference | Fusion, SW | ✅ | — |
| Extrude / revolve features | all CAD | ✅ | — |
| Fillet / chamfer / shell / draft / rib features | all CAD | 🟡 | API present; not regenerable feature-tree ops yet |
| Patterns (linear/circular) | all CAD | 🟡 | API present; generative backing TBD |
| Hole wizard, sheet metal, direct edit | SW, Fusion | 🟡 | Rich API façade; needs generative core |
| Assemblies + mates (mate/align/gear/dist/angle) | all CAD | 🟡 | Types defined; solver/regeneration TBD |
| Drawings / views / BOM / configurations | all CAD | 🟡 | API defined; output pipeline TBD |
| GD&T / MBD / PMI annotations | SW, NX | 🟡 | Data model present; authoring/render TBD |
| Design rules / interference / measure | all CAD | 🟡 | Checkers defined; wiring TBD |

**Verdict:** SolidWorks-shaped breadth on paper; depends entirely on L5 (regeneration engine) to become real.

## L7 — UV & unwrapping

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| Unwrap (LSCM/ABF/angle-based) | all | ✅ (LSCM) | Add ABF++/conformal options |
| Seams / pin / live-unwrap | all | 🟡 | Seam data TBD; interactive live unwrap missing |
| Packing / islands / margin | all | ❌ | No packer surfaced |
| UDIM / multi-tile | Maya, Blender, Mari | ❌ | — |
| Checker/distortion overlay | all | ❌ | — |

## L8 — Modifiers & deformers

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| Cage / lattice deform | all | ✅ | Lattice UX on top of cage TBD |
| Displace | all | ✅ | — |
| Skin / skeleton bind | Maya, Blender | ✅ | — |
| Bend / twist / taper / stretch | 3ds Max, Blender | ❌ | — |
| Wrap / mesh-deform / surface-deform | all | ❌ | — |
| Shrinkwrap | all | ❌ | — |
| Morph targets / blend shapes | all | ✅ | — |

*(Deformers become truly useful once L5's non-destructive stack exists.)*

## L9 — Selection, snapping, transforms, precision

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| Object + component (vert/edge/face) select | all | ✅ | — |
| Box / lasso / circle / paint select | all | 🟡 | Marquee is a bare flag; lasso/circle/paint missing |
| Soft / proportional edit | all | ✅ | Falloff-curve options TBD |
| Snapping (vertex/edge-mid/face/grid) | all | ✅ | Add snap-to-normal, incremental, angle snap |
| Numeric entry during transform | all | 🟡 | Confirm typed-value + axis-lock flow |
| Transform orientations & pivots (cursor/median/active) | all | 🟡 | `TransformOrientation` exists; pivot modes TBD |
| Symmetry / mirror editing | all | 🟡 | Mirror mode exists; live topological symmetry TBD |

## L10 — Scene organization & instancing

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| Scene graph / parenting | all | ✅ | — |
| Instancing (linked duplicates) | all | ✅ | — |
| Outliner / hierarchy panel | all | ❌ | No user-facing outliner surfaced |
| Collections / layers / groups | all | ❌ | — |
| References / linked libraries | Maya, Blender | ❌ | — |

## L11 — Viewport, display & visual feedback

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| Gizmos (translate/rotate/scale) | all | ✅ | — |
| Grid / working plane / ortho-persp | all | ✅ | — |
| Selection outline / highlight | all | ✅ | — |
| Constraint & sketch overlays | CAD | ✅ | — |
| Measurement HUD / dimensions | all | ✅ | — |
| **Shading mode switch** (wire/solid/material/rendered) | all | ❌ | **No app-level shading switcher** — a visible parity gap |
| Matcap / studio-light shading | ZBrush, Blender | 🟡 | Kernel shading exists; matcap UX TBD |
| X-ray / see-through, backface cull toggle | all | ❌ | — |
| Cavity / AO / wireframe-overlay display | all | 🟡 | `MeshAmbientOcclusion` in kernel; viewport toggle TBD |
| Retopo / on-surface overlay | Maya, Modo | ❌ | — |

## L12 — History, undo, non-destructive workflow

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| Global undo/redo (all ops) | all | 🟡 | Sculpt-stroke undo ✅; `CadCommand` pattern exists; app-wide undo stack TBD |
| Non-destructive edit history | Houdini, CAD | 🟡 | Feature history partial (L5) |
| Operator redo-panel / adjust-last-op | Blender | ❌ | — |

## L13 — Interchange / IO

| Format | Direction expected | Who ships it | Nexus | Gap |
|---|---|---|---|---|
| OBJ | import + export | all | ✅ | import (v/vt/vn, n-gon, negative idx) + ASCII export |
| PLY | import + export | all | ✅ | ASCII + binary-LE import + ASCII export |
| STL | import + export | all + CAD/print | ✅ | binary export + binary/ASCII import (+ optional weld) |
| glTF 2.0 | import + export | all modern | ✅ | hand-rolled .gltf + .glb import; .glb export |
| FBX | import + export | games/film | ❌ | — |
| USD / USDZ | import + export | modern pipelines | 🟡 | .usda ASCII mesh import+export; binary crate (.usdc) + full USD later |
| Alembic | import + export | film | ❌ | — |
| STEP / IGES | import + export | CAD | ❌ | — |
| Native (.nxm) | round-trip | — | ✅ | — |

**Verdict:** interchange was the most glaring gap and is now largely closed. **OBJ, STL, PLY, and glTF 2.0 import/export have landed** (glTF hand-rolled: JSON parser + base64 + accessor decoding, `.gltf` & `.glb`), all with non-finite hardening and round-trip tests. `import → edit → export` now works for every universal mesh format plus the modern standard, **reachable from the editor's File menu** (all routed through MeshIO). USD `.usda` (ASCII) mesh import/export is also in (hand-rolled). Remaining: a proper file picker (fixed filenames for now), then FBX/Alembic + USD binary crate (heavier, likely SDK deps).

## L14 — Units, measurement & analysis

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| Unit system (metric/imperial, precision) | CAD | ✅ | — |
| Dimensions / measure (dist/angle/radius) | all | ✅ | — |
| Mass / volume / inertia properties | CAD | ✅ | — |
| Curvature / zebra / draft analysis | Class-A CAD | ✅ | Draft/wall-thickness surfacing TBD |
| Mesh health (non-manifold, holes, self-intersect) | all | ✅ | — |

**Verdict:** analysis/diagnostics is another strength.

## L15 — UX, onboarding & discoverability

| Feature | Who ships it | Nexus | Gap |
|---|---|---|---|
| "Simple by default" starter surface | modern DCCs | 🟡 | Charter defined (`ux-baseline-charter.md`); implementation TBD |
| Progressive disclosure / context tools | Modo, Fusion | 🟡 | Planned; mode system supports it |
| Keymap / customization | all | 🟡 | `Keybindings.h` new; remap UI TBD |
| Guided intro / templates | Fusion, modern apps | 🟡 | `TemplateManager` present |

---

## Current-state summary

Nexus-Modeling is, structurally, **much further along than a young project usually is**: a broad, well-instrumented
mesh + NURBS kernel with dual topology reps, a mesh-backed B-rep, robust exact predicates, the full destructive
modeling op suite, multiple remesh/decimation/subdiv paths, implicit/voxel/SDF tooling, and Rhino-class surfacing.
Diagnostics (Euler-Poincaré, genus, non-manifold gates, Hausdorff, repair) are excellent. The editor has a clean
mode/registry architecture with the expected direct-modeling and CAD modes, gizmos, snapping, and a new Vulkan viewport.

The gaps cluster in **four themes**, in priority order:

1. **Interchange (L13)** — *largely done:* OBJ, STL, PLY, and glTF 2.0 import/export landed; only editor wiring (+ FBX/USD/Alembic) remain. Was the top gap; now mostly closed.
2. **Non-destructive / procedural core (L5)** — feature history regenerates only Sketch/Extrude/Revolve; no modifier
   stack and no procedural node graph. This is the structural keystone: it makes the entire CAD façade (L6) and the
   deformer set (L8) real.
3. **Sculpting depth (L3)** — no dyntopo / multires / layers.
4. **Editor polish (L1/L9/L11)** — spin/rip/dissolve edit ops, box/lasso select, a viewport shading-mode switcher,
   an outliner, and consolidating the duplicated camera controllers.

---

## Prioritized parity-closure backlog

### Foundation (P0⁺ — the depth track, outranks the breadth P0s below)
- **Robust mesh boolean** that splits along the intersection curve (tri-tri intersect → CDT retriangulation → classify → stitch). Fixes the cosmetic boolean; unblocks real CSG that everything CAD depends on.
- **Unified topological core:** promote `HalfEdgeMesh` to authoritative + add Euler operators (split/collapse/flip/split-face/join-face) with invariant tests, then migrate Bevel/Extrude/Inset/EdgeBridge onto it.
- **Tolerance/units module** (absolute + relative, scale/unit aware) every op consults, replacing the scattered epsilons.

### P0 — table-stakes; nothing new in-layer until these land
- **IO importers + core formats:** ✅ OBJ, STL, PLY, glTF 2.0 import/export landed **and wired into the editor File menu** (routed through the tested MeshIO). Remaining polish: a file picker (fixed filenames for now). Then FBX/USD/Alembic (P1). (L13)
- **Non-destructive modifier stack** (ordered, re-evaluatable) for mesh ops — the minimum viable procedural core. (L5)
- **Viewport shading-mode switcher** (wireframe / solid / material / rendered) + x-ray + backface toggle. (L11)
- **Box/lasso/circle select** promoted from the current bare-flag marquee. (L9)
- **Consolidate `ViewportController`/`ViewportManager`** into one camera controller. (editor debt)
- **App-wide undo/redo stack** covering all operators. (L12)

### P1 — expected by any professional user
- 🟡 USD `.usda` (ASCII) mesh import/export landed. Remaining: FBX import/export, Alembic export, USD binary crate — the SDK-dependency decisions. (L13)
- Back the CAD feature tree so fillet/chamfer/shell/draft/pattern/hole are **regenerable features**, not just API. (L5→L6)
- Sculpt **dyntopo + multires + layers**; add clay-strips/scrape/snake-hook/pose brushes. (L3)
- Missing sketch constraints (tangent/fix/collinear/midpoint). (L6)
- Edit-mode ops: spin/screw, rip, dissolve/limited-dissolve, poke, separate/join. (L1)
- Outliner + collections/layers. (L10)
- UV packing + UDIM + distortion overlay + live unwrap. (L7)

### P2 — depth & pipeline maturity
- Procedural **node graph** (Geometry-Nodes/SOP-style) on top of the modifier core. (L5)
- Deformers: bend/twist/taper, wrap/surface-deform, shrinkwrap. (L8)
- STEP/IGES for CAD interchange; assemblies/mates solver; drawings/BOM/GD&T output. (L6/L13)
- Retopo tools + on-surface overlay; matcap/studio shading; adjust-last-op redo panel.

---

## Parity-first working policy (definition of done)

A modeling layer is **"at parity"** when every feature marked table-stakes for that layer (the P0/P1 rows above)
is ✅ **Have** — real, tested, and reachable from the editor UI — not 🟡 façade. Until a layer reaches parity:

1. Work in that layer targets its open P0 → P1 gaps, in that order.
2. New/novel capability in that layer is deferred (logged here, not built).
3. A feature counts only when it is wired end-to-end: **kernel op → editor tool → visible result → round-trips through save/IO**.

This document is the source of truth; update the status column as gaps close, and keep the Artifact rendering in sync.
