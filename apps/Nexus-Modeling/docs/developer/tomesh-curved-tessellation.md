# Scope — `Body::toMesh` and the interior of a curved patch

Status: **design, not implemented.** One attempt was made and reverted; what it taught is
recorded in §6 because it constrains the design.

---

## 1. The defect, measured

`toMesh(subdivisions)` refines every **edge** through its curve, so a shared edge is
refined identically by both faces that own it. It then triangulates each **face from its
boundary ring alone** — a fan for a convex ring, an ear-clip for a concave or holed one.
The patch *interior* is never sampled, and the boundary points are connected however the
fan happens to pick.

Measured on `makeCylinder(0.5, 4, 16)`:

| subdivisions | rim points | lateral area | ruled connection of the same points | volume |
|---:|---:|---:|---:|---:|
| 0  | 16  | 12.485781 | 12.485781 | 3.061467 |
| 4  | 80  | 12.524514 | 12.563141 | 3.087100 |
| 16 | 272 | 12.525993 | 12.566091 | 3.088083 |
| ∞  | —   | —         | 12.566371 | 3.141593 |

The rim refines correctly. The surface does not: the volume **converges to 3.0881** instead
of π·r²·h = 3.1416. It converges — more subdivision does not help, because every level
repeats the same mistake. Horizontal sliver triangles along the rim are the fan's signature.

Two things depend on this being right, beyond anyone looking at the mesh:

- **`classifyPoint` casts its parity ray against `toMesh(6)`.** Its accuracy near a curved
  boundary is bounded by this tessellation.
- **`massProperties` falls back to the tessellation for any body with a holed face.** That
  is why the centred cylinder-through-box still misses both volume identities by 2.75e-2
  while every offset case is exact — the offsets have no holed faces and take the analytic
  path.

## 2. The constraint

A body's mesh must be **watertight**. Two faces sharing an edge must tessellate that edge
identically, or the mesh gets a T-junction and a crack, and a crack destroys the parity ray
that `classifyPoint` — and therefore the whole boolean — rests on.

So: **interior points are free; boundary points are frozen.** Any face may add whatever it
likes strictly inside itself, and may not touch, move, or subdivide a point on its
boundary. That is the whole difficulty, and it is what makes this a *constrained*
triangulation rather than a meshing convenience.

## 3. The property that makes it tractable

`buildRing` computes a face's boundary from the loop's coedges and each edge's own curve.
**It does not consult how the face will be triangulated.** So the boundary ring is identical
whichever path a face takes.

That gives the design its most valuable safety property:

> A face triangulated by the new method and its neighbour triangulated by the old one still
> meet exactly, because neither touched the boundary.

Which means the work can be rolled out **per surface kind, per face, behind a fallback**,
with no flag day and no all-or-nothing cutover. Any face the new path declines is simply
handled the way it is handled today, and the body stays watertight.

## 4. Tier 1 — connect the boundary correctly (no interior points)

The deficit above is **not** caused by missing interior points. It is caused by *long
diagonals*: a fan connects boundary points across the patch, and in 3D that chord cuts
inside the surface.

For a **developable** surface — cylinder and cone — connecting the same boundary points in
the *ruled* direction is already convergent, with no interior samples whatsoever:

| rim points | ruled lateral | deficit vs true |
|---:|---:|---:|
| 16  | 12.485781 | 8.06e-02 |
| 32  | 12.546194 | 2.02e-02 |
| 80  | 12.563141 | 3.23e-03 |
| 272 | 12.566091 | 2.79e-04 |

Second-order convergence, which is what a chordal approximation should give. **Tier 1 alone
fixes the measured cylinder defect**, and it is cheap: same vertex count, same triangle
count, different connectivity.

**Method.** Triangulate in the surface's own `(u,v)` domain instead of in 3D, so that
"short" means short *on the surface*:

1. Map the boundary ring through `surfaceUV`, scaling `u` by the local radius so both axes
   are lengths (keeps the predicates conditioned), and unwrapping `u` along the ring so a
   patch straddling the ±π seam is a non-wrapping polygon. Precedent exists:
   `pointInSurfacePatchUV` already does this unwrap.
2. Triangulate the `(u,v)` polygon with **`ConstrainedDelaunay2D`**, every boundary segment
   passed as a `ConstraintEdge`. Delaunay in the parameter domain maximises the minimum
   angle, which is precisely "no long diagonals"; the constraints are what forbid touching
   the boundary. That class was rebuilt and hardened in inc66 (exact `orient2D`, progress-
   guaranteed edge recovery, zero inverted triangles over a 16k-case battery), so this is
   reuse, not new machinery.
