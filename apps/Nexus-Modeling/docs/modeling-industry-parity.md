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
| Robust mesh boolean / CSG | ✅ Solid | Real CSG pipeline — `TriTriIntersect` → `TriangleRetriangulate` → `MeshCut` → `robustMeshBoolean`, **now wired into `BooleanOperation`** (old whole-triangle classifier deleted). Watertight, 2-manifold, exact volumes on **coarse** boxes (union 15 / intersection 1 / difference 7, Euler χ=2). Also fixed a fully-broken `ConstrainedDelaunay2D` en route. Remaining: coplanar-overlap faces |
| Unified topological core + Euler operators | 🟡 Fractured *(Euler ops now solid)* | Real half-edge (`HalfEdgeMesh`) **with** Euler operators (flip / split / collapse) that are now **proven integrity-clean**: added `checkIntegrity()` — a direct half-edge-invariant validator (twin reciprocity, face-cycle closure, no live→dead references) — plus property tests asserting *every* successful flip/split/collapse preserves connectivity + genus on a triangulated solid. Fixed **two real bugs** the validator exposed: `collapseEdge` left the collapsed triangles' outer wing-twins dangling on dead half-edges (+ no triangle guard); `splitEdge` mis-wired one sub-face's `prev`. Prerequisite cleared: `HalfEdgeMesh` now round-trips the **full** vertex-attribute set losslessly (positions/uvs/normals/tangents/skin), size-guarded. **But an audit with `checkIntegrity()` found the half-edge's own local ops are mostly broken** — 5 of 6 (`pokeFace`, `extrudeFaces`, `connectVertices`, `insertEdgeLoop`, `insertEdgeVertex`) produced corrupt topology, invisible because `toMesh` rebuilds a fresh index mesh. Fixed **`insertEdgeVertex`** (crossed twin pairing + un-repaired endpoint roots — it backs `splitEdge` on n-gons) and **`pokeFace`** (misused `addEdgePair` created 6n half-edges per fan with garbage twins/faces and orphaned the perimeter on a dead face — rewritten to reuse the perimeter edges + 2n proper spoke edges). and **`connectVertices`** (the face-split diagonal linked its cross edges to the *original* successors on the other sub-face, and a `pos0<pos1` swap decoupled the endpoints from `v0/v1` — reworked to close each sub-face on its own first edge, deriving endpoints from chain positions). **`extrudeFaces`** (manual walls with correct bottom↔neighbour / top↔cap twin pairing), and **`insertEdgeLoop`** (same crossed split-edge twins as insertEdgeVertex, plus `splitFacesAlongLoop` added *dangling un-wired* cross edges via `addEdgePair` — reworked to split each crossed face through the fixed `connectVertices`). **All 6 of 6 HEM local ops are now proven integrity-clean** (`bevelVertex`, `insertEdgeVertex`, `pokeFace`, `connectVertices`, `extrudeFaces`, `insertEdgeLoop`), each with a property test asserting connectivity + genus preservation. The half-edge core's own operators are sound. **Migration underway:** a HEM-native `insetFace` primitive lands the inner-ring + border-quad topology directly on the half-edge (full attribute interpolation, integrity/genus/attribute-preserving, proven), and **two raw-index edit ops now route through it**: `InsetFacesOperation` (Factor + replaceOriginal) via `insetFace`, and `ExtrudeOperation` (keepOriginalFaces=false) via `extrudeFacePrism` — both `fromMesh → per-face op → toMesh(triangulate=false)`, quad + full-attribute preserving, with a raw-index fallback for non-manifold input. (Extrude's `keepOriginalFaces=true` retains the base as overlapping non-manifold prisms the half-edge core can't represent, so it stays raw — a real semantic boundary.) **Next:** Bevel / EdgeBridge |
| Tolerance / units model | 🟡 Module landed | Central `Tolerance` (`geometry/Tolerance.h`) now exists — absolute floor + relative-to-magnitude term, with `nearlyEqual`/`isZero`/`coincident`/`at` and a `forCharacteristicLength()` factory that proportions the floor to model size. **Proven scale-aware**: coincidence behaves correctly at both a 0.5 mm part and a 5 km terrain, and a test demonstrates a fixed `1e-5` epsilon fails one scale while `Tolerance` handles both. First high-traffic sites migrated (`weldCoincidentVertices`, `MeshVertexMerge::mergeByDistance`). **Remaining**: migrate the ~180 scattered scale-blind literals (`1e-10`×115, `1e-8`×37, `1e-12`×32, …) op-by-op and make per-op defaults derive from the model's characteristic length |
| Manifold / degenerate handling | ✅ Solid | **Foundation sweep verified**: the ops that must produce manifold/watertight output do — solidify (open surface → closed euler-2 shell; closed box → manifold double-shell), boolean union/diff/intersect (watertight euler-2), and the migrated inset/extrude (euler-2) — all confirmed 2-manifold-constructible (`HalfEdgeMesh::fromMesh`) + Euler-correct, and degenerate faces (repeated index, <3 sides) are rejected at the half-edge boundary. Locked in by `test_ManifoldEnforcement`. (`isManifold`/non-manifold gates/Euler-Poincaré remain available for callers that need per-op assertions.) |
| Stable element IDs across ops | ✅ Solid | Sweep found 3 candidate gaps; `MeshFillet` was a false positive (operates on `HalfEdgeMesh`, which has no Mesh-level IDs). Closed the two real ones — `EdgeBridge` and `ModifierStack::computeResult` now `rebuildStableElementIds()` on their output (tested). Every Mesh-producing edit op now emits stable IDs |
| Non-finite input hardening | ✅ Solid | **Foundation sweep verified sound**: every public geometry op rejects non-finite scalars via IEEE-754 **bit inspection** (`(bits & 0x7F800000) != 0x7F800000`) — none use the `-ffast-math`-unreliable `std::isfinite`. Consolidated into a canonical `geometry::isFinite(float/Vec3)` in `Tolerance.h` with a fast-math regression-guard test (constructs NaN/±Inf via bit patterns) |
| Determinism (Null backend) | ✅ Solid | Boolean union/difference/intersection have explicit cross-run determinism tests; inset/extrude/bevel + solvers likewise. Snapshot/serialization byte formats versioned with backward-compatible reads |

