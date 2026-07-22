# Nexus-Modeling · Modeling Parity Index
Every tool the industry ships, before anything new

A layer-by-layer audit of the standard 3D-modeling toolset — geometry engine to user-facing visual — measured against Blender, Maya, 3ds Max, Houdini, Modo, ZBrush, Plasticity and Rhino, with Nexus's real status today. Grounded in the July 2026 kernel + editor inventory, not aspiration. This is the governing checklist: close the P0 gap in a layer before adding anything new to it.

**Overall parity · 112 tracked features**  
56 Have | 31 Partial / façade | 25 Missing  
**50% shipped (breadth)** — but see Foundation & Robustness below: the boolean is now a real CSG pipeline (✅), while the topological core + tolerance model remain the thin foundation. Loop stays foundation-first.

---

## Four gap themes (priority order)

### ✓ Interchange (IO) — done
OBJ/STL/PLY/glTF import+export shipped and wired into the editor File menu. Only FBX/USD/Alembic (P1) remain.

### 2 Non-destructive core
History regenerates only Sketch/Extrude/Revolve — no modifier stack or node graph. The keystone that makes CAD & deformers real.

### 3 Sculpt depth
No dyntopo / multires / layers.

### 4 Editor polish
Box-select, shading-mode switch, outliner, spin/rip/dissolve, dedupe camera controllers.

---

## Foundation & Robustness — the depth axis

The layer matrix below measures breadth (is a feature present?). This measures depth (is the foundation production-robust?). A cell can be present yet not robust — the boolean is the clearest case. Foundation gaps outrank breadth features.