3. Emit with the vertices' existing 3D positions. Orient each triangle by
   `Surface::normalAt` at its parameter centroid, **not** by `Surface::normal` — for a
   cylinder that field is the *axis* and says nothing about which way the patch faces.

## 5. Tier 2 — interior Steiner points (doubly curved, and large patches)

A sphere is not developable: no connection of boundary-only points reproduces its interior,
because the patch bulges away from any triangle spanning it. Here interior points are
genuinely required.

They are also *safe*, by §2: a Steiner point strictly inside the face is invisible to every
neighbour. Add them as candidate positions and let the same CDT absorb them.

**Density.** Derive from the boundary rather than from `subdivisions` directly: take the
median 3D length `h` of the boundary segments and choose parameter steps whose 3D image is
about `h` — for a sphere of radius `R`, `Δv = h/R` and `Δu = h/(R·cos v)`. That keeps
interior and boundary resolution consistent, which is what stops the triangulation from
being dominated by the transition between them.

**When to bother.** Only where the patch actually deviates: compare the surface at a
candidate's parameter position against the plane of the triangle that would otherwise cover
it, and insert only if the sagitta exceeds the tolerance. A patch already flat to tolerance
gets nothing, which is what keeps the cost off the primitives that do not need it.

In practice the shipped primitives use many small patches (`makeSphere(1, 8, 12)` is 96),
so Tier 1 alone carries most of them. Tier 2 matters for a single large face — a hemisphere
as one face, or a boolean result whose seam left a broad patch.

## 6. What the reverted attempt established

An attempt at the `(u,v)` step of Tier 1 was made with an **ear-clip** rather than a CDT,
and reverted. It is worth being precise, because it bounds the design:

- **It did help.** Cylinder sliver triangles (quality < 0.01) fell 64 → 16 at subdivisions 4
  and 576 → 496 at 16, at identical triangle count, still watertight. Cap areas became
  exactly the n-gon value at every level — the inflated cap figures in earlier probes were
  side slivers being miscounted as flat.
- **It did not fix the volume.** Still 3.088. An ear-clip is not obliged to avoid long
  diagonals; it takes whatever ear it finds first. This is the direct argument for
  **Delaunay** in step 2 rather than any ear-clip: the empty-circumcircle property is what
  actually forbids the long connection.
- **It broke watertightness.** Eight tests failed, including a real hole in a closed body's
  mesh — caught by `ArcBiteSeam.EveryClosedBodyTessellatesToAClosedMesh`, which had been
  shipped one increment earlier as hardening for a path believed unreachable.

### 6.1 Step 1 result — root-caused, and all three candidates were wrong

The three candidates listed here originally (a self-intersecting `(u,v)` polygon, a bad
`surfaceUV` branch, an ear-clip stall) were each tested and **each disproven**:

- The `(u,v)` polygons are simple — zero self-intersections on every cylindrical face.
- The ear-clipper does not stall — instrumented, no stall on any face.
- There is no missing triangle. Counting mesh edges by index: **zero used once**, and
  **four used more than twice**. It was never a hole; it was overlap.

Counting the same thing on the *committed* build settles it: **six** over-used edges, versus
four with the `(u,v)` change. The defect is **pre-existing and the change reduced it.**

The over-used edges are on the box's *planar* faces, and the triangles responsible have
**area exactly 0.000000** — for example `(0,3,46)` and `(0,46,3)`, the same three collinear
points wound both ways. They come from fanning a face whose boundary contains **collinear
refined points**: refining a straight edge puts intermediate points along it, and a fan from
`ring[0]` connects consecutive points of that same edge.

The mechanism that produced the symptom:

> A zero-area triangle has no defined orientation — its geometric normal is the zero vector,
> so `emitTri`'s `dot(g, nrm) < 0` test compares against nothing and picks a winding
> arbitrarily. It is stable only by accident. Changing how a face is triangulated changes
> that accident, and a directed-edge analysis then reports a closed body as having a
> boundary. The `(u,v)` change did not open the mesh; it disturbed the arbitrary winding of
> triangles that were already wrong.

### 6.2 And they are load-bearing, which is the real finding

The obvious repair — drop degenerate triangles in `emitTri`, since they contribute no
surface — was implemented and **reverted**. It takes the over-used edges from six to zero
and then opens **57 one-sided edges**, with `boundaryLoops` going 0 → 18 on a cylinder and
0 → 746 on a sphere at subdivisions 16.

So the zero-area triangles are not litter. **`toMesh`'s watertightness currently depends on
them**: they are stitching something, and removing them exposes what.

That changes the order of work. Interior sampling cannot be built on a tessellation whose
manifoldness rests on degenerate triangles, because every step of §4 and §5 changes exactly
the connectivity they are compensating for — which is what the reverted attempt demonstrated
in miniature.

**The next question, and the true step 1:** what are those triangles stitching? The evidence
points at a mismatch between the coarse boundary edge and its refined chain — `edgeMid` is
populated per edge and `buildRing` inserts it in traversal order, which *should* be
crack-free, so the disagreement is somewhere between that intent and the emitted triangles.
Find it by taking one closed primitive at `subdivisions = 1`, listing every triangle each
face emits, and checking the boundary-edge coverage rule of §8.4 face by face. Do not
proceed to §4 until a cylinder tessellates watertight **without** any zero-area triangle.

## 7. Parameterisation work required

`surfaceUV` currently inverts **Plane** and **Cylinder** only.

| kind | (u, v) | notes |
|---|---|---|
| Plane | `dot(d, uAxis)`, `dot(d, vAxis)` | done |
| Cylinder | angle, axial | done |
| Cone | angle, axial from apex | `v = 0` is the apex: `u` degenerate there |
| Sphere | longitude, latitude | poles degenerate; `Δu` metric varies with latitude |
| NURBS | — | no general inverse; always falls back |

Both degeneracies are real and must be handled explicitly rather than discovered: a face
touching a cone's apex or a sphere's pole has a `(u,v)` polygon that collapses in `u`, and
its triangulation there is not a planar problem at all. **Scope decision to make before
implementing: handle pole/apex patches, or detect and fall back.** Falling back is honest
and safe (§3) and is the recommended first cut.

## 8. Fallback policy

Per face, in order — any failure falls through to the *current* behaviour, never to a
partial result:

1. Surface kind not invertible → fall back.
2. `surfaceUV` fails on any ring vertex → fall back.
3. Ring contains a repeated vertex, or the `(u,v)` polygon is not simple → fall back.
4. CDT reports failure, or its output does not cover every constraint edge exactly once →
   fall back.

Check 4 is the important one and should be an assertion, not a hope: **the triangulation
must use each boundary edge exactly once.** That is the crack condition stated locally, and
checking it per face turns a whole-body symptom into a per-face diagnosis.

## 9. Cost

Tier 1 is free — same points, better connectivity. Tier 2 adds vertices and triangles where
the sagitta test says they are needed, which is the intended trade. The number to watch is
`classifyPoint`, which calls `toMesh(6)` on *every* query; a tessellation that grows there
slows every boolean. Measure it before and after: the boolean torture batteries are the
existing benchmark, and `KernelPerfSmoke` is where a regression would show.

## 10. How it gets verified

The oracles already exist and have been calibrated in this arc:

- **Convergence, not just a value.** Assert the volume approaches π·r²·h as subdivisions
  rise — the current defect passes any single-level check but fails a series.
- **Analytic vs tessellated agreement.** `massProperties`' analytic path is independent of
  `toMesh`; where both apply they must converge to the same number. This is the oracle that
  found the reversed-face bug, and it does not share the subject's weaknesses.
- **Watertightness at every level** — `EveryClosedBodyTessellatesToAClosedMesh`, extended to
  every surface kind and to boolean results, is the guard that already caught this once.
- **Unsigned area, not signed volume.** Signed volume cancels duplicate triangles; three
  separate defects in this arc were invisible to it and to every topological invariant.
- **Determinism** — the kernel's contract. Grid generation and CDT insertion order must be
  fixed, and the existing determinism tests extended to a curved body.

## 11. Sequence

1. Root-cause the crack from §6. Do not proceed until it is explained.
2. `surfaceUV` for Cone and Sphere, with pole/apex detection that falls back (§7).
3. Tier 1: CDT in `(u,v)` with boundary constraints, behind the §8 fallbacks and the §8.4
   per-face coverage assertion. Expect the cylinder volume to converge to π·r²·h.
4. Extend the watertightness and convergence batteries across all surface kinds.
5. Tier 2: sagitta-gated interior Steiner points; re-measure `classifyPoint` cost.
6. Re-check the centred cylinder-through-box volume identities, which should stop needing
   the analytic path to be correct.