**Foundation-first track (outranks breadth P0s):** ① robust boolean that splits along the intersection curve ✅ **done** → ② promote the half-edge to the authoritative core + Euler operators *(Euler ops flip/split/collapse validated integrity-clean ✅; full attribute round-trip incl. tangents+skin ✅; `checkIntegrity()` audit found 5/6 HEM local ops corrupt topology — all 6/6 HEM local ops now integrity-clean ✅ (insertEdgeVertex, pokeFace, connectVertices, extrudeFaces, insertEdgeLoop fixed + bevelVertex); next: migrate the raw-index edit ops onto the hardened core)* → ③ a Tolerance/units module every op consults *(module landed + proven scale-aware ✅; weld/merge migrated; ~180 scattered epsilons still to migrate op-by-op)*.

## L0 — Geometry representations (the engine substrate)

| Feature | Industry-standard behavior | Who ships it | Nexus | Gap |
|---|---|---|---|---|
| Indexed polygon mesh (n-gon) | Positions/normals/UV/tangents, stable IDs, skin weights | all | ✅ | — |
| Half-edge / BMesh topology | Adjacency for interactive edit ops | Blender, Maya, Modo | ✅ | — |
| Boundary representation (B-rep) | Typed vertex/edge/face/shell with exact topology | CAD apps, Plasticity | 🟡 *(analytic core underway)* | Mesh-backed `BRepBody` exists; **a true analytic B-rep is now being built** — `brep::Body` (`AnalyticBRep.h`) binds typed topology (vertex/edge/**coedge**/loop/face/shell/solid) to analytic geometry (Line/Circle curves, Plane/Cylinder/Sphere surfaces), with a `checkIntegrity()` validator (coedge partners reciprocal + opposite-oriented, loop rings close, vertex continuity, no non-manifold edges) and a `fromFaces` assembler. Proven so far: analytic **box** (euler 2), **cylinder** (Cylinder-surface sides), **cone**, and **UV sphere** (pole triangle fans + latitude quad bands on a Sphere surface; V=2+(lat-1)·lon, F=lat·lon, E=lon·(2lat-1) ⇒ euler 2) — all watertight coedge-partnered solids for any segment/resolution count, winding validated by `checkIntegrity` (not eyeballed) and cross-checked against the mesh validator; all verts lie exactly on the analytic surface. A second validator `checkGeometry()` proves the analytic **geometry** agrees with the topology (edge curves reproduce their endpoint vertices via the Tolerance module, geometry finite, normals unit, partner endpoints consistent) — topological validity ≠ geometric consistency, and it catches stale-curve / non-finite corruption. **NURBS-surface faces wired in**: a face can lie on an exact `NurbsSurface` (stored on the `Body`, referenced by handle) and `Body::surfacePoint` dispatches evaluation to the existing NURBS toolkit — proven by building a NURBS-patch face that validates on both checkers and evaluates correctly. **Euler operators started**: `splitEdge` (make-edge-vertex) inserts a vertex on an edge, splitting it into two curve-sharing edges and re-linking the incident coedges + partners — proven χ-neutral (ΔV+1/ΔE+1) across every edge of box/cylinder/sphere; and **`splitFace`** (make-edge-face) adds a diagonal edge splitting a face into two surface-inheriting sub-faces with partnered diagonal coedges (ΔE+1/ΔF+1) — both proven χ-neutral on both validators, and they **compose** (split-edge then split-face stays valid). **Entity liveness/tombstoning** added (the prerequisite for removal ops + boolean): entities carry an `alive` flag, both validators skip dead entities, count only live, and enforce the "no live entity references a dead one" guard (the check that flushed out the half-edge bugs) — proven by a negative test. **Removal Euler op** `joinEdges` (kill-edge-vertex) lands the inverse of `splitEdge` — merge the two same-curve edges at a degree-2 vertex, tombstoning the removed vertex/edge/coedges — proven by the **round-trip**: `splitEdge` then `joinEdges` restores the exact live V/E/F counts and euler on every box/cylinder/sphere edge, both validators clean. **`mergeFaces`** (kill-edge-face) completes the pair — remove a shared edge, splice the two loops, merge the faces — proven round-tripping with `splitFace` (split then merge restores exact live counts). **The make/kill Euler algebra (edge-vertex ↔ edge-face) is now complete and invertible.** **Curved tessellation**: `toMesh(subdivisions)` places intermediate points per *edge* (shared by both incident faces → watertight/crack-free at any level, proven euler-2 on all primitives), and the cylinder's ring edges and **every sphere edge** (latitude rings + meridian great circles) are now true **Circle arcs** (`setEdgeArc`, validated by `checkGeometry`) — so the analytic cylinder and sphere are *exact curved solids* that tessellate directly onto the surface (every vertex on-radius to float precision, at any resolution), not on chords. **B-rep boolean prerequisite chain**: (1) analytic **surface–surface intersection** (`BRepSurfaceIntersect.h`) returns exact `brep::Curve`s — plane∩plane→Line, plane∩sphere/plane∩cylinder⊥/sphere∩sphere→Circle, None for parallel/miss/disjoint — proven every sampled point lies on *both* surfaces (`surfaceDistance`) to float precision; (2) **imprint** (`Body::imprintCurve`) cuts a planar face along a Line intersection curve: introduces a vertex where the curve pierces each boundary edge (via `splitEdge`), then splits the face with a NEW edge carrying the curve itself — the shared edge lies exactly on both surfaces, χ stays neutral (ΔV+2/ΔE+3/ΔF+1), both validators clean, composes with `mergeFaces`; (3) **point classification** (`Body::classifyPoint` → Inside/Outside/OnBoundary) ray-casts against the watertight crack-free triangulation of the shell (odd crossings ⇒ Inside; irrational ray + degeneracy-retry ⇒ robust & deterministic), proven on box/cylinder/sphere for interior, exterior, and on-face/edge/vertex points; (4) **face-vs-solid classification** (`Body::classifyFace` + `faceCentroid`) samples a face's centroid and classifies it against another solid — the boolean's per-face keep/discard decision — proven on overlapping boxes (faces inside/outside the other solid, coincident face → OnBoundary); (5) **affine transform** (`Body::transform`/`translate`) moves the whole body consistently — vertices AND every analytic Curve/Surface frame (radii + Line params scale with a uniform scale, Circle angles preserved) — so both validators stay clean; shear/non-uniform/reflection/non-finite are rejected leaving the body unchanged; classification is transform-invariant; (6) **mutual imprint** (`imprintMutually`) — the boolean's segmentation step — cuts two overlapping solids along their face-plane intersection lines to a fixpoint so that afterwards **no face of either straddles the other's boundary** (every face is uniformly Inside/Outside/On the other solid); `imprintCurve` was hardened to recognise cut endpoints that land on existing boundary vertices (essential once neighbouring faces drop shared-edge vertices), proven on corner/partial box overlaps staying watertight (euler 2, both validators clean) with all faces segmented; (7) **B-rep BOOLEAN — select + emit** (`booleanToMesh`, `BooleanOp::Union/Intersection/Difference`) — the keystone: mutually imprints, selects kept faces per op via classifyFace (Union = A-outside-B + B-outside-A; Intersection = both inside; Difference = A-outside-B + B-inside-A reversed), and emits one welded watertight triangle mesh, **proven exact** on overlapping unit boxes (Union=15, Intersection=1, Difference=7; offset overlap 13.75/2.25/5.75) — each result closed, 2-manifold, euler 2; (8) **boolean → analytic Body** (`booleanToBody`) — the kept faces are re-assembled via `Body::fromFaces` (seam vertices welded, Difference-B faces reversed) into a first-class analytic solid whose checkIntegrity/checkGeometry are clean (euler 2, boundaryEdges 0, closed) with the same exact volumes, and which **feeds another boolean** (proven `(A∪B)∪C` → valid closed solid, volume 22) — booleans now compose/chain; (9) **coplanar cleanup** (`Body::mergeCoplanarFaces`) collapses the boolean/imprint over-segmentation — merging adjacent coplanar faces that share exactly one edge via the `mergeFaces` Euler op to a fixpoint — proven to cut live face count (corner union 42→18) while keeping the solid valid/closed/euler-2 and volume-exact, idempotent, deterministic; (10) **collinear-edge cleanup + `simplify`** (`Body::mergeCollinearEdges` — the general kill-edge-vertex for collinear separate-curve Lines, generalising `joinEdges`; and `Body::simplify` which alternates it with `mergeCoplanarFaces` to a combined fixpoint) — proven to reduce a boolean's V/E/F below coplanar-merge alone (corner union 44/84/42 → 36/52/18) while keeping the solid valid/closed/euler-2/volume-exact, idempotent, deterministic. Next: coincident-face (coplanar-overlap) handling for aligned solids → curved-solid booleans (circle/curved-face imprint) |
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
| Boolean (union/diff/intersect) + tolerant | all | ✅ | robust — splits along the intersection curve → watertight / 2-manifold / correct-volume on coarse meshes (`BooleanOperation` now uses the real CSG pipeline) |
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