| Aspect | Status | Notes |
|--------|--------|-------|
| **Exact geometric predicates** | Solid | Adaptive-exact orient2D/3D (Shewchuk-style expansion arithmetic, float fast-path → exact fallback), genuinely used by boolean / Delaunay / CDT / Voronoi. |
| **Robust mesh boolean / CSG** | Solid | Real CSG pipeline — tri-tri segment → per-triangle retriangulation → whole-mesh cut → classify/stitch — now wired into BooleanOperation (the old whole-triangle classifier is deleted). Watertight, 2-manifold, exact volumes on coarse boxes (union 15 / intersection 1 / difference 7, Euler χ=2). Fixed a fully-broken ConstrainedDelaunay2D en route. Remaining: coplanar-overlap faces. |
| **Unified topological core + Euler operators** | Fractured | Half-edge with Euler operators (flip / split / collapse) now proven integrity-clean: added checkIntegrity() — a direct invariant validator (twin reciprocity, face-cycle closure, no live→dead refs) — + property tests that every successful flip/split/collapse preserves connectivity & genus. Fixed two real bugs it exposed (collapse left dangling wing-twins; split mis-wired a face's prev). Prerequisite cleared — the half-edge now preserves the full attribute set losslessly (size-guarded toMesh). But a checkIntegrity() audit found 5 of 6 HEM local ops corrupt the topology (pokeFace, extrudeFaces, connectVertices, insertEdgeLoop, insertEdgeVertex) — invisible because toMesh rebuilds a fresh index mesh. Fixed insertEdgeVertex (crossed twins + un-repaired vertex roots) and pokeFace (misused addEdgePair → 6n garbage half-edges + orphaned perimeter; rewritten to reuse perimeter edges + 2n spokes). connectVertices (face-split cross edges into the wrong sub-face + a pos-swap decoupled the endpoints), extrudeFaces (walls mis-twinned; rewired bottom↔neighbour / top↔cap), and insertEdgeLoop (crossed split-edge twins + splitFacesAlongLoop added dangling un-wired cross edges — reworked through the fixed connectVertices). **All 6 of 6 HEM local ops are now integrity-clean** (bevelVertex, insertEdgeVertex, pokeFace, connectVertices, extrudeFaces, insertEdgeLoop), each with a property test. The half-edge core's own operators are sound. Migration underway: HEM-native insetFace + extrudeFacePrism primitives (full-attribute, integrity-proven) now back TWO raw-index edit ops — InsetFacesOperation (Factor+replace) and ExtrudeOperation (keep=false) — via fromMesh → per-face op → toMesh(false), quad+attribute preserving, raw fallback. (Extrude keep=true keeps the base as overlapping non-manifold prisms the half-edge can't represent → stays raw, a real semantic boundary.) Next: Bevel/EdgeBridge. |
| **Tolerance / units model** | Module in | Central Tolerance (geometry/Tolerance.h) now exists — absolute floor + relative-to-magnitude, with nearlyEqual / isZero / coincident / at and a forCharacteristicLength() factory that proportions the floor to model size. **Proven scale-aware**: coincidence behaves correctly at a 0.5 mm part and a 5 km terrain (a fixed 1e-5 epsilon fails one scale; Tolerance handles both). First high-traffic sites migrated (weld / mergeByDistance). Remaining: migrate the ~180 scattered scale-blind literals op-by-op. |
| **Manifold / degenerate handling** | Solid | Foundation sweep verified: solidify (open→closed euler-2 shell), boolean (watertight euler-2), migrated inset/extrude (euler-2) all confirmed 2-manifold-constructible + Euler-correct; degenerate faces (repeated index, <3 sides) rejected at the half-edge boundary. Locked in by test_ManifoldEnforcement. |
| **Non-finite input hardening** | Solid | Foundation sweep verified: every public geometry op rejects non-finite scalars via IEEE-754 bit inspection (not the -ffast-math-unreliable std::isfinite). Consolidated into a canonical geometry::isFinite in Tolerance.h with a fast-math regression-guard test. |

### Foundation-first track (outranks the breadth P0s):
1. Robust boolean that splits along the intersection curve ✅ done
2. Half-edge authoritative core + Euler operators (flip/split/collapse + insertEdgeVertex integrity-clean ✅; full-attribute round-trip ✅; audit found 5/6 HEM local ops corrupt topology — all 6/6 HEM local ops integrity-clean ✅; edit-op migration underway — Inset (Factor+replace) + Extrude (keep=false) now route through HEM insetFace/extrudeFacePrism, quad + attribute preserving with raw fallback; next: Bevel/EdgeBridge)
3. A tolerance/units module every op consults (module landed + proven scale-aware ✅; weld/merge migrated; ~180 scattered epsilons still to migrate)

**Foundation sweep COMPLETE**: non-finite ✅ (consolidated + fast-math guard); determinism ✅ (boolean cross-run tests); stable element IDs ✅ (EdgeBridge + ModifierStack fixed); manifold/degenerate enforcement ✅ (verified + locked by test_ManifoldEnforcement). **Only analytic B-rep remains — a major feature, not a foundation fix.**

---

## Status Legend
- **Have** — real & tested
- **Partial** — limited or API façade
- **Missing**

---

## Layer Parity Matrix

### L0 Geometry representations
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Indexed n-gon mesh | ✅ | | |
| Half-edge / BMesh | ✅ | | |
| B-rep (typed topology) | ◐ | mesh-backed | NURBS-backed |
| NURBS curves & surfaces | ✅ | | |
| Subdivision + creases | | ✅ | |
| Implicit / SDF | | | ✅ |
| Voxel / sparse VDB | | | ✅ |
| Point / Gaussian splat | | | ✅ |

### L1 Mesh direct-modeling
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Extrude / inset | ✅ | | |
| Bevel / chamfer | | ✅ | |
| Fillet (mesh/face) | | ✅ | |
| Bridge loops | | ✅ | |
| Edge / vertex slide | | ✅ | |
| Knife / cut / section | | | ✅ |
| Merge / weld | ✅ | | |
| Boolean + tolerant robust CSG | ✅ | splits curve | |
| Solidify / thicken | | | ✅ |
| Displace | | | ✅ |
| Loop cut / ring n-cuts | | | ✅ |
| Dissolve / collapse | ✅ | | |
| Separate / join | ✅ | | |
| Subdivide / poke | ✅ | | |
| Spin / screw | | | ✅ |
| Rip / rip-fill | | ✅ | |
| Separate / join | ✅ | | |

### L2 Retopology & remeshing
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Quad remesh | ✅ | | |
| Voxel remesh | ✅ | | |
| Decimate / simplify | ✅ | | |
| Manual retopo tools | | | ✅ |
| Shrinkwrap / project | | ✅ | |

### L3 Sculpting
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Masking (+flood/invert) | | ✅ | |
| Symmetry (planar/radial) | | | ✅ |
| Core brushes (7) + clay/scrape/pose | | | ✅ |
| Dynamic topology | | | ✅ |
| Multiresolution | | | ✅ |
| Sculpt layers | | | ✅ |
| In-session remesh | ✅ | kernel has it | |
| Vector displacement | | | ✅ |

### L4 Curves & surfaces
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| NURBS/Bezier curves | ✅ | | |
| NURBS surfaces | ✅ | | |
| Sweep/loft/revolve/rail | | | ✅ |
| Blend / patch network | | ✅ | |
| Continuity G0–G3 + zebra | | ✅ | |

### L5 Parametric / procedural
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Feature history / regen | ✅ | Sketch/Extrude/Revolve only | |
| Expression-driven dims | | | wiring TBD |
| Modifier stack | | | 7 modifiers + caching + serialize |
| Procedural node graph | | | TBD |

### L6 CAD / solid modeling
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Sketch + constraint solver | | ✅ | tangent/fix TBD |
| Auto-constraint | | | ✅ |
| Extrude / revolve features | ✅ | | |
| Fillet/chamfer/shell features | | façade | |
| Patterns | | façade | |
| Hole/sheet-metal/direct | | façade | |
| Assemblies + mates solver | | | ✅ |
| Drawings / BOM / configs output | | | ✅ |
| GD&T / MBD / PMI authoring | | | ✅ |
| Design rules / interference | | | ✅ |

### L7 UV & unwrapping
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Unwrap (LSCM) + ABF | ✅ | | |
| Seams / pin | ✅ | | |
| Packing / islands | ✅ | | |
| UDIM / multi-tile | | | ✅ |
| Distortion overlay | | | ✅ |

### L8 Modifiers & deformers
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Cage / lattice deform | | | ✅ |
| Displace | | | ✅ |
| Skin / bind | | | ✅ |
| Morph / blend shapes | | | ✅ |
| Bend / twist / taper | | | ✅ |
| Wrap / surface-deform | | | ✅ |
| Shrinkwrap | | | ✅ |

### L9 Selection / transform / precision
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Object + component select | ✅ | | |
| Soft / proportional edit | | | ✅ |
| Snapping (vert/edge/face/grid) + angle/normal | | ✅ | |
| Box / lasso / circle select | | ✅ | bare flag |
| Numeric transform entry | | ✅ | |
| Orientations & pivots | | ✅ | |
| Symmetry / mirror editing | ✅ | | |

### L10 Scene org & instancing
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Scene graph / parenting | ✅ | | |
| Instancing (linked dup) | | | ✅ |
| Outliner panel | | | ✅ |
| Collections / layers | | | ✅ |
| References / libraries | | | ✅ |

### L11 Viewport & visual feedback
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Transform gizmos | ◐ | TransformGizmo coded | |
| Grid / working plane / ortho | ✅ | | |
| Selection outline | ○ | wireframe stub | |
| Constraint & sketch overlays | | | ✅ |
| Measurement HUD | | | ✅ |
| Shading-mode switch | | visible gap | |
| Matcap / studio light | ✅ | kernel has it | |
| Cavity / AO display | | ✅ | kernel has AO |
| X-ray / retopo overlay | | | ✅ |

### L12 History & undo
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Global undo/redo | | ✅ | sculpt only |
| Non-destructive history | | | ✅ |
| Adjust-last-op panel | | | ✅ |

### L13 Interchange / IO
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Native .nxm round-trip | ✅ | | |
| OBJ import + export | ✅ | | |
| STL import + export | ✅ | | |
| PLY import + export | ✅ | | |
| glTF 2.0 import + export | ✅ | | |
| USD .usda ASCII | | mesh in+out | |
| FBX | | | SDK |
| Alembic | | | SDK |
| STEP / IGES | | | ✅ |

### L14 Units, measure & analysis
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Unit system | ✅ | | |
| Dimensions / measure | | ✅ | |
| Mass / volume / inertia | | | ✅ |
| Curvature / zebra / draft | | | ✅ |
| Mesh health / diagnostics | | | ✅ |

### L15 UX & onboarding
| Feature | Have | Partial | Missing |
|---------|------|---------|---------|
| Simple-by-default surface | | | charter set |
| Progressive disclosure | | | ✅ |
| Keymap / customization | | | new |
| Guided intro / templates | | | ✅ |

---

## Parity-closure backlog
**What to build, in order**

### P0 — Table-stakes (nothing new in-layer until these land)
| Item | Layer |
|------|-------|
| IO: ✅ OBJ/STL/PLY/glTF in+out + editor File-menu wiring done · polish: a file picker | L13 |
| Non-destructive modifier stack for mesh ops — minimum procedural core | L5 |
| Viewport shading-mode switch (wire/solid/material/rendered) + x-ray/backface | L11 |
| Box / lasso / circle select (promote the bare marquee) | L9 |
| Consolidate ViewportController / ViewportManager debt | — |
| App-wide undo/redo stack across all operators | L12 |

### P1 — Expected by any professional user
| Item | Layer |
|------|-------|
| FBX + USD in/out; Alembic export | L13 |
| Back the CAD tree: fillet/chamfer/shell/draft/pattern/hole as regenerable features | L5→L6 |
| Sculpt dyntopo + multires + layers; clay-strips/scrape/snake-hook/pose | L3 |
| Sketch constraints: tangent / fix / collinear / midpoint | L6 |
| Edit ops: spin/screw, rip, dissolve, poke, separate/join | L1 |
| Outliner + collections/layers | L10 |
| UV packing + UDIM + distortion overlay + live unwrap | L7 |

### P2 — Depth & pipeline maturity
| Item | Layer |
|------|-------|
| Procedural node graph (Geometry-Nodes / SOP-style) on the modifier core | L5 |
| Deformers: bend/twist/taper, wrap/surface-deform, shrinkwrap | L8 |
| STEP/IGES; assemblies + mates solver; drawings/BOM/GD&T output | L6/L13 |
| Retopo tools + on-surface overlay; matcap/studio shading; adjust-last-op panel | L1/L11 |

---

## Parity-first · definition of done

A layer is at parity when its table-stakes features are **Have** — real, tested, reachable from the editor — not façade. A feature counts only when wired end-to-end: kernel op → editor tool → visible result → round-trips through save/IO. Until then, work in a layer targets its open P0 → P1 gaps, and novel capability is deferred.

**Source of truth**: `docs/modeling-industry-parity.md` · keep this rendering in sync

---

*Inventoried July 2026*