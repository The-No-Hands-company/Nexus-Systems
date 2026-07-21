# The Nexus Geometry Kernel — A Logbook

*The running record of building a geometry kernel with no room for error, told as it happened: exact arithmetic underneath, a solid modeller and a mesh modeller above, and a long campaign to make every one of them refuse to lie.*

Each **Part** is a body of work. Each **Chapter** is a feature — an arc with a beginning, a problem, and a thing finally proven. The numbered **passages** inside a chapter are the individual increments: one small, tested, committed change at a time. Nothing here is aspirational; every claim below was proven by a test that is green in the suite as this was written.

The exhaustive technical log lives beside this book in the audit memory and the commit history. This is the readable edition — the story, not the changelog.

---

## Contents

- **Part I — Bedrock: arithmetic that cannot lie**
  - 1. Predicates that hold at the boundary
  - 2. Simulation of Simplicity
- **Part II — The analytic solid (B-rep)**
  - 3. A typed topology
  - 4. Primitives that prove themselves
  - 5. The Euler operators
  - 6. Curved on the inside
  - 7. The keystone: a real Boolean
  - 8. Watertight or empty
  - 9. Curved Booleans, honestly
  - 10. History without destruction
- **Part III — The mesh world (World #2)**
  - 11. An exact answer to "inside?"
  - 12. The coplanar seam
  - 13. Winding you cannot trust
- **Part IV — Closing every gap**
  - 14. The five-axis audit
  - 15. Build the thing that finds bugs
  - 16. The dead edge and the double-free
  - 17. A tolerance that scales
  - 18. Nothing sacred, nothing NaN

---

# Part I — Bedrock: arithmetic that cannot lie

A geometry kernel makes two kinds of statement. One is *metric* — "these two points are about a hair apart," a judgement of distance that floating-point rounding is allowed to soften. The other is *combinatorial* — "this point is inside that solid," "this edge crosses that face," "these three vertices turn left." A combinatorial statement is a topological decision, and a single wrong one corrupts the whole model. The governing rule of this project: **metric quantities may be tolerant; combinatorial decisions must be exact.** Everything in Part I exists to make the second half of that sentence true.

## 1. Predicates that hold at the boundary

The bedrock is `RobustPredicates` — adaptive-exact `orient2D` and `orient3D` in the Shewchuk manner: a fast floating-point path that falls back to exact expansion arithmetic precisely when the fast answer might be wrong. These are not decorative. They are wired through the Delaunay and constrained-Delaunay triangulators, the Voronoi builder, the triangle–triangle intersector, and — later — both Boolean classifiers. Where the kernel decides *which side*, it asks a predicate that cannot round to the wrong answer.

## 2. Simulation of Simplicity

Exactness solves the generic case. It does not, by itself, solve the *degenerate* case — the query point that lands exactly on a plane, the ray that grazes exactly through an edge, the pole triangle of a sphere that is exactly coplanar with an interior point. There `orient3D` returns exactly zero, and an exact test with no tie-break simply stalls.

The answer is Edelsbrunner and Mücke's Simulation of Simplicity: perturb the query point symbolically by an infinitesimal $(\varepsilon, \varepsilon^2, \varepsilon^3)$ that no test can ever see as zero, and resolve every tie by the sign of the first non-zero term. Nothing moves; the perturbation is a bookkeeping device. Two building blocks were built and proven against a brute-force perturbation limit — `pointPlaneSideSoS` and `segmentCrossesTriangleSoS`, the latter counting each shared triangle edge exactly once so that a parity ray-cast is watertight. These are the exact heart that Chapters 8 and 11 depend on.

---

# Part II — The analytic solid (B-rep)

The first modeller is a true analytic boundary representation: a solid is typed topology — vertex, edge, coedge, loop, face, shell — bound to exact geometry, not a bag of triangles. This is the CAD paradigm, the one the industry calls Parasolid or ACIS. Building it took the better part of the project, and it is the part that is now genuinely solid.

## 3. A typed topology

`brep::Body` holds the topology and two validators that are the conscience of the whole subsystem. `checkIntegrity()` walks the raw half-edge invariants — coedge partners reciprocal and opposite-oriented, loop rings closed, no live element referencing a dead one. `checkGeometry()` is a separate oracle: it proves the analytic geometry *agrees* with the topology — that each edge curve reproduces its endpoint vertices, that normals are unit, that nothing is non-finite. The two are deliberately distinct, because topological validity is not geometric consistency, and a stale curve after a vertex moves will pass the first and fail the second.

## 4. Primitives that prove themselves

Box, cylinder, cone, and sphere were each built as analytic solids and — this is the point — *validated by the checkers, not by eye*. Every winding was derived so that shared edges traverse in opposite directions; every vertex was placed exactly on its analytic surface; every result was confirmed watertight with Euler characteristic 2 for any segment count. A face may also lie on an exact NURBS surface, evaluated through the existing NURBS toolkit. The primitives do not merely look right; they carry a proof that they are.

## 5. The Euler operators

Topology is edited only through Euler operators that preserve the manifold invariants: `splitEdge` and its inverse `joinEdges` (make- and kill-edge-vertex), `splitFace` and its inverse `mergeFaces` (make- and kill-edge-face). Each was proven χ-neutral across every edge and face of every primitive, and each make/kill pair was proven *invertible* — split then join restores the exact live counts. A liveness system (tombstoning with an "no live entity references a dead one" guard) makes removal safe. This is the algebra a solid modeller is built from, and it is closed and reversible.

## 6. Curved on the inside

A cylinder whose sides are flat is not a cylinder. Edges can be retagged as circular arcs — the ring edges of a cylinder, both the small-circle latitudes and the great-circle meridians of a sphere — with the parameter range set so the curve still reproduces its endpoints and `checkGeometry` still holds. `toMesh(subdivisions)` then places intermediate points *along the shared edge curve*, so both faces meeting at an edge subdivide identically: crack-free at any level, and every side vertex exactly on the analytic surface.

## 7. The keystone: a real Boolean

This is the chapter the whole B-rep was building toward. A regularised Boolean of two solids assembles every prior increment. First **surface–surface intersection** returns exact analytic curves for the common pairs — plane∩plane a line, plane∩sphere a circle, and so on. Then **imprint** cuts each body's faces along those curves, introducing the intersection curve *itself* as a shared edge, so the seam lies exactly on both surfaces. Then **mutual imprint** iterates to a fixpoint until no face straddles the other solid's boundary. Then **classify** decides each face inside/outside the other by an exact point-in-solid test. Then **select** keeps the faces the operation wants, and **sew** re-assembles them — welding the shared seam so the two bodies' patches share edges — into either a welded triangle mesh or a first-class analytic `Body` that can feed another Boolean. Union, intersection, and difference were proven by the divergence theorem and the topology validator: exact volumes, closed, genus-0, and chainable.

## 8. Watertight or empty

A Boolean that is *usually* watertight is not a kernel; it is a demo. So the output was hardened to an invariant. The hard-won lesson, recorded in capitals in the log: **`checkIntegrity().ok` is not watertightness** — it permits boundary edges, so an open shell passes it. A solid result must *also* be closed. `booleanToBody` now requires `checkIntegrity().ok && isClosed()`, or it returns a clean empty body — never a leaky one. A second lesson followed: the Euler characteristic of a valid Boolean is *not* fixed. Two cubes touching at a corner are a legitimate result with an odd Euler number; the invariant is watertight-or-empty, not any particular χ. Torture batteries — grazing, near-coincident, rotated, face-touching, chained three-way, mixed primitives, and difference-splits-into-two-pieces — proved the contract holds across hundreds of near-degenerate configurations.

Later, a torture aimed specifically at *near-tangency* found the one way the invariant could still be defeated — not by a wrong answer, but by no answer at all. A faceted sphere set to graze a box face by a ten-thousandth of a unit made the sphere's facet planes imprint a full line arrangement onto that face — a cut per pass, faces climbing past a thousand and still climbing, the Boolean not crashing and not finishing, simply grinding. A hang is a correctness failure wearing a disguise. The imprint now carries a work budget proportioned to the inputs; when a degenerate tangency blows it, the imprint abandons the attempt and the Boolean returns a clean empty body — the same watertight-or-empty contract, now with *bounded* stapled to the front of it. The pathological case that ran without end now finishes in under a tenth of a second, and the surface–surface intersection underneath it was checked, at every grazing offset down to a millionth of a unit, to return a curve that lies exactly on both surfaces or an honest nothing — never a plausible-looking lie.

## 9. Curved Booleans, honestly

Not everything closed. A cylinder built with true `Cylinder`-surface sides cannot yet sew through a Boolean, because the intersection produces inner-loop holes the reassembler can't yet rebuild. Rather than pretend, the log records the gap precisely — and records the pragmatic path that *does* work: build faceted curved solids with all-planar faces (an n-gon prism, a polygonal UV-sphere) and the existing planar Boolean handles them perfectly, converging to the smooth answer as facets grow. An AABB broad-phase later made this ~190× faster by refusing to imprint far faces whose infinite planes cross the target but whose actual boundary is nowhere near.

## 10. History without destruction

Finally the B-rep became non-destructive. A `FeatureStack` records the parametric history — base primitive, then transforms, chamfers, fillets — and `evaluate()` replays it from scratch, so editing a chamfer's setback and re-evaluating is pure and repeatable. The stack serialises to a versioned binary format that round-trips both the history and the result. A solid modeller is not just shapes; it is the recipe, kept.

---

# Part III — The mesh world (World #2)

The second modeller is the polygon/DCC world: triangle meshes, cut and classified and stitched. It shares the exact bedrock but is a wholly separate code path, and when this book's most recent work turned to it, it was roughly where the B-rep Boolean had been before its rebuild — plausible-looking, not yet exact. Three chapters brought its *decisions* up to standard; a fourth arc, still open, is its seams.

## 11. An exact answer to "inside?"

The mesh Boolean classified each cut sub-triangle by shooting a floating-point ray from its centroid and counting crossings. Sub-triangle centroids sit right against the cut seam, so that ray could flip and misjudge a triangle. The classification was replaced with the exact Simulation-of-Simplicity ray parity from Chapter 2 — the same watertight-parity core proven for the B-rep. A 156-configuration torture probe then established the honest scope: the decision was now exact and deterministic, but the surface still leaked on coplanar seams. The classifier was not the whole story.

## 12. The coplanar seam

The probe's verdict was blunt: 108 of 156 near-degenerate configurations leaked. The dominant cause was coplanar-overlap faces, which the cut stage silently dropped. Two coordinated fixes followed. The cut now *imprints* each coplanar triangle's edges onto the other using exact `orient2D`, so both surfaces share seam vertices along the overlap. And the classifier gained coincident-face resolution — the B-rep's `selectFace` rule table, ported over: probe just outside each face along its normal, and for a coincident pair keep exactly one copy. Leaks fell from 108 to 30; axis-aligned coplanar box Booleans became watertight and volume-exact where before they had holes.

## 13. Winding you cannot trust

One residual class was subtler. The coincidence test assumed a sub-triangle's geometric normal pointed outward — but the retriangulator does not guarantee winding, and a flipped sub-triangle inverted the decision and dropped a face, opening an operand-order-dependent hole. The fix makes the classification winding-independent: re-orient each normal outward by an exact self-test before deciding. Axis-aligned coplanar Booleans then became watertight in *both* operand orders. What remains — rotated coplanar/transversal junctions, where two independent triangulations don't share their diagonals — is diagnosed and named: it needs a shared-seam-triangulation rebuild, the mesh world's analogue of the B-rep's imprint/sew. That arc is open, and the book will have a chapter for it when it closes.

---

# Part IV — Closing every gap

With both modellers substantially exact, the work turned from building features to hunting for cracks in the foundation — deliberately, systematically, and with a tool built for the purpose. This part is the campaign, and it has been the most productive stretch in the whole project: five real defects found and fixed, most of them invisible to every hand-written test that came before.

## 14. The five-axis audit

An honest survey along five axes: robustness (exact predicates ✅, but a tolerance model only partly adopted), performance (deferred by choice, with real gaps in acceleration), topology guarantees (solid for the B-rep, an open arc for the mesh), extensibility (no representation-agnostic abstraction — flagged as a decision to make, not reflexively patch), and testing (thorough but entirely hand-picked). The audit produced a ranked, sequenced plan — and its first move was not to fix any single gap, but to build the instrument that would find the ones nobody had guessed.

## 15. Build the thing that finds bugs

The instrument is a seeded, deterministic fuzz harness. It randomises over primitive pairs, transforms, and operator sequences and asserts only the invariants that genuinely hold: the B-rep Boolean is watertight-or-empty and byte-deterministic across hundreds of random rotate/scale/translate configurations; the mesh Boolean is finite and deterministic; every half-edge operator preserves integrity or refuses. On its very first run it found a real bug — four half-edge operators range-checked their index but did not reject a *tombstoned* one, so iterating the raw index space after any edit corrupted live topology. Fixed with a liveness guard. The tool paid for itself on run one.

## 16. The dead edge and the double-free

Extending the fuzzer to all thirteen index-taking half-edge operators found something worse. `insertEdgeLoop` built two parallel arrays but skipped boundary edges while filling them — then iterated by the *un-skipped* length and indexed past the arrays' true size, writing through garbage indices into live memory. A reachable heap corruption, observed as an actual double-free, invisible to every single-operator test because a clean mesh never triggers the skip. Fixed to iterate the real length, and a public liveness query (`isLiveEdge`/`isLiveFace`) added so callers can skip dead slots at all. The lesson, recorded: this was a memory-safety bug, strictly worse than a topology violation — exactly the category hand-picked tests never reach.

## 17. A tolerance that scales

The audit's number-one crack was a scale-blind tolerance model: a central, scale-aware `Tolerance` existed but ~180 fixed epsilons across the code ignored it. Two of them were proven, by probe, to be *reachable* bugs. The mesh Boolean's seam weld used a fixed `1e-5`, which cannot merge coincident seam vertices on a large model — so a Boolean watertight at unit scale leaked at scale 100 and beyond; made proportional to the model's coordinate span, it now holds from a 2-metre box to a 10-kilometre one. And the degenerate-triangle test compared a length⁴ quantity to a fixed floor, so it dropped valid sub-millimetre triangles and made small-scale Booleans fail with an empty result; made scale-invariant — area against the triangle's own edge² — it now holds from 0.05 mm to 5 km. The discipline that emerged: *verify the epsilon's bug is reachable before migrating it.* One candidate was investigated and deliberately left alone, because its only failing regime was one where a matrix inverse failed anyway — you do not migrate an epsilon whose change you cannot prove improves a real result.

## 18. Nothing sacred, nothing NaN

The last chapter of this edition pointed the fuzzer at malformed *inputs*: non-manifold edges, open shells, degenerate and duplicate and self-intersecting geometry, and positions that are NaN or infinite — fed through every public entry point. Nothing crashed. But two operations, the vertex weld and the mesh repair, silently passed non-finite positions straight into their output, unlike the Boolean, which rejects them. Emitting "welded" or "repaired" NaN geometry is a quiet footgun for whatever consumes it next, so both were brought into line with the kernel's convention: the weld rejects non-finite input and returns empty; the repair declines and reports failure. The malformed-input battery now guards that guarantee for good.

That fix had a sequel, because a leak found in two operations is rarely confined to two. The same NaN-input probe, turned on the wider processing surface, gave a blunt verdict: six of the seven ops it reached passed the poison straight through — decimation, thickening, Laplacian smoothing, both flavours of displacement, and Catmull–Clark subdivision all emitted NaN geometry; only the quad-remesher, which rebuilds from a fresh grid, came out clean. None crashed, which is why none had ever been noticed. Each was given the same three-line guard at its entrance — a mesh-returning op refuses non-finite input and returns empty; the subdivider declines and returns nothing — and the battery grew to feed the poison through all of them and insist the output stay finite. The convention is no longer a habit two operations happened to follow; it is enforced across the surface.

---

*This edition ends here, but the logbook does not. The mesh world's shared-seam rebuild, the rest of the tolerance migration, the representation-abstraction decision, and the performance work each have a chapter waiting to be written — one proven passage at a time.*
