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

One residual class was subtler. The coincidence test assumed a sub-triangle's geometric normal pointed outward — but the retriangulator does not guarantee winding, and a flipped sub-triangle inverted the decision and dropped a face, opening an operand-order-dependent hole. The fix makes the classification winding-independent: re-orient each normal outward by an exact self-test before deciding. Axis-aligned coplanar Booleans then became watertight in *both* operand orders. What remained was a stubborn residual of about thirty near-degenerate configurations, and the temptation, having named a plausible cause once, was to trust the name.

So before touching a line of the seam, the residual was put under a microscope. The story that had been told about it — that the two operands' independent triangulations left *T-junctions*, a vertex of one surface stranded on the interior of the other's edge — turned out to be wrong. A boundary-edge audit across the whole torture battery found not a single T-junction. Nor were the gaps unwelded coincident vertices: welding the output a hundred times more aggressively closed nothing. The residual was, in plain terms, two things and neither of them the guessed one — about two dozen genuine *holes*, faces simply missing where the two independently-cut surfaces failed to tile the seam between them, and a handful of *non-manifold* seams where a coplanar overlap left a face doubled. A flat cap over a hole would be watertight and geometrically false, which for this kernel is worse than a hole; the honest fix is to generate the missing seam faces correctly, from a seam both operands share. That is a real chapter's worth of work, not a paragraph's. This one ends by pinning the corrected map — the exact residual count, and a standing guard that no future change may quietly re-introduce a T-junction — so the rebuild that follows measures itself against the truth rather than the guess.

There was a hope, in that map, that one of the two classes might be cheap. The non-manifold seams — an edge with too many faces on it — looked like nothing more than a doubled triangle left over from a coincident overlap, the kind of thing a pass of duplicate-removal sweeps up in an afternoon. It was not. Deduping identical triangles removed exactly none of them, because they are not duplicates: they are three distinct sheets meeting along one edge, or, in the reversed operand order, two sheets pinched together at a single point — a *bowtie* vertex, where every edge is still shared by exactly two faces yet the surface is not a surface. The map was completed to name all three shapes it can take — a hole, a crowded edge, a pinched vertex — and the completion carried a verdict: they are one wound seen from three angles. Two surfaces were cut apart along a seam and never told to share it; a hole is where their triangulations missed each other, a crowded edge is where they overlapped, a bowtie is where they touched at a corner and nowhere else. There is no cheap corner of this problem to pick off first. The cure is singular — a shared seam — and it is the work of the chapters to come.

The work began, as the discipline demands, by probing rather than assuming. The first probe overturned the framing that had guided the last several passages: each operand's cut surface, taken alone, is *already watertight*. The fault was never in cutting an operand — it was entirely at the junction where the two kept surfaces are asked to meet. So the two surfaces were fed the identical shared seam — the exact overlap polygon, the same segments to both — and it changed nothing; a probe confirmed the correct cut line was delivered and the hole stayed anyway. That thread, pulled, led one layer down, to the retriangulator that turns a cut triangle into sub-triangles along its seam segments. It was inserting the seam's *endpoints* and then triangulating straight across the seam's *edge* — leaving a sub-triangle that straddled the very line it had been told not to cross. The sharp irony: that straddle, welded into the output, registered as a *T-junction* — the one defect the map had sworn under oath was gone.

The crack was in the constrained-Delaunay triangulator itself, the small exact engine every seam retriangulation stands on. To force a required edge into a triangulation you flip the edges that cross it; this one flipped them *blindly*, with no check that the four points around the crossing edge form a convex quadrilateral. Flip a non-convex quad and you do not recover the edge — you fold two triangles over each other, and the "triangulation" now overlaps itself. A fuzz over sixteen thousand random inputs said it plainly: nearly a quarter of all constraints were never enforced, and one in seven triangulations came out with an inverted, overlapping triangle. This was not a seam bug. It was a foundation crack running under everything the seam is built on.

The repair is the textbook algorithm, executed exactly. First, if a vertex lies precisely on a constraint — the collinear case that flipping can *never* resolve, because there is no crossing edge to flip — split the constraint at that vertex and recover each half. Then recover the edge by flipping only quads that are strictly convex *and* whose flip makes progress, so the new diagonal no longer crosses the target; Anglada's theorem guarantees such a flip always exists while any edge still crosses, so it terminates and it recovers. Every sidedness decision is an exact `orient2D`; nothing here trusts a float. The fuzz's inverted triangles went to exactly zero, and unenforced constraints fell from thousands to a few dozen — and those few proved to belong to a *different*, older gap in the unconstrained pass, not to this one. The dividend arrived downstream without being asked for: the mesh Boolean's leaks fell by half in a single change — thirty configurations to fifteen — the bowtie class vanished entirely, and the rotated-box Booleans that had leaked for a dozen increments closed and stayed closed. What is left is the harder residual the chapter opened on: a coplanar cap meeting a side wall at a genuine three-way junction. But the seam is no longer fighting a triangulator that could not hold a straight line.

There was a flaw hiding inside that repair, though — the kind only a fuzz that never fully closes will surface. "Flip only quads whose flip makes progress" is *almost* right, and the word doing the damage is *only*. A few of the surviving failures were not the older gap at all; they were the recovery itself giving up. In these configurations three edges cross the constraint, only two sit in a convex quad, and neither convex flip makes immediate progress — flip either and the new diagonal still crosses the line. A rule that flips *only* progress-making edges looks at that and declares defeat, though the edge is perfectly recoverable. The escape is the one Sloan wrote down: you sometimes must flip a convex edge that makes *no* apparent progress, because it rearranges the crossings so the *next* flip can. It is safe — a convex flip can never *increase* how many edges cross the constraint — and it terminates, because processing the crossings in order from one endpoint toward the other always advances the front. So the rule was corrected to flip the convex crossing edge nearest the constraint's start, progress or not. The two configurations that used to deadlock now recover cleanly, every triangle still wound the same way. The count barely moved — thirty-three failures instead of thirty-four — but that number was never the point: the remaining failures are now, provably, all one thing, the near-collinear boundary point that the *unconstrained* triangulation leaves stranded. That is a crack in a different wall, named and waiting.

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

Two operations remained, with a different shape of output, and one of them hid the worst offence of all. The decimator, handed a mesh with a poisoned vertex, quietly folded the poison into its simplified result. The remesher did worse: it returned a report that said *valid* over a mesh full of infinities — a lie with a certificate attached. A downstream consumer trusts a success flag; handing it NaN geometry under a green light is exactly the failure the convention exists to prevent. Both now reject at the door — the decimator returns nothing, the remesher declines and marks the input invalid — and with that the sweep is complete: every public operation that turns geometry into geometry either produces finite output or honestly refuses.

There was one loose thread the audit had noticed and left for the next pass: two headers, one for turning a mesh into voxels and one for turning voxels back into a mesh, each declared a type called *VoxelGrid* — and the two declarations were not the same type, merely the same name, so any file that needed both refused to compile. They were never meant to be one thing; the occupancy grid a voxelizer fills and the signed grid a surface-extractor reads are different animals. The collision was resolved by giving the second its own name, and a test that includes both headers at once now stands guard against the clash ever returning. The voxelizer, while its neighbourhood was open, learned the same manners as the rest: hand it a mesh with an infinity in it and it returns an empty grid rather than a plausible-looking wrong one.

## 19. The triangle that was too small

The mesh Boolean's remaining leaks had been traced, chapter by chapter, down through the seam and into the triangulator beneath it. The last reading left a single suspect: a near-collinear point that somehow left a hull edge missing from the Delaunay triangulation, in a state where nothing crossed that edge, so no amount of edge-flipping could ever recover it. The diagnosis named the symptom precisely. It was still a layer too high.

Bowyer–Watson begins by wrapping the input in a *super-triangle*, inserts every point, and deletes the wrapper at the end. The wrapper is only safe if it lies outside the circumcircle of every triangle the true triangulation contains. Circumradius is `abc/(4A)`, and a sliver — three points that very nearly lie on one line — has an area so small that its circumcircle grows without bound. Points spanning a single unit that deviate from their line by a millionth need a wrapper about a million times the bounding box. Both triangulators had hard-coded a small fixed multiple: twenty times for the constrained one, three-quarters for the unconstrained. Below the threshold the sliver's circumcircle simply swallows the wrapper's corners; the sliver is never emitted; and deleting the wrapper at the end takes real area with it.

The scale sweep settled it in one table. A five-point sliver returned **zero triangles** at the shipped scale, two at a hundred times, and the correct four from a thousand times upward. Measured across four hundred random thin point sets per decade of thinness, nearly every input thinner than a thousandth had been losing its entire triangulation — not a hull edge, the whole thing. The missing hull edge was the mildest visible symptom of a triangulator that, on thin input, returned nothing at all.

No constant could fix it, and that is the interesting part. The wrapper size that works is dictated by how *thin* the input is, not how *large* it is — so any fixed multiple of the bounding box is beatable by making the input thinner, and the failure had nothing to do with the scale-awareness that closed earlier cracks. What replaced it is a triangulator that checks its own work: build at a scale, verify the result tiles the convex hull of the input, and grow the wrapper and rebuild if it does not. The first scale tried is the historical one, so well-conditioned input costs exactly one build and comes out bit-for-bit as before, and the retry terminates because a finite point set with any non-degenerate triple has a finite largest circumradius.

> Measure the thing you are about to fix with a formula that survives the regime you are fixing.

That warning is earned. The hull areas being compared were first computed with the shoelace formula — the obvious choice, and the wrong one. Shoelace multiplies absolute coordinates and subtracts nearly-equal products, so on a sliver, whose area sits many orders of magnitude below its coordinates, it throws away most of its significant digits: four parts in ten thousand of error on a triangle spanning one unit with an area of a millionth. That is more than enough to invent a hull gap that does not exist, and it sent this increment chasing a phantom for an hour before the same triangle, measured two ways, gave the game away. Differencing the coordinates first — the orientation predicate's own arrangement, exact for float input — measures it properly. It also overturns an earlier verdict recorded in this logbook: a hull under-fill previously dismissed as noise in the probe was never noise. The probe was wrong; the under-fill is real.

**What was proven.** Across sixteen thousand random constrained inputs and eight thousand thin point sets: vertices dropped by the triangulator fell from 15,396 to none; constraints the CDT failed to enforce fell from 40.1% to 0.62%, with the dominant class — an endpoint that reached no triangle at all, and so could never be flipped into place — eliminated entirely. The randomized fuzz battery's mesh-Boolean watertightness leaks went from two in a hundred and eighty to zero, and that count graduated from a tracked metric to an assertion.

And the honest remainder: a second defect, independent of the first, is now separated out and pinned rather than claimed. At extreme thinness the cavity retriangulation can still under-fill the hull — one point in four hundred sets, where a vertex's deviation from a hull edge was measured at five parts in a thousand trillion, right at the resolution of a float. Insertion treats such a point as interior while the exact hull places it outside, and no wrapper size reconciles them. It is a different mechanism with a different fix, it is recorded with a number so the next pass can drive it down, and it is not part of what this chapter claims.

## 20. The bedrock was not bedrock

The previous chapter closed with an honest remainder: at extreme thinness the triangulation still under-filled its own convex hull, and the suspicion was recorded as a second defect somewhere in the Bowyer–Watson cavity. That suspicion was wrong, and finding out why exposed the deepest fault this kernel has had.

The cavity was investigated first, because that is where the fault was supposed to live. There are exactly three ways the cavity step goes wrong: the set of triangles whose circumcircle contains the new point can be empty, or disconnected, or not star-shaped when seen from that point. Each was instrumented and counted across every insertion, at every decade of thinness. All three came back zero. The cavity was textbook-clean.

Which left only one possibility. If the cavity is computed correctly from the set of bad triangles, and the result is still wrong, then the *set of bad triangles* is wrong — and that set is decided entirely by one call to `inCircle`.

The kernel's very first recorded strength, the thing the five-axis audit listed as its number-one solid foundation, was that its geometric predicates were genuine Shewchuk adaptive-exact arithmetic. Everything else was built in that confidence: the Delaunay insertions, the constrained recovery, the boolean's inside/outside classification, the Simulation-of-Simplicity tie-breaks that Chapter 2 built on top. So the predicates were finally measured — not read, measured — against an independent exact reference computed in 113-bit arithmetic. All three were wrong on near-degenerate input. Not ties, not close calls: **wrong signs**, at roughly four in a thousand sliver configurations for the two-dimensional orientation test, five in a thousand for the three-dimensional one, and four in a thousand for the in-circle test.

The reason was in plain sight in the source, under a comment citing Shewchuk. Two of the three functions named `…Exact` contained no expansion arithmetic whatsoever. They computed six floating-point terms of wildly different magnitude, added them up in whatever order they were written, and then — if the result happened to fall below a hard-coded absolute `1e-10` — re-added the same terms sorted *largest-first*, which is precisely the ordering that maximises cancellation, and returned that. The third did use expansions, but drove them through a helper with its parameters transposed. The floating-point fast paths in front of them were guarded by error bounds that were not provably conservative either.

> The audit read the file's structure — the helper functions, the names, the citation — and concluded exactness. It never tested a sign against an independent oracle. Reading code for provenance is not verification.

The rebuild is the honest version of what the comment always claimed. An expansion represents an exact value as a sum of nonoverlapping doubles in increasing magnitude, so its sign is simply the sign of its largest component; exact two-sum, two-difference and two-product operations each capture their own rounding error, so nothing is assumed about the operands' exponent range and the result is exact for any double input, not merely for the floats the kernel currently stores. Shewchuk's published error bounds guard the fast paths. His intermediate adaptive stages were deliberately left out: they are optimisations that refine the estimate before falling through to exact, and every one of them is another delicate error bound with another chance to be quietly wrong — which is exactly the failure being repaired. Two stages carry the same guarantee with far less that can lie.

And the hull under-fill vanished with it. It had never been a cavity defect at all; a correctly-computed cavity was being built from an incorrectly-chosen set of triangles. The measurement that had been pinned one chapter earlier as a known residual — a hundred and twenty-five thin point sets in four hundred failing to tile their own hull — went to zero, and stayed there as an assertion, because it is the sharpest end-to-end witness the suite has: a wrong sign inside `inCircle` shows up as missing area long before it ever surfaces as a leaking Boolean.

**What was proven.** All three predicates now agree with an exact reference on every one of 1.2 million adversarial configurations, against 2,609 wrong before. An exactly degenerate configuration returns exactly zero, so Simulation of Simplicity has a real tie to break; one unit in the last place either side of a degeneracy resolves in the correct direction. The guard is portable and does not depend on any wide floating-point type — integer-valued coordinates, bounded so the determinant fits exactly in a 64-bit integer, give an exact oracle on every compiler. The cost is honest: the fallback is real work now rather than a cheap approximation, and the kernel suite went from ten seconds to fourteen and a half.

## 21. The offset that could not exist

The previous chapter ended by noting that nine seam leaks were still outstanding, and that they were now worth re-diagnosing rather than fixing, because the diagnosis on file had been reached while the predicates underneath it were returning wrong signs. A map drawn with a broken instrument is not a map you correct; it is a map you redraw. So the mesh Boolean's defect map was measured again from nothing, assuming none of its predecessor.

Three of the old map's claims did not survive. The residual had been recorded as dominated by a "coplanar-cap three-way junction", with the fix being a shared-seam reconstruction — a substantial piece of work that was next in the plan. It was aimed at nothing: box-against-box Booleans in general position turned out to be essentially watertight already, two leaks in thirteen hundred runs across a systematic sweep and randomized operands of unequal size and arbitrary rotation. The entire box residual sat in a narrow band of near-coincident offsets, a different defect wanting a different fix. The old map also recorded that no leak was a T-junction; that was true only because its battery contained nothing but boxes. And it had never measured curved or finely-tessellated operands at all — which is where the largest defect turned out to live. One claim did survive, re-tested rather than inherited: each operand's cut is individually watertight, so whatever was going wrong was happening after the cut.

The redrawn map had two live classes. Near-coincident coplanar faces leaked in a narrow band of separations. And the leak rate on a sphere cut against a box climbed with tessellation density — twelve per cent at six by ten, twenty-two at twelve by sixteen, forty-eight at twenty-four by twenty-eight, sixty-seven at thirty-two by thirty-six — on inputs asserted watertight and genus-zero beforehand, with clean cuts. A cylinder of comparable face count, all-planar, leaked nothing at all, which said the defect tracked the size of the features the seam had to resolve rather than the number of faces. Both classes were scale-invariant, so neither was a stray absolute epsilon awaiting the tolerance migration.

The cause was a single quantity. To decide whether to keep a sub-triangle, the classifier stepped off its centroid along the normal by a small distance and asked the exact point-in-solid test on both sides, using the two answers both to place the face and to detect coincidence with the other surface. That made a combinatorial decision — keep or discard — depend on choosing that step well. And it cannot be chosen. It must be large enough to clear the floating-point noise in the cut geometry, and smaller than the distance to the nearest other sheet of surface. The implementation took it from the model's bounding box, while the distance to the nearest sheet is a local quantity that shrinks every time a model is tessellated more finely. Measured, the smallest sub-triangle the cut produces falls from three parts in ten thousand at the coarsest sphere to nine parts in ten million at the finest, while the step stays fixed — at the extreme, two hundred times larger than the triangle it is stepping off. The probe lands past another sheet of surface, the exact test faithfully answers a question about the wrong region, and the face is kept or dropped wrongly. Both classes scale-invariant follows immediately: step and features scale together, so the ratio, and the damage, stay exactly where they are.

The first repair attempt made it worse — deriving the step from each sub-triangle enlarged it for ordinary faces, and the rate went from sixty-seven per cent to seventy-five. Capping it instead of replacing it halved the rates, which was progress and still not a fix: at the finest tessellation the slivers are smaller than the noise floor itself, so the two requirements have no overlap and no value satisfies both. The step could not be chosen because no correct step exists.

> When a combinatorial decision depends on a length you have to tune, the bug is not the length. It is that the decision was made to depend on one.

What removed it was the rule this logbook opened with: metric quantities may be tolerant, combinatorial decisions must be exact. Two questions had been fused into one probe, and they are not the same kind of question. *Which side is this face's material on* is combinatorial, and is now answered by the exact Simulation-of-Simplicity parity at the centroid itself, with no offset whatsoever — the perturbation of Chapter 2 already resolves a centroid lying exactly on the other surface, consistently, which is precisely what it was built for. *Is this face coincident with the other surface* is metric, a genuine measurement, and is answered by the distance from the centroid to that surface within a scale-aware tolerance — together with a check that the two surfaces are locally parallel, because distance alone misreads ordinary seam slivers, every one of which is close to the other surface by the definition of a seam. A face's outward direction now comes from the nearest original triangle of its own solid, which carries the input's consistent winding, retiring a third probe that had the same impossible requirement.

Both classes fell to it. Class two — the large one — is closed through moderate tessellation, zero leaks at every density the old map would have called dense. Class one closed as well, unbilled: the near-coincident coplanar faces had been failing to the same probe all along, which is why the redrawn map found them together.

**What was proven.** Across 2,593 Boolean runs the leak count went from thirty-eight to one: the pinned battery from nine to zero, randomized box-against-box from one to zero, curved operands from twenty-eight to one, the systematic sweep holding at zero throughout. Sphere-against-box is watertight at six by ten, twelve by sixteen and sixteen by twenty. Two residuals are recorded rather than claimed: a narrow band where the separation between coplanar faces is about equal to the coincidence tolerance, which is the irreducible ambiguity of deciding coincidence by measurement and is properly solved by snapping geometry to tolerance before the Boolean runs; and three runs in sixty at the very finest sphere, down from twenty-nine, where the cut emits sub-triangles a few units in the last place across and the geometry itself has run out of precision to be classified by. That last one is a question about the quality of the cut, not about classification, and it is the next chapter's problem.

## 22. The triangle that was not a T-junction

The previous chapter closed the mesh Boolean's seam almost everywhere and left one thing standing: at the finest tessellations a few results still came back open. It was recorded as a question about the quality of the cut, and it was — but the route from there to the answer ran through two wrong diagnoses of my own, and the interesting part of this chapter is how each was caught rather than what the fix turned out to be.

The hunt began by looking at what the cut emits at high density, and found something else entirely. The vertex weld was all-or-nothing. Deep in the loop that remaps faces onto their merged vertices sat a single `return false`: if welding would collapse *any* one face, the entire weld was abandoned and the mesh left untouched. On a finely tessellated Boolean the cut always produces some sliver whose two ends fall within tolerance, so one degenerate triangle anywhere meant that not a single vertex in the entire result was welded. The surface came apart into loose triangles — 5,958 boundary edges from 1,986 faces, which is exactly three per face, the arithmetic signature of nothing having welded at all.

What made this instructive is that the leak *count* barely noticed. Before the repair, twelve of sixty configurations leaked; after it, eleven. The number the seam work had been steering by was blind to a defect that was destroying whole results, because it asked only whether a result leaked and never how badly. Measured by severity instead — total boundary edges across the battery — the same change reads 10,926 down to 80, and the worst single result 5,958 down to 16.

> A metric that refuses to move is sometimes telling you about the metric.

The repair itself was a policy rather than a silent change of behaviour: refusing the whole weld stays the default, since for general editing a weld that quietly deletes faces would be the greater surprise, while the Booleans and the cut opt into dropping just the collapsed face. That is sound and not a fudge — removing a collapsed triangle is an edge collapse, its two surviving corners become the same undirected edge, the neighbours across them stay matched, and a closed surface stays closed. The box says so directly in a test.

That left the actual residual, and here is the first wrong turn. Measuring the surviving holes reported that a third of them were T-junctions — a seam edge split on one operand and not the other — which is a real and well-known failure of cutting two meshes independently. A conforming pass was written against that diagnosis, wired into the cut, and measured. It changed nothing at all: leak rate, severity and the cut's own watertightness were byte-for-byte identical with and without it. Instrumenting it showed it was finding the boundary edges and then finding nothing on them to split.

The diagnosis had been wrong, and the fix proved it. The probe behind the T-junction claim had searched every vertex for one lying on a boundary edge's interior without excluding that edge's *own opposite corner*, so every degenerate triangle in the mesh had been counting itself as evidence of a T-junction. The conforming pass was reverted rather than left in as dead code that looked like diligence.

Measured correctly, every residual boundary edge belonged to a **cap**: a triangle whose third corner sits on its own opposite edge, three hundredths of a millionth of a unit off an edge an eighth of a unit long. With exact predicates those three points genuinely are not collinear — only collinear to within what a float can represent — so emitting a valid, degenerate triangle is the honest answer to the question as posed. Where the caps come from is the retriangulation: when a seam point lands a hair off an edge of the triangle being cut, the triangulator returns both the correct sub-triangles that split through that point and a cap spanning the whole unsplit edge.

And here is the second wrong turn, recorded in the previous chapter's own commit. I had written that deleting a cap could not be the fix, because it is the only user of its long edge but shares its other two, so removing it would turn one unpartnered edge into two. That was reasoning from an assumption instead of from the mesh. Printing the neighbourhood of an actual cap showed its two shared edges were used *three* times each — its own use, plus the two genuine triangles that had correctly split at that point — while its long edge was used once. Deletion takes those edges from three to two and removes the long edge altogether. Deletion had been exactly right the whole time; I had talked myself out of it with an edge count I never checked.

The filter that drops them measures thinness as twice the area over the sum of the squared edge lengths — a dimensionless ratio, about 0.29 for an equilateral triangle and tending to zero as one degenerates, so it means precisely the same thing on a half-millimetre part and a five-kilometre terrain without any bounding box being consulted. The threshold was measured rather than chosen. Across 54,643 sub-triangles of a cut sphere the caps occupy a band from one to four and a half parts in ten million, the next triangle that is not a cap sits at three parts in a million, and the thousandth-percentile of ordinary geometry is two and a half parts in ten thousand. One part in a million falls in that gap with room on both sides. A first attempt set it ten times stricter, left nine caps behind, and that failure is what produced the measurement instead of a guess.

**What was proven.** The residual is closed rather than reduced: sphere against box goes from five per cent to zero at twenty-four by twenty-eight and from eighteen to zero at thirty-two by thirty-six, the operand cuts from five of twenty not watertight to none, and the boundary-edge damage from seventy-seven to nothing. Every tracked seam measurement now reads zero — general position, every curved density from six by ten through thirty-two by thirty-six, extreme tessellation, caps emitted, and the randomized fuzz battery. It holds past what it was tuned on, with no leaks at forty-eight by fifty-two or sixty-four by sixty-eight, at model scales of one and of a hundred; the single exception, stated and not buried, is sixty-four by sixty-eight at scale one hundred, which still loses two of twenty. Volume is conserved, which is the guard that a filter which deletes triangles is not deleting anything real: the union and intersection volumes still sum to the operands' to within four parts in a million, float precision, and that is asserted rather than assumed.

## 23. The tree that never grew

With the seam closed, the next question was no longer whether the Boolean was right but whether it was usable. It was not. Timed against operand size it came out quadratic: fifty-one times the triangles cost four hundred and seventy-six times the time. A model of the size a real tool opens would have taken minutes for a single operation. Profiling put two thirds of that in classification, and inside classification the culprit was the pair of queries each sub-triangle asks — where is the nearest surface, and is this point inside that solid — each of which walked every triangle in the operand.

That is precisely what a bounding volume hierarchy exists to prevent, and the kernel has had one from early on: `MeshBVH`, with surface-area-heuristic construction, a ray cast and a nearest-point query. Wiring it in should have been an afternoon. Instead the results changed — and a Boolean whose output changes when you add an *acceleration* structure is telling you something about the structure.

It was broken in two independent ways, and the more interesting relationship is between them.

The first is an ordering mistake. `build` fills its triangle array in mesh order. The recursive `buildNode` then partitions its own working array with `nth_element`, and each leaf records the range of triangles it owns as offsets into *that* partitioned order. The queries, however, read the original array. Every leaf therefore scanned a block of triangles that had nothing to do with the region it described. Measured against an exhaustive scan over a sphere of 2,232 triangles, the nearest-point query disagreed on **2,764 of 3,000** queries — and always in the same direction, reporting a distance too large, which is exactly what happens when a search looks confidently in the wrong place.

The second is a single word's worth of confusion between depth and size. The recursion's stopping rule asked whether `m_nodes.size()` exceeded a maximum *depth* of sixty-four — the number of nodes built so far, compared against a limit meant for how deep the recursion had gone. Once any mesh produced sixty-four nodes, every subsequent node became a leaf holding whatever triangles were left. The tree stopped growing at about seventy-one nodes for a model of *any* size, and on a sphere of 8,568 triangles a single leaf held 4,284 of them.

> An acceleration structure that stops accelerating does not fail. It keeps answering, only slowly — the one failure mode a correctness test is guaranteed not to see.

The second defect was the first one's alibi. When a leaf spans half the model, scanning the wrong range still tends to contain the right answer, so the ordering bug's damage was mostly absorbed. Repairing the tree first therefore made correctness visibly *worse* — wrong answers rose from about a thousand to two thousand seven hundred — which is a disconcerting thing to watch and the clearest possible evidence that a second defect was hiding underneath. With the ordering repaired too, disagreement fell to two in three thousand, and both of those are float rounding on a query point lying on the surface.

None of this was confined to the Boolean. The nearest-point query has seven other callers — snapping, signed-distance fields, Hausdorff distance, Poisson-disk sampling, quad remeshing, mesh closest-point — every one of which had been handed the wrong nearest triangle on any mesh large enough to split a node, for as long as the structure has existed.

Which raises the question of how it survived a test suite at all. Every existing test for the hierarchy was written against a **quad**. A quad has two triangles; it never splits a node, never permutes anything, and never builds a second level. The tests were not weak so much as aimed below the altitude where the code does its work. The replacements use a tessellated sphere, check the nearest-point and ray queries against exhaustive scans rather than hand-picked expectations, and assert that leaves stay bounded as the mesh grows — the property whose absence made the structure ornamental.

One capability had to be added rather than repaired. A point-in-solid test counts how many times a ray crosses the surface, so it needs *every* triangle along a segment; the existing ray cast returns only the nearest hit and cannot serve it. The new query collects every triangle whose box a segment passes through, deliberately conservative — extra candidates cost a little time, while a single omission flips a parity and inverts the answer — with each candidate still resolved by the same exact predicate as before.

**What was proven.** With a working hierarchy behind both queries the Boolean's output is byte-identical at every size tested — the same 208, 542, 968, 1550, 3150 and 5370 triangles as before, which is the guarantee that this accelerates and nothing more — while classification runs 7.2 times faster, 563 milliseconds down to 78 on the largest case, and the whole operation drops from 829 milliseconds to 312. The nearest-point query now agrees with an exhaustive scan to float precision, the ray cast likewise, leaves stay at eight triangles as the mesh grows, and the segment query provably contains every crossing. The cut's brute-force pairing of triangles is now the dominant cost, at 235 of those 312 milliseconds, and is the next thing to take apart.

## 24. The cost that was somewhere else

Repairing the hierarchy left the Boolean's cut as the dominant cost, and the reason looked obvious. To find where two surfaces meet, the cut paired every triangle of one mesh against every triangle of the other and tested their bounding boxes. That is the textbook definition of a missing broad phase, and the newly trustworthy hierarchy could supply one: ask which triangles of B could possibly meet this triangle of A, and get an answer in logarithmic time instead of by walking the whole model.

It worked exactly as advertised. Against a pair of spheres of 1,288 triangles each, the brute-force loop tested 1,658,944 pairs; the hierarchy proposed 4,271. Three hundred and eighty-eight times fewer.

The cut got twenty per cent faster.

That gap — between a reduction of nearly four hundredfold in the work being done and a fifth off the clock — is the whole chapter. An optimisation can be perfectly correct, perfectly effective at the thing it targets, and still barely register, and when that happens it has stopped being an optimisation and become an experiment whose result is *the time was never there*.

> An optimisation that works perfectly and changes almost nothing is not a failure. It is a measurement.

Where the time actually was: welding. After retriangulating, the cut merges the vertices that its independently-cut triangles duplicated along the seam, and the routine that does it compared every vertex against every earlier one. Timed across sizes it was flawlessly quadratic — a constant three hundred and twenty nanoseconds per vertex-squared, which at forty thousand vertices is four hundred and eighty-six milliseconds for a *single* weld. The cut welds both operands, and the Boolean welds the result again. Three quadratic passes, sitting underneath geometry work that had just been made logarithmic.

Replacing it is ordinary: a spatial hash whose cells are the weld tolerance, so any two vertices close enough to merge fall in the same cell or one of its twenty-six neighbours, and all twenty-seven get examined. What is not ordinary — what makes it a legitimate substitution rather than a different algorithm wearing the same name — is that the original had a *semantic* the replacement had to honour. The old loop bound each vertex to the **first** earlier vertex equivalent to it, scanning upward from zero and stopping at the first match, so the survivor of any cluster was always its lowest-numbered member. A hash returns candidates in whatever order the buckets happen to hold them. The replacement therefore gathers every candidate the unchanged predicate accepts and takes the minimum index among them, which reproduces the old rule rather than merely resembling it. Checked against a brute-force reference on two hundred clustered meshes, it disagreed on none.

The results are worth stating together, because separately each is misleading. Welding forty thousand vertices fell from four hundred and eighty-six milliseconds to eleven. The cut, on two spheres of 4,888 triangles, fell from two hundred and twenty-nine milliseconds to under eight. The whole Boolean on that pair fell fivefold. And measured across this arc from where it started — a box against a sphere of 8,568 triangles — one Boolean went from eight hundred and twenty-nine milliseconds to under twenty-five, thirty-three times faster, with the output byte-identical at every size on both benchmarks throughout. That last clause is the one that matters: none of this was allowed to change an answer, and it did not.

There is an embarrassment in the middle of this worth recording. The benchmark that had been driving the work was a twelve-triangle box cut against a finely tessellated sphere, and a twelve-triangle box is precisely the shape for which a broad phase can do nothing — each of its triangles has a bounding box spanning the entire model, so every pair genuinely does overlap and nothing can be pruned. The measurement was not wrong; it was aimed at the one case where the thing being measured cannot help. Sphere against sphere is both what a broad phase is for and what an actual model looks like.

**What was proven.** The Boolean is thirty-three times faster than at the start of this arc and its output is unchanged, verified by identical triangle counts at every size on both benchmarks. The weld agrees with brute-force first-match semantics on two hundred clustered meshes. The box query provably contains every genuinely overlapping triangle, as the segment query does every crossing. And the weld now carries a test that asserts its *shape* rather than its answers — quadrupling the vertex count must not roughly sixteen-fold the time — because a quadratic weld passes every correctness test ever written for it and fails only the model someone actually wanted to open. That is the same class of guard the hierarchy's leaves needed a chapter ago, and for the same reason: some defects are invisible to any test that only asks whether the answer is right.

## 25. Nothing turned red

Repairing the bounding volume hierarchy raised a question that had nothing to do with geometry. The structure had been returning the wrong nearest triangle on any mesh large enough to split a node, for as long as it had existed, and seven features are built on that query. Every one of them had tests. Every one of those tests passed while the structure underneath was wrong — and passed again, unchanged, once it was fixed.

A suite that cannot tell the difference between those two states is not testing the thing it appears to be testing. So the question became: which test *should* have caught that, and since none did, what else is riding on faith?

> When a fix lands and nothing turns red, the tests have told you where to look next.

Checked against independent references — exhaustive scans, or properties the answer must satisfy — six of the seven came back correct the moment the hierarchy was sound: closest-point, signed distance fields (including the sign, which nothing had ever checked), Hausdorff distance, snapping. The seventh did not. Poisson-disk sampling, asked for points a quarter of a unit apart on a sphere with eighteen square units of surface, returned **one point**. It returned one point for almost every configuration it was given.

The cause was upstream of all of them, in nine lines that no geometry test could have been expected to reach. The kernel's random source shifted its sixty-four output bits down to fifty-three — correct, that is exactly a double's mantissa — and then divided by two to the sixty-fourth instead of two to the fifty-third. Every "random" number the geometry kernel had ever drawn was a value between zero and **0.000488**. Off by a factor of two thousand and forty-eight; constant for any purpose that matters.

Four features draw from it. The symptom in the disk sampler was almost comic once traced: it throws darts around an accepted point, at a random angle and a random distance between one and two radii, and every one of the thirty attempts came back asking for the same distance — 0.3001, thirty times — in the same direction. The first seed could never find a neighbour, the active list emptied immediately, and the algorithm terminated having placed exactly the point it started with. With the generator repaired it tracks theory across the board: forty-four points where the packing predicts forty-two, eleven hundred where it predicts a thousand.

Two existing tests were, it turned out, asserting the bug. One expected the Hausdorff distance from a translated unit box back to the original to be 2.0, with a comment explaining a "nearest-face gap"; the configuration is symmetric and both directions are 3.0, and the comment had never been true — the test passed only because sampling never reached the far face. The other asserted that scattered instances spanned less than one and a half units, which held because every instance was landing on top of the first. Both were re-derived rather than relaxed. A test that encodes a defect is worse than no test, because it will actively resist the repair.

That left the three consumers that had simply had their randomness handed back and never been examined at all. Point sampling proved sound — samples fall in proportion to triangle area, which is the property that matters and the one a uniform-per-triangle sampler would fail. The instancer proved sound. Ambient occlusion did not, and for a reason with nothing to do with random numbers.

Every occlusion ray starts *on* the surface, so the first thing it meets is the triangle it started from, at distance zero. The hierarchy reports only the nearest hit — that is what a ray cast is for — so the self-intersection was the only thing it ever returned, and the guard that discards near-zero hits then read it as *nothing was hit*. Every ray came back unoccluded. The bake was a uniform 1.0 for every scene ever given to it: on a plane with a sphere resting against it, the vertices directly beneath the sphere scored precisely what the far rim did.

The fix is the oldest trick in ray tracing — lift the origin a hair off the surface along the normal before tracing, so the self-hit never happens. The same ray goes from a distance of negative zero to 0.05, and the shadowed patch beneath the sphere drops to 0.28 against an open rim at 0.985. What makes it convincing rather than merely different is that the brute-force path, which examines every triangle and keeps the nearest hit *beyond* the epsilon, had never had this problem: it skipped the self-hit naturally. Two implementations that share nothing but their sampling directions now agree to within nothing at all, where before they could not possibly have. That disagreement is now a test.

Three of my own measurements in this chapter were wrong before they were right, and each nearly produced a false bug report. Sphere primitives carry no normals, and the occlusion bake correctly returns empty without them. A plane carries texture coordinates and a box does not, so the routine that merges two meshes refuses the pair — and because I discarded its return value, the "occluder" in my test scene was never in the scene at all; the geometry I was measuring was a bare plane, and its uniform openness was the correct answer. A box's corners are too exposed to serve as a concave test in the first place. All three were caught by counting the vertices and triangles of the thing under test instead of assuming it had been built.

**What was proven.** The random source is now asserted as a *distribution* — mean, range, and the occupancy of every decile — because a generator whose range has collapsed still returns numbers, still runs, and still reports no error. Poisson-disk sampling matches packing theory across three surfaces and five spacings; point sampling is area-weighted to within four per cent on a deliberately lopsided box; ambient occlusion is uniformly open on a convex body, separates a shadowed patch from open surface by seven tenths, and agrees exactly between its accelerated and exhaustive paths; the instancer covers its target and reproduces. And six features that sit on the hierarchy are checked against references derived independently of them, on meshes large enough that the hierarchy has to work — which is the property their previous tests lacked, and the reason none of this was found sooner.

## 26. What the cube could not show

The previous chapter's method suggests its own next question. If a test that cannot fail when the layer beneath it is broken protects nothing, then the thing worth auditing next is whatever the most claims rest on while being itself least examined.

For this kernel that is not a subtle choice. Every assertion of watertightness in the entire Boolean and seam campaign — every leak counted, every residual closed, every "the cut is individually watertight" — reduces to a single number returned by a single function: the count of boundary loops in a mesh. If that number is wrong, none of it means what it says.

Its tests were four calls to the static arithmetic helpers with vertex, edge and face counts written down by hand — arithmetic, with no mesh anywhere near it — and two that validated an actual mesh, both of them a closed cube.

A closed cube is precisely the shape on which the defects cancel.

The reassuring half first, because it is the half that matters most: closed surfaces were always correct. A cube, a sphere and a torus each report the right boundary count, the right Euler characteristic and the right genus — a torus being the case that distinguishes genus from Euler characteristic at all. Everything the seam work claimed rests on closed meshes, and it stands.

Open surfaces were another matter, in three independent ways.

The edge count was taken as half the number of half-edges. That is exact when every edge carries two of them, which is the definition of closed; a boundary edge carries one. So the count came out short by half the boundary, and the Euler characteristic correspondingly too high — a flat four-by-four plane, which is a disk and has characteristic one, reported nine. The genus calculation subtracted an unsigned boundary count from a small integer, which promoted the whole expression to unsigned: one boundary loop against a characteristic of three evaluated as four billion and change, halved, and every open mesh in the kernel was reported to have a genus of 2,147,483,647.

The third is the interesting one. Three triangles meeting along a single edge is the textbook non-manifold configuration, and the validator reported it valid, with no violations at all. The check was looking for an edge whose half-edge valence exceeded two — a reasonable thing to look for, except that a half-edge structure *cannot represent* three faces on an edge. The third face never receives a twin, is therefore indistinguishable from a boundary, and no valence ever rises above two. The defect had been asked about in a language that cannot express it.

> The structure meant to expose the defect was the thing destroying the evidence.

Counting how many faces use each edge, straight off the face list before any structure exists, needs nothing and is exact.

Then repairing the edge count turned a passing test red, and that is the part of this chapter worth keeping. Solidifying an open surface — thickening it into a shell — is supposed to produce something closed. It builds side walls along the original boundary, and it had been storing those boundary edges as sorted pairs of endpoints, which quietly discards the direction the owning face traversed them. About half the walls therefore came out wound backwards. The result was closed as a *set of triangles* while not being consistently *oriented*: six directed edges on a solidified unit plane were each traversed twice in the same direction, a configuration no half-edge structure can pair, and the shell reported two boundary loops it did not have.

And the old halve-the-half-edges arithmetic had been cancelling exactly that error out, returning a characteristic of two — the right answer — by coincidence. Two defects, each concealing the other, precisely as the hierarchy's two had done three chapters earlier. The pattern has now happened often enough to state plainly: a repair that turns another test red is more often a second defect losing its cover than a regression.

The walls now keep the direction their face gave them and are wound to oppose the surface they join. The shell comes out with every one of its thirty-six directed edges traversed exactly once, characteristic two, genus zero, no boundary.

**What was proven.** Euler characteristics are checked against topology rather than against previous output, for closed surfaces and open ones alike — two for a sphere, zero for a torus, one for a disk and for a punctured sphere. Genus is exercised across a grid of characteristic and boundary-count combinations, so the arithmetic cannot wrap again for any plausible input. Non-manifold edges are detected and reported. And a closed surface must now traverse every directed edge exactly once — an orientation property that no measurement of area, volume, boundary count or Euler characteristic could see, and whose absence had been sitting inside a passing test.

## 27. The chord that used to be an arc

The curved Boolean — the gap chapter nine recorded honestly and left standing — finally has a safety net under it. Before touching anything, the current behaviour was pinned: a Boolean between curved solids is never leaky, it is a valid closed solid or a clean empty one; box against sphere, box against cylinder and sphere against sphere presently bail to empty, and the test says so in a message that tells a future reader to replace the assertion when that changes; the surface pairs genuinely outside the analytic scope stay unsupported; and a high-facet curved pair returns rather than grinding. None of that is a fix. It is the thing that makes the fixes safe to attempt.

The plan then reads: teach the assembler to carry a curved seam, teach the imprint to cut a curved face, wire the driver to pass curved seams through, sew across them. The first of those was expected to be preparatory — scaffolding for a defect that could not fire until the imprint started producing arcs.

It was already firing, on every Boolean that works.

The reassembler is handed vertex rings and a surface per face, and nothing else. It has no way to know that two consecutive vertices were joined by an arc, so it does the only thing it can and builds a straight chord between them — a `Line`, unconditionally, for every edge of every face. So the analytic curvature of a curved operand does not survive the sew, and the way to see this without any curved-Boolean machinery at all is to run a Boolean that is geometrically a no-op. A unit box sits strictly inside a twelve-segment cylinder: its corner radius is about 0.707 against the prism's inradius of 0.966, and its half-height is well within the caps. The union of the two *is* the cylinder. It comes back as the cylinder — fourteen faces, thirty-six edges, closed, both validators clean, the volume exact. And all twenty-four of the cylinder's rim arcs come back as chords.

That is why it had gone unseen for so long. Every invariant the Boolean campaign had built was satisfied. Topology, watertightness, Euler characteristic, mass properties: all correct, because a chord and the arc it subtends share their endpoints, and every one of those measurements is computed from the vertices. The lost geometry is only observable if you ask the body to *retessellate itself*, because the analytic curve is precisely what refinement refines against, and a chord refines to itself.

> Every invariant we had was blind to it, because a chord and its arc agree at exactly the points we were measuring.

The repair does not need a new interface, which was the pleasant surprise. The Boolean already welds every kept vertex into one shared list, and the reassembler numbers its vertices one-for-one from that list — so an index computed before the sew is still the same vertex after it. Walking each kept face's coedges before assembly yields every edge that carries a circle, recorded against its welded endpoint pair without direction, so the key survives both the winding of the loop and the outward flip a difference operation applies to the tool's cavity walls. After the sew, each edge that matches a recorded pair has its arc restored.

The restoration deliberately goes through the existing operator rather than writing the curve in directly, and that choice is what makes the pass unable to do harm. That operator re-derives the parameter range from the edge's own endpoints and refuses outright when they do not lie on the circle within tolerance. An edge whose seam vertex the weld pulled off the arc, or which a later split left spanning something else, therefore keeps its chord — degrading to exactly the behaviour of the previous paragraph rather than to geometry that contradicts its own topology. There is no configuration in which the pass can attach a curve the validators would then reject.

**What was proven.** The refinement series is the assertion that matters, because it is the one a relabelled `Line` cannot pass. The cylinder alone tessellates to 6.000 unrefined — the twelve-gon prism exactly — then 6.0706, 6.0838, 6.0884, climbing toward the true π·r²·h of 6.2832. The union with the interior box now reproduces that series at every level of refinement, having previously been flat at 6.000 forever. In the disjoint case the two kinds of edge are seen behaving differently inside a single body: the box contributes exactly its own 1.0 no matter how finely it is subdivided, while the cylinder's arcs add 0.088 across the same calls. A purely planar Boolean gains no curvature at all, so the pass cannot invent geometry where none was. Four of the five new tests fail with the restoration disabled and the fifth is the guard that must hold either way, which is the only evidence that a passing test is load-bearing. The suite stands at 2,425 with no regression — and the curved Boolean still bails to empty, exactly as chapter nine said and as the new safety net insists, because assembly was never what was stopping it. It was simply the layer that would have thrown the answer away.

## 28. The arc that went the long way round

Carrying a curved seam is useless while nothing produces one, so the next question was where the seam fails to get cut. The imprint accepts an intersection circle only onto a *planar* face, and the consequence of that is best stated as a count. Over every pair of faces between a two-unit box and a cylinder of radius one half driven through it, thirty-two circle seams land on the box's flat faces and every one is accepted; thirty-two land on the cylinder's curved faces and every one is refused. One operand is cut along its share of the seam and the other is not touched at all, so the reassembler is asked to close a boundary that exists on only one side of itself. All three operations return empty, and had done since the beginning.

A cylinder contains exactly one family of circles — the latitude circles, centred on the axis, lying in a plane square to it, at the cylinder's own radius. That is precisely what a plane perpendicular to the axis cuts, which is to say it is exactly the seam a cylinder driven through a box needs, and no other circle need ever be considered. Better still, such a circle crosses a side patch through its two upright edges, which is structurally the same two-point bite the planar path had been performing all along. The cut itself needed nothing: it turned out to be purely a matter of topology and parameter ranges, with no dependence on the face being flat.

What could not be carried over was the small test that decides *which* of the two arcs between those crossings lies inside the face. The direct rule projects the boundary onto a plane and counts ray crossings, and a curved patch has no plane to project onto. So containment moves to where a trimmed surface's boundary is properly defined in the first place — the surface's own parameter domain, where the patch is an ordinary rectangle and the ordinary rule applies unchanged. The one care needed is that a cylinder's angular parameter is periodic: walking the boundary ring, each successive angle is shifted by whole turns until it lies within half a turn of its predecessor, which reconstructs a polygon that does not wrap even for the one patch per cylinder that straddles the seam where the angle changes sign.

Substituting the naive flat test back in is the measurement that justifies all of this, and its result is stronger than expected. Every cylindrical face then chooses the complement: an arc of 5.890 radians where 0.393 was wanted, three hundred and thirty-seven degrees the wrong way round the cylinder instead of twenty-two the right way. And the resulting body is *clean*. Integrity passes. Geometric consistency passes, because the curve does still meet its endpoints. The Euler characteristic is unchanged, the shell is still closed, and four of the five tests written for this increment go green on it.

> A face cut the long way round the cylinder satisfies every invariant we own. Only its length gives it away.

**What was proven.** All sixteen of a cylinder's side patches accept the latitude seam, which necessarily includes the one straddling the parameter seam, so the unwrapping is exercised rather than assumed. The cut edge's span is asserted against the patch's own angular width — the single measurement that separates the correct arc from its complement, and the one that fails by a factor of fifteen without the parameter-domain test. The imprint is neutral in the Euler characteristic on a curved face exactly as on a flat one, two vertices and three edges and one face, with both validators clean and the shell still closed. Circles that are not curves *of* the cylinder — wrong radius, centre off the axis, plane not square to it, or a latitude beyond the patch's reach — are all refused, so the imprint cannot attach a trim curve that leaves the surface it claims to lie on. And the curved Boolean *still* returns empty, which is correct: the driver that walks face pairs continues to discard every seam that is not a straight line, and wiring it is the next passage.

## 29. Two defects that were waiting to be reachable

The wiring itself is four lines. The driver that walks every pair of faces asks for their surfaces' intersection and then offered the imprint only the straight branch of the answer, discarding the circle and the two-line cases unexamined. Offering all of them instead — and letting the imprint be the authority on whether a given curve actually applies to a given face, since it already refuses everything that does not — is the whole of the change.

Turning it on produced a box with one million five hundred and ninety-nine thousand nine hundred and ninety-two vertices.

It still had six faces. That is the entire explanation. The imprint has two ways to cut a face with a circle: bite an arc off it, which splits the face in two, or — when the circle falls wholly inside the boundary — punch a hole, which adds an inner loop and leaves the face count exactly where it was. The driver iterates to a fixpoint, re-offering every tool surface on every pass until nothing changes, and it decides that something changed by being told so. The arc bite consumes its own precondition: having split the face, the circle no longer crosses that face's interior, so the second offer is refused and the loop settles. The hole does not. It leaves the circle precisely as interior as it found it, so the same ring was appended on every pass, forever, eight vertices at a time. The guard that exists specifically to catch a runaway imprint was watching the face count, and the face count never moved.

So the hole path now refuses a hole it has already made, which is what idempotence means for an operation like this, and the runaway guard has been taught to bound the entity count as well as the face count — because the argument that any one operation terminates is a weaker thing to rely on than a ceiling that holds whether it does or not.

Underneath that sat a second defect, and it is the more interesting of the two. With the loop settled, the cylinder came out structurally perfect and geometrically invalid: sixty-four vertices, ninety-six edges, thirty-four faces, integrity clean, and six edges whose curve did not pass through its own endpoint. All six missed by the same amount, 1.22 × 10⁻⁴, and all six missed only in the axial direction — a vertex sitting at −0.9999 on a circle lying at exactly −1.

A latitude circle crosses a cylinder's upright edge at one height, and finding that height had been left to the same routine the planar case uses: solve where the distance from the circle's centre equals its radius. On a plane, where circle and edge are coplanar, that root is transversal and the arithmetic is well behaved. On a cylinder it is nothing of the kind. The upright edge sits at exactly the cylinder's radius along its *entire length*, so the distance function does not cross the radius — it touches it, once, tangentially. The quadratic has a double root, and a double root does not resolve to the precision of the arithmetic but to its square root: a hundred-thousandth becomes a ten-thousandth, which is a thousand times the tolerance the kernel calls coincident, and quite enough to fail the validator on any arc built through the vertex.

> The formula was not wrong. It was being asked a question whose answer it could not hold.

A latitude circle is the level set of the cylinder's axial parameter. Asked that way — at what fraction along this edge does the axial coordinate reach the circle's — the problem is linear, and linear problems have no square roots in their error. The worst endpoint error over the whole imprinted body falls below a millionth, which is where float noise actually lives, and the test asserts that bound rather than the validator's own looser one, so a regression to the quadratic cannot hide inside a passing check.

One consequence had to be judged rather than fixed. A Boolean that was a geometric no-op — a box strictly inside a cylinder, unioned — used to return the cylinder's fourteen faces exactly, and now returns twenty-two, because the interior box's planes cut real latitude seams on the eight side faces whose bounding boxes reach it. The solid is identical: the analytic volume is π·r²·h to the last digit on both, and the tessellated volume agrees at every level of refinement. What changed is the partition, and a seam that does not bound a change of surface is removed by face unification, which is a separate pass and not the Boolean's job. The earlier test had been asserting the face count because at the time it happened to be fourteen; it now asserts the volume identity, which is the thing that was actually meant.

**What was proven.** Both operands of a cylinder driven through a box are cut — the assertion that fails outright without the wiring, since the cylinder's sixteen curved faces previously received none of their sixteen seams — and both remain valid bodies afterwards, the cylinder still closed. Endpoint error across an imprinted body is held below a millionth. A repeated interior-circle imprint adds geometry exactly once and is refused five times running with the vertex, edge and loop counts unmoved. Imprinting terminates and stays bounded across four curved pairs including one with some three hundred and sixty curved faces. Each of the three fixes was reverted in turn to confirm the tests that guard it actually fail without it: the conditioning fix takes two of them down, the wiring one, and the idempotence guard four — the sole survivor being the boundedness test, held up by the entity ceiling, which is exactly the division of labour it was added for. And the Boolean between curved solids still returns empty, because what remains is the sew.

## 30. The same circle, counted twice

With both operands finally cut, the seam could be looked at from both sides, and the two sides did not match. Not in position — in *number*. Where a box's face had been holed by a cylinder passing through it, the box's hole ring carried eight vertices and the cylinder's latitude ring carried sixteen. The eight sat on eight of the sixteen to within two ten-millionths, which is to say they were the same circle beyond any doubt. But each of the box's hole edges spanned two of the cylinder's, so not one edge on either side could find a partner, and a sew that works by pairing edges had nothing to pair.

The eight was a constant in the source, chosen when a hole ring was something a face acquired on its own account. That is a perfectly good answer for a lone body, where nothing else has an opinion. It is the wrong kind of answer for a seam, because a seam is not owned by either operand: the other side has already decided how finely that circle is divided, and the only correct resolution is the one already in use over there. So the imprint now takes the other operand's vertices on the seam circle and builds the ring on exactly those points — not near them, on them — and the two rings become the same ring by construction rather than by luck.

That is where the increment was expected to end, and it is where it started instead, because the fix did nothing at all. The rings still came out eight against sixteen.

The cause is an ordering that is obvious once seen and invisible until then. The cylinder's latitude ring *does not exist* at the start; it is created by imprinting the box's planes onto the cylinder. The mutual imprint cuts the box first, and at that moment there is nothing on the cylinder at that height to match — no vertices, no ring, nothing to copy. Asking the other operand what resolution to use is only meaningful once the other operand has been cut, and the first thing done is cut something else.

> The information needed to make the seam correctly is produced by the very step that comes afterwards.

Which reframes it: committing to a resolution at that moment is not merely arbitrary, it is *premature*, and the honest response to a question that cannot yet be answered is to decline to answer it. The hole now defers whenever it is being coordinated across two operands and its partner ring is not yet present, and the mutual imprint runs a second round to make the cuts that deferred. Everything already imprinted refuses idempotently — which is the guard from the previous chapter, doing work it was not written for — so the extra round converges instead of accumulating. A caller imprinting a lone body passes no ring, is asking a question that has no other side, and still gets the uniform ring it always did.

**What was proven.** Both rings on both seams of a cylinder driven through a box now carry sixteen vertices, and every vertex on each side has a counterpart on the other at a distance that measures exactly zero — not within a tolerance, zero, because one side's ring vertices are the other side's positions rather than an independent construction that happens to agree. Reverting only the hand-off of the partner ring returns the count to eight against sixteen and fails that assertion, so it is the mechanism and not a coincidence of this configuration. Both operands remain valid with the shared ring attached, the cylinder still closed, and the arcs built through the supplied points still reproduce their own endpoints — which the ring's parameters are now derived per-point to ensure, the closing edge wrapping a full turn. Supplied points that are not on the circle are discarded rather than placed as vertices off the curve, so the list is a hint and never a way to corrupt the geometry. A coordinated hole with no partner ring modifies nothing at all. And the Boolean between curved solids still returns empty, for a reason that is now the only reason left: a face's inner loop does not survive the sew. The routine that reads a face for reassembly walks its outer loop alone, the structure it fills has room for one ring, and the reassembler builds only outer loops — so the hole this whole chapter was spent making correct is, for the moment, discarded on the way out.

## 31. A hole the assembler had no word for

Everything about a hole was already understood except how to build one. The validators check inner loops, the tessellator subtracts them, the serialiser writes them, and the imprint had been creating them for chapters. What could not be done was put one back together. The routine that reads a face for reassembly returns its outer boundary and nothing else; the structure it fills has room for a single ring; the assembler builds only outer loops; and the Boolean's face collection never mentioned inner loops at all. A pierced face went in holed and came out solid, which loses the opening and — worse — leaves the other operand's ring edges with nothing to pair against.

Widening that path is not interesting in itself, and the one decision worth recording is that a hole ring goes through *exactly* the same construction as an outer one. Edge deduplication, coedge partnering, the non-manifold rejections: all of it, unchanged, because a hole differs from an outer boundary only in which way it winds and in being listed as inner. Writing a second loop-builder for the inner case would have been the obvious shape and would have meant two implementations of edge sharing, diverging on the first bug fixed in one of them. The reversal that flips a face outward now flips its holes too, so a hole keeps winding against its boundary rather than with it.

And then the Boolean still returned empty, which is where this chapter earns its place.

The instrumentation is worth quoting in full because it says the whole thing in two lines. For the union: twenty-two faces, *zero* with holes. For the intersection: eighteen faces, two of them holed, each hole a sixteen-vertex ring — the shared ring from the previous chapter, arriving intact — and then the assembler rejecting the result as non-manifold. The holed face was dropped from one operation and wrongly kept by the other.

Both come from the same place. A face is classified against the other solid by testing a single point, and that point is the average of its outer boundary's vertices. Take a square face with a circular hole punched through its middle: the average of the square's four corners is the centre of the square, which is the centre of the hole. The one point chosen to stand for the face is the one point that is not on it. It lies inside the cylinder, so the face is called *inside* — dropped from the union, where its material is entirely outside the cylinder and belongs on the boundary, and kept for the intersection, where none of it belongs at all.

> The sample point representing the face was chosen from the one region the face does not occupy.

The tempting repair is to weight the centroid by area, subtracting the hole. It does not work, and seeing why is the useful part: for a hole concentric with its face the area centroid is *still* the centre, because both regions share it. No formula that averages the face's extent can escape the hole, because the hole is where the middle is. What is needed is not a better centroid but a point known to lie on the material — which means asking the face for a sample of itself rather than computing a summary of its outline.

**What was proven.** A holed face reports its hole separately from its outer boundary, and an unholed one reports none. A face with a hole survives disassembly and reassembly — the exact round trip the sew performs — with the ring's vertex count intact, both validators clean, and the loop flagged as inner rather than outer, so every consumer that subtracts holes still treats the opening as an opening. Hole-ring edges are shared, not duplicated: a plate holed by a square plus a lid over that hole assembles into eight edges rather than twelve, with the four ring edges partnered across both faces and the four outer ones left one-sided, which is the exact property the curved sew needs. Degenerate rings — two vertices, or an index past the point list — are rejected outright rather than half-built. Disabling the construction fails four of the five, so they are load-bearing. Two thousand four hundred and forty-five tests pass with no regression, which for a change inside the routine every primitive and every Boolean is built by is the claim that mattered most.

## 32. Asking the face instead of its outline

The previous chapter ended on a face being judged by a point that its own hole had swallowed. The repair is to stop summarising and start sampling: return a point known to lie on the material — inside the outer boundary, inside none of the holes — and classify with that.

The implementation is candidate-and-test rather than closed-form, and the ordering of the candidates is the only part worth defending. Tried first is the midpoint between an outer vertex and a hole vertex, because that is the candidate which survives the case that defeats every averaging scheme. For a square holed concentrically, the corner sits at one radius and the nearest ring vertex at another, and the point halfway between them is comfortably in the material while the centre of the square — the answer any average gives — is in the opening. Failing that, points drawn in from the outer boundary at a quarter, a half and three quarters of the way toward the centroid. The first candidate that passes wins, the order is fixed, and so the answer is reproducible, which matters because a classification that varied between runs would break determinism on the headless path. When a face has no holes the function returns the centroid unchanged, so nothing that existed before moves by a bit.

That is a small piece of code, and it did what it was meant to. On a box pierced by a cylinder the centroid of each holed face lies at the axis and classifies as inside the cylinder; the sample point lands at roughly four tenths and seven tenths off-axis and classifies as outside. All six of the box's faces now classify outside the cylinder, which is the truth: no part of any of them is inside it. The coincident-face probe, which offsets from the same point and had the same flaw, was moved over too.

**What was proven.** With no holes the sample point equals the centroid to the bit, across a box, a cylinder, a sphere and a cone, so every classification that came before is untouched — and two thousand four hundred and fifty tests confirm it, which for a change inside the function every Boolean classifies through is the assertion that carried the risk. On a pierced face the centroid falls in the opening and the sample point on the material, and the two land on opposite sides of the piercing solid. A concentric hole is pinned explicitly: the outline average sits at distance zero from the hole's centre, which is why no area weighting could rescue it, and the sample point is beyond the hole's radius while still inside the face. The result is deterministic across repeated construction. Reverting the one line that chooses between the two points fails the classification assertion, so it is the mechanism.

And the Boolean between curved solids still returns empty. It is worth being precise about what improved and what did not, because the numbers moved in the right direction without arriving. The union's face collection went from twenty-two faces with no holes to twenty-four with two — the pierced faces are now correctly kept and they bring their openings with them. But the assembly still refuses: five directed edges are traversed twice, and fifty-eight edges are used only once where a closed solid would use every one of them twice. Counting what a correct answer would contain — the box's six faces, the cylinder's two protruding stubs at sixteen side faces each, and their two end caps — gives forty faces, and twenty-four is not that. The cylinder's side faces are being cut into fewer pieces than the two cuts through them should produce, so faces that straddle the box's boundary are still being kept or dropped whole.

> Three chapters of this arc ended by naming the next obstacle. This one ends by naming a count: forty faces wanted, twenty-four offered.

## 33. The cut that had already happened

Forty faces wanted, twenty-four offered, and the shortfall was in the cylinder. A cylinder driven through a box is crossed by two planes, so each of its side faces has to end up in three pieces. Each was ending up in two. Measured on the usual pair, the sixteen side faces became thirty-two in four groups of eight — one group spanning the lower stub, one the upper, and two spanning from an end all the way past the far plane. Sixteen faces still straddled a plane, and a face that straddles is kept or dropped whole, which is exactly the thing the imprint exists to prevent.

The cause is a consequence of the imprint's own success. The faces are cut one at a time, and the moment one of them is cut at a latitude, its neighbours' upright edges have been split at that same height — they are shared edges, and splitting one splits it for both faces. So for every face after the first, the level no longer falls somewhere inside an upright edge; it falls exactly *on* an existing vertex. The routine that finds where a latitude circle meets an edge required a fraction strictly between the ends, reported no crossings at all, and the face fell through to the fully-interior case, which refused it. The face was not uncut because the cut was hard. It was uncut because the cut was already there and the solver could not see it.

> Requiring a crossing to be strictly inside an edge is a reasonable rule for the first face and a false one for every face after it.

The planar path had always accepted an endpoint fraction and snapped it onto the existing vertex — that is how a Line imprint reuses a corner rather than manufacturing a duplicate beside it — so the fix is to make the cylindrical path behave the same way. A crossing within an axial epsilon of either end is accepted, the reported fraction is clamped into the unit interval, and the caller's existing snap-to-vertex step turns it into a reuse of that vertex. Nothing new is built; something that existed is found.

**What was proven.** The sixteen side faces now become forty-eight in three bands, at the two latitudes the box's faces actually cut, with both extremes locked so neither the endpoint case nor the strictly-interior one can regress. The strict-interior guard survives for crossings genuinely inside an edge, which is what keeps a crossing from being counted twice at a shared corner. Two thousand four hundred and sixty-two tests pass.

## 34. The imprint that took something away

Everything the sew needed was now in place. Arcs survive assembly; a circle can be imprinted onto a cylindrical face; the driver offers every seam branch; both operands discretise a shared seam identically; inner loops make it through reassembly; a pierced face is judged by its material; and the side faces are finally cut at both levels. The Boolean between a box and a cylinder still returned nothing.

What remained was not in the sew, and had been sitting in plain sight for four chapters. Where a seam circle fell entirely inside a face, the imprint gave that face an inner loop and stopped. The ring's arcs had one coedge each — nobody on the other side of them — so the box came out of the imprint as an open shell with two circular holes in it. The word for that in the source was *hole*, and it was accurate: material had been removed.

Two things follow from an open operand, and both are fatal. The first is that classification stops meaning anything. A point is classified against a solid by counting how many times a ray from it crosses the tessellated boundary, and the parity of that count is only informative if the boundary is closed. A ray that enters through an opening and leaves through material crosses once and reports the point as inside. Measured: of the sixteen faces in the cylinder's lower stub — a clear half-unit *below* a box that ends at z = −1 — five came back Inside. The pristine box classifies those same five points Outside, which is how the mechanism was pinned rather than guessed. The ray had been walking in through the hole the imprint left and out through a wall.

The second is more fundamental, and it survives fixing the first. The disk inside the ring is the material that *caps the intersection*. A box intersected with a cylinder through it is a plug: sixteen side faces from the cylinder's middle band and a flat disk at each end, and those two disks are precisely the parts of the box's faces that lie inside the cylinder. Thrown away at the imprint, they do not exist anywhere, and no amount of careful sewing can close a solid out of pieces that are missing.

> An imprint segments a boundary. It must never remove material — the operand it hands on is the thing the classifier will be asked about, and the thing whose pieces the result is built from.

So a fully-interior circle now segments the face instead of opening it: the circle becomes an inner loop of the face it cut, the disk it encloses becomes a face in its own right, and the two share the ring's arcs edge for edge. The shell stays closed, the volume is untouched, and the disk is present to be classified on its own account — inside the cylinder, so the union drops it and the intersection keeps it, which is what each of them wants.

One detail in that construction is worth stating because assuming it is wrong. The ring is built in increasing order of angle, and which way that turns about the face being cut depends only on where the seam circle's axis happens to point: a plane cutting a cylinder yields one circle per level, and its axis agrees with the normal of the face above and opposes the face below. Hard-coding a direction therefore gets one of any two such rings backwards — an inner loop wound *with* its outer boundary bounds a second outer region rather than an opening, and the disk beneath it comes out inside-out. The direction is read off the face instead: whichever way its own outer loop turns is outer-like, the ring bounding the opening turns the other way, and the disk's boundary turns with the face. Hard-coding it instead fails three of the six new tests, which is the measurement that the reasoning was not theoretical.

**What was proven.** Both operands come out of a mutual imprint closed, with no boundary edge, both validators clean and their volumes unchanged — and a point outside the imprinted box classifies exactly as it does against the pristine one, across every face of the cylinder, which is the invariant whose violation caused all of this. All three operations then sew, at the face counts the geometry dictates rather than approximately: forty for the union, eighteen for the plug, twenty-two for the box with a tunnel through it. The volumes satisfy inclusion–exclusion at every level of refinement and match the exact sixteen-gon arithmetic unrefined — 1.530734 for the intersection, 6.469266 for the difference, 9.530734 for the union — so no face was dropped, duplicated or misplaced. That the analytic circles survived the sew cannot be seen in any invariant at a fixed tessellation, because a chord shares its arc's endpoints and refines to itself; the witness is that the plug, being exactly half the cylinder's length, tracks exactly half of the cylinder's own refinement series level for level, and it does. A union with a cylinder that swallows its box whole is still that cylinder to within a hundred-thousandth at every level, so segmenting along a seam that bounds no change of material moves nothing. Removing the disk fails five of the six. Two thousand four hundred and sixty-eight tests pass, with no regression anywhere — which for a change inside the routine every imprint and therefore every analytic Boolean passes through is the claim that mattered.

The first curved Boolean in this kernel is real, and it is exact where it produces an answer. It is also narrow: the case proven is a cylinder whose footprint lies strictly inside the box, so each seam is a full circle interior to a face. Offset the cylinder until it pokes through a side wall and the seam becomes an arc bite instead, and that still returns empty — the baseline from chapter nine says so and continues to pass unchanged. A circle imprinted onto a *sphere's* face is likewise still refused, which is what box-and-sphere and sphere-and-sphere need.

## 35. The bedrock that a compiler flag dissolved

This chapter is not about geometry. It is about the ground everything else in this logbook stands on, and about the fact that the ground had quietly stopped being there.

Chapter twenty told the story of predicates that were measured rather than read, found wrong, and rebuilt on genuine expansion arithmetic. That rebuild was correct. What was never checked is whether the compiler was still running it. The build carried `-ffast-math`, which grants permission to reassociate floating-point arithmetic, and Shewchuk's error-free transformations exist *only* as expressions that must not be reassociated: `twoSum` recovers the rounding error of a sum with a formula that is algebraically zero, and a compiler entitled to simplify it does. Measured with everything else held identical, the error term is 8.67 × 10⁻¹⁹ at `-O2` and exactly zero once the flag is added.

With every error term zero the expansion arithmetic computes nothing that plain double arithmetic would not, and plain double is not enough. Against an exact reference in 128-bit integers, over coordinates whose intermediate products pass the 2⁵³ a double can hold, the shipped `orient3D` returned six wrong signs in the first five and a half thousand cases. Every one of them was on input that is *exactly* coplanar, where the predicate answered with a confident ±512 or ±1024 instead of zero. That is the worst answer available: Simulation of Simplicity, the whole degeneracy strategy of chapter two, is never consulted, because it only runs when the predicate reports a tie. Two thirds of the degenerate configurations in that sample were decided by a coin flip. Without the flag, and changing nothing else: zero wrong signs in four hundred thousand.

> The algorithm was right. The build was undoing it.

The reason no test noticed is worth more than the fix. The exactness battery bounds its coordinates so that its *own* reference stays computable in 64-bit integers — which puts the determinant under 2⁵¹, and 2⁵¹ is below the 2⁵³ a double holds exactly. In that range plain double arithmetic is also exact. The bound chosen to make the reference trustworthy is precisely the bound that made the subject's failure invisible. All eight of those batteries still pass with the flag re-added; this was verified deliberately, because a guard that cannot fail is worth knowing about.

Two smaller casualties of the same flag: `std::isfinite` reports NaN and infinity as finite under it, so two input guards — one on a camera target, one on a fluid solver's smoothing radius — had been dead code, and a NaN could walk into a view matrix unchallenged. The geometry module escaped only because it had long since stopped trusting the standard library here and inspects the exponent bits directly.

**What was proven.** The flag is gone, `-ffp-contract=off` is in (an FMA moves where the rounding lands, which is the one liberty an error-free transformation cannot survive), and `-march=native` was replaced by a fixed baseline — a host-specific instruction set makes the same source produce different floating-point results on different machines, which contradicts this kernel's promise that a result is reproducible. The battery now carries a 128-bit reference well past 2⁵³, and alongside it the actual failing quadruple, recorded, so the guard does not depend on a random draw finding it again. Re-adding the flag fails that test and nothing else, which is the measurement that says the new one is load-bearing and the old ones were not. Both dead guards now use the bit-inspecting helper their own files already had, and both fail if the guard is removed. Two thousand four hundred and seventy-four tests pass, and the whole thing costs four tenths of a second on a thirty-five second suite.

## 36. The seam that had to be shared twice

The cylinder of chapter thirty-four sits centred, its footprint strictly inside the box's walls, so each seam is a full circle in the middle of a face. Slide it sideways until it reaches a wall and every one of those Booleans returns empty again. The seam stops being a circle interior to a face and becomes an *arc bite* — a circle that enters and leaves through the face's boundary, cutting a lens off it — and four separate things were in the way.

The bite itself was deferred, for a reason that turns out to be structural. Both crossings land on the *same* boundary edge; on the usual pair the circle meets the wall's edge at two points and touches nothing else, at every offset. So the natural topology is a face with two sides — the arc, and the chord it cuts across — and a loop of two coedges is rejected by the integrity checker, which requires three. The temptation is to relax that rule. The better move is to split the chord at its midpoint: the bite then has three edges, the two crossings stop being adjacent in the loop, and the extra vertex sits exactly on the original straight boundary, so it costs a redundant vertex and not one micron of geometry.

Then the box's side wall was never cut, because the intersection of a plane with a cylinder returned *unsupported* whenever the plane ran parallel to the axis. That section is not a conic at all — the plane slices the cylinder along two straight lines — and the result type had carried a `TwoLines` case for it all along, waiting.

Then the remainder of a bitten face was classified as being inside the cylinder. An arc bite leaves a *concave* face, and the routine that picks a representative point assumed a face without holes has its outline's average somewhere on it. True for a convex face; false here. Worse, the polygon it tested was built from bare vertices, which replaces the arc with its chord and so re-encloses the very lens that had just been removed. The average of the remainder's six boundary vertices is a third of the way along, well inside the cylinder, while the face's material is almost entirely outside it.

And then the tessellator, which had been fanning every unholed face from its first vertex. Correct for a convex ring, and every ring *had* been convex — a straight imprint splits convex into convex, and an interior circle goes down the hole path. The bite breaks that quietly: the bitten face tessellated to an area of 4.65 where the whole face is 4.0, double-covering the 0.65 lens it had just cut away. Signed volume survived it, because the duplicate triangles cancel, so the validators, the Euler characteristic and the volume all looked clean. Only the unsigned area gave it away.

> Three times in this arc a face has been geometrically wrong while every topological invariant said it was fine. A degenerate face is topologically ordinary. When something changes how a face is bounded, measure its area.

With those four the operands were both cut correctly — and the Boolean was still empty, because they did not agree on how *finely*. Cutting a box face along the seam gives the box one arc from entry to exit; the cylinder's rim over the same stretch is a chain of facet arcs. Two vertices facing thirteen; one edge facing twelve; not one of them able to partner. This is chapter thirty's eight-against-sixteen exactly, one layer out, and it takes the same answer — build on the points the other operand already chose — with the same ordering trap attached, since the mutual imprint cuts one operand at a time and the round that made the cut had nothing yet to match. So a face already segmented along a circle now *reconciles* its discretization on a later round rather than merely refusing.

One more thing had to be undone before that worked, and it is the most instructive failure in the chapter. Teaching the imprint to cut a cylinder along a generatrix produced four zero-area faces, each joining a point on one generatrix straight across the rim to a point on the other. The obvious suspect was the new code. It was not. A cylinder's side face has its own rim *on* the latitude circle, and that circle meets each of the face's uprights at the upright's endpoint — which chapter thirty-three had taught the solver to accept, correctly, since that is how a neighbour's cut is recognised. Two endpoint crossings look exactly like a clean two-point bite, and the face gets cut from one end of its own rim to the other. What had been preventing that for chapters was luck: the two rim corners were *adjacent* in the loop, and the cutter refuses adjacent vertices. Dropping a vertex between them removes the accident. Measured both ways round — four flat faces with the generatrix imprint enabled, none with it disabled — the new code had only exposed a hazard the old code already carried, and the guard that belongs there had been written two chapters earlier and placed one branch too late.

**What was proven.** A cylinder offset until it pierces the wall now yields all three Booleans as closed, valid solids, at three different offsets. The two operands share the seam vertex for vertex — thirteen points on each side, at both levels. The difference and the intersection tile the box exactly. And inclusion–exclusion is exact on the analytic volume at every offset, which required correcting this book's own account of the matter: measured on the tessellation the identity misses by seven thousandths, and that was first written up as a fault in the union's account of the protruding cylinder. It is not. The tessellator under-refines the *interior* of a curved patch — a lone cylinder converges to 3.088 where π·r²·h is 3.1416, its flat caps exact and its side patches not — and the two sides of the identity carry different amounts of curved surface, so the error does not cancel. The union had been right all along, and the oracle was wrong.

> When an oracle and the thing it is judging share a known-approximate path, the oracle proves nothing.

That measurement, made properly, turned up something else on its way past: the analytic volume of a *difference* through a curved solid is wrong, while planar differences and every intersection are exact. A face is reversed by negating its surface's stored normal, and that field means the outward direction for a plane but the *axis* for a cylinder, a sphere or a cone. Negating it flips a plane correctly and merely re-parameterizes a cylinder, reversing nothing. The face definition handed to the assembler has no way to say "reversed" at all, so the Boolean cannot express what it means, and the curved integrator — which does honour that flag — is simply never told. It is recorded, bounded by a test that fails when it is fixed, and left for its own passage.

## 37. Six digits, and the four calls that kept them

The last chapter closed by naming two decisions rather than defects, and said each gets more expensive with everything built on top of it. This chapter spends one of them.

The B-rep stored coordinates as `float`. Cheap, GPU-shaped, and the same choice the mesh side makes for good reasons. It also caps the representation at about six decimal digits, which is a *relative* cap: a model a hundred metres across can be trusted to a tenth of a millimetre and no further, and every industrial kernel this one means to stand beside — Parasolid, ACIS, OCCT — uses double throughout. A kernel cannot promise exact predicates on inputs it has already rounded. So the decision was not close; the only real question was how to make the change without a month of edits.

The lever turned out to be one line. The geometry namespace pulled `Vec3` in from the renderer with a using-declaration, so replacing that declaration with an alias to a new double-width vector migrated every declaration in the namespace at once. The new type widens implicitly from the float one and narrows only through an explicit `toFloat()`, which means the compiler finds each boundary where geometry hands a point back to the renderer and refuses to cross it silently. What remained was the arithmetic the alias could not see: parameter ranges, radii, the serialized format.

That format was a version bump, and version bumps in this kernel must not break existing files. The rule had been checked all along by taking a *current* blob and relabelling it with an older version byte — which had only ever worked because every version happened to share one payload layout. Across a change of width the trick tests nothing at all: it produces a new payload wearing an old header, and decoding it as the old version reads doubles as floats. So a real blob, written by the old writer before the migration, is committed as bytes. It cannot drift with the code, which is the whole point of it.

> A compatibility fixture that the current code can regenerate is not a fixture. It is a mirror.

The primitives came next and taught the sharper lesson. Widening one of them — the cylinder alone, as a cautious first step — improved the cylinder's construction error from 5.9 × 10⁻⁸ to 4.4 × 10⁻⁸ and made the *box* worse, from about 6 × 10⁻⁸ to 1.2 × 10⁻⁷. Two solids that must agree along a seam had been built at different widths, and agreement between two approximations is not improved by making one of them better. All fourteen went together after that. The same trap caught the body transform, which multiplied double points through a float matrix: an identity translation perturbed every coordinate, and a Boolean of two identical cylinders returned empty.

And then the migration was complete, every declaration on the path was double, and a vertex still disagreed with the curve it sits on by 4.371 × 10⁻⁸ — single precision — at every scale. The number named itself. Half of sin(float π) is 4.3711390 × 10⁻⁸ exactly: a radius times the error in a π that had been rounded to float. Something in the chain was still narrow, and no declaration in it was.

They were in the helpers. Four lines of file-local convenience, in two files:

```
float dot(const Vec3& a, const Vec3& b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
float length(const Vec3& a)             { return std::sqrt(dot(a, a)); }
```

Every arc parameter in this kernel is an `atan2(dot(w, bi), dot(w, ref))`, and `atan2` cannot return a double angle from float arguments. However wide the points were, each parameter was rounded on its way *out* of a three-line helper — and then stored into a double field, where it looked perfectly wide. Widening them moved the box from 4.371 × 10⁻⁸ to 2.0 × 10⁻¹⁶, eight orders of magnitude, for a diff of twenty lines.

The cylinder did not move. It stayed at 4.371 × 10⁻⁸ exactly, and the test written to pin the fix is what said so: a check that a half-turn arc parameter equals double π rather than float π, run over *both* operands. It reported 3.1415927410125732 — float π, to the last bit. The box's arcs come from the imprint's parameter pair and the cylinder's from the seam ring, and the two were being rounded by different code. A handful of locals held the second set: the imprint's own `et0`/`et1`, the crossing fractions, the edge parameters, the accessor pair that reads and writes a vector's components, the line-crossing solver's output. All of them declared `float`, all of them assigned from double expressions and into double fields.

> Every declaration on the path was already double. The narrowing was in return values, three calls down. Types could not show it and did not.

**What was proven.** Worst curve-to-vertex disagreement on a cylinder-through-box imprint, both operands, at three scales:

| scale | box | cylinder | relative |
|---|---|---|---|
| 1 | 4.371e-08 → 2.001e-16 | 4.371e-08 → 1.531e-16 | 2.0e-16 |
| 100 | 4.371e-06 → 1.589e-14 | 4.371e-06 → 1.531e-14 | 1.6e-16 |
| 10000 | 4.371e-04 → 2.417e-12 | 4.371e-04 → 1.531e-12 | 2.4e-16 |

The relative error is now flat at about 2 × 10⁻¹⁶ across four orders of scale, which is what double looks like. A float in the chain looks like a fixed 10⁻⁷ that grows in absolute terms with the model — and a test at a single scale cannot tell those two apart, which is exactly how this survived the entire migration. The guard measures rather than inspects, asserts both operands, asserts the bound relatively, and keeps the π fingerprint that found the second half. Two thousand four hundred and ninety-nine tests pass.

One thing this was expected to unlock and did not. The tolerance defaults were sized for float at roughly eight units in the last place, and the plan was to tighten them once the ceiling lifted. Tightened to 10⁻⁶, six tests fail. Five of them pin the constants themselves and would move with them — four tolerance tests, and a mesh-seam characterization whose sample separations are chosen relative to the current value. The sixth is a real failure, and the first account given of it was that the mesh Boolean still runs on float positions and therefore needs the loose weld band. That account was wrong, and the next chapter is what it took to find out.

## 38. The tolerance was not a workaround

The claim at the end of the last chapter was reasonable and it was untested. A float mesh, a loose weld band, and a Boolean that only stays watertight with the band loose — the inference writes itself. It is also exactly the shape of inference this book has been burned by twice already, so it got probed rather than believed.

Tightened to 10⁻⁶ and run over the failing fixture — a box against a sphere at five tessellations, twenty random offsets each, three operators, three hundred results — **one** configuration leaks. One, under all three operators, always the same offset. If float resolution were the mechanism, the failures would be scattered across the fixture in proportion to how much seam each configuration has. A single repeat offender is not a precision profile; it is a particular piece of geometry.

The points it cannot weld are 3.19 × 10⁻⁶ apart. At unit scale float resolves about 1.2 × 10⁻⁷, so that gap is some twenty-five units in the last place — an order of magnitude too large to be rounding. And the three vertices involved say plainly what they are:

| vertex | position | what it is |
|---|---|---|
| v64 | (−0.126088738, 0.526467443, 1.000000000) | seam crossing, on the face and the sphere |
| v66 | (−0.126070619, 0.526456952, 1.000000000) | seam crossing, on the face and the sphere |
| v204 | (−0.126087785, 0.526467562, 0.999996960) | a sphere **vertex**, three microns below the face |

All three sit on the sphere to within a micron of its 1.2 radius. At this offset the sphere grazes the box's top face closely enough that its tessellation drops a vertex three microns underneath the plane, while the seam crossing for the same region lands exactly on it. The distance the weld has to bridge is not error. It is the real gap between two genuinely different points that a modeller means to be the same one.

> The band is a modelling scale, not a numerical workaround. Widening the Boolean's arithmetic would not move it by a single bit, because the distance being bridged is real.

Which settles the tolerance question in a different way than expected. Tightening the band does not clean anything up; it redefines what "the same point" means, and a sphere grazing a box then cannot close its seam. That decision is available — but it belongs to whoever is prepared to say that three-micron features in a two-unit model are distinct, and on its own it buys nothing. The habit that applies here was already written down during the epsilon migration and had to be applied to a conclusion of this book's own: *check that an epsilon's problem is reachable and fixable before migrating it.* This one is neither.

**What was proven.** The grazing configuration is pinned by measurement rather than by reference to the fixture's random seed: a sphere vertex lands between 10⁻⁷ and 10⁻⁵ below the box face — bounded on *both* sides, so the test fails if the configuration stops grazing and fails if the gap shrinks to something float rounding could explain — and that vertex is verified to lie on the sphere rather than being an artifact of the cut. All three operators are watertight at the shipped band, and stay watertight with the same model built at a hundredth and a hundred times the size, which is what distinguishes a coincidence scale from a constant fitted to one fixture. Two thousand five hundred and two tests pass.

The correction worth carrying forward is not about tolerance at all. Three chapters ago the union was accused of an error that belonged to the tessellator. Here the mesh Boolean's storage was accused of an error that belonged to the geometry. Both times the accusation was structurally plausible, arrived at by reasoning rather than measurement, and written down as though it had been established.

---

## 39. Six bites and no ring

The last chapter left a circle imprinted on a sphere's face at the top of the list of things still owed, and the reason it was owed was a single line. `circleLiesOnSurface` knew about planes and it knew about cylinders, and everything else it refused. So a Circle seam offered to a spherical face died at the guard, the sphere came out of the imprint untouched, and every Boolean involving one returned empty.

The condition itself is short. A cylinder contains exactly one family of circles, the latitudes; a sphere contains *every* plane section, which is a two-parameter family and is precisely what a box face or a second sphere cuts. Writing **d** for centre-to-centre, a circle of radius *r* lies on the sphere when **d** points along the circle's own axis and |**d**|² + *r*² equals *R*². Eight lines. It was written, it compiled, the sphere started accepting seams, and every invariant the kernel owns went on saying yes.

The body was closed. Both validators were clean. Euler characteristic held at two. Every new vertex sat on the sphere to 2.2 × 10⁻¹⁶ — not near it, *on* it, to the last bit a double has. Face classification against the box was exact: over a hundred and thirty-one faces, zero disagreements with a closed-form oracle. And the Boolean still came back empty.

What was actually there was this:

| the seam should be | what it was |
|---|---|
| 12 arcs, 12 vertices | 6 arcs, 12 vertices |
| every vertex degree 2 | every vertex degree **1** |
| one closed ring | six disconnected bites |

Six lens-shaped nibbles taken out of a sphere, sharing nothing, going nowhere. Confetti.

> No topological or geometric invariant is a witness to this. A closed shell, clean validators, correct Euler number and vertices exact to the last bit are all perfectly compatible with a seam that is not connected. Only a census of the seam's own connectivity tells a ring from confetti.

The cause was a rule this book has already written down once. Chapter 33 established that a crossing landing exactly *at* an edge endpoint has to count, because the imprint's own progress puts it there — cut one face at a latitude and its neighbour's uprights are already split at that height. That fix went into the straight-edge path, where the problem had appeared. The arc path was written afterwards and asked for a fraction strictly inside the arc.

On a cylinder that omission is nearly invisible. A side patch is bounded by two straight uprights and two rim arcs, and the uprights carry the crossings, so the arc path is barely exercised. A sphere has no straight edge anywhere. A lat-lon patch is bounded by four arcs, so *every* crossing on a sphere is an arc crossing — and the moment one patch is cut, its neighbours' shared boundary arcs are already split at that exact point. Demanding a strictly interior fraction reported every neighbour uncrossed. The seam stopped at the first face it touched, six times over.

> A rule learned on one surface was filed as a special case of that surface. On the cylinder the endpoint crossing is an edge case; on the sphere it is the general one. The same sentence describes a corner in one geometry and the whole behaviour in another.

Admitting the endpoint — the same slack, clamped the same way, that the straight-edge path had carried for six chapters — takes each seam to twelve arcs over twelve vertices, every one of degree two.

And then sphere against sphere closes. That pair had been sitting in the curved-Boolean baseline as an expected-empty since the arc began; it is now three watertight solids. Union, intersection and difference at three separations, both validators clean on all nine.

The volume check needed care of its own. Inclusion–exclusion says union plus intersection must equal the two operands added together, and measured against the *pristine* spheres it was off by 5 × 10⁻². The tempting reading is a real volume error at the half-percent level. It is not: the Boolean's output carries the seam vertices and the pristine operands do not, so a mesh taken from the result is simply a finer approximation of a curved surface than a mesh taken from the input. Compared against the *mutually imprinted* operands — the same vertices, the same tessellation — the identity holds to about 3 × 10⁻⁸ relative at every refinement level. The comparison was measuring tessellation density, not volume.

**What was proven.** The seam census is the headline assertion, over five configurations including a great circle tilted off the grid so that crossings fall on arcs of both families; reverting the endpoint rule alone fails it. A section that coincides with the tessellation's own equator or meridians is refused, and that is correct rather than a gap — there is no face for it to cross — which is pinned alongside the tilted circle that *is* cut, so the refusal cannot quietly become a refusal of great circles in general. Circles the sphere does not contain are still rejected: wrong radius, off-axis centre, radius too large for *R*. Both halves are load-bearing by individual revert — the endpoint rule fails three tests, the containment case fails seven. Two thousand five hundred and eight tests ran and two thousand five hundred and three passed, the rest being hardware skips, with no regressions.

Box against sphere is still empty, and it is worth being precise about what that now means. Its seams imprint correctly — seventy-two cuts, six closed rings, both operands closed and valid — and its face classification is exact against a closed-form oracle. Everything upstream of the sew is right. The fault has moved downstream, and for once it has moved without taking a new disguise.

---

## 40. The long way round

The fault had not moved downstream. It had been sitting four layers upstream the whole time, in a single arc.

The previous chapter ended pointing at the sew, and the sew is where the symptom lived: a census of what the Boolean handed to its assembler showed every edge of one seam ring being claimed by three faces instead of two, which `fromFaces` refuses, correctly. Chasing that backwards produced a measurement worth stating plainly, because it looked for a while like the answer. After the mutual imprint, the sphere carried **ten pairs of duplicate vertices**, all on one seam, each pair 2.7 × 10⁻⁸ apart. The Boolean welds at 10⁻⁴, four orders coarser, so it collapsed each pair onto one point — and a collapsed pair is exactly how an edge acquires a third user.

Two point seven times ten to the minus eight is float resolution at unit scale, and there was indeed a float in the chain: the helper assembling an intersection Curve took its radius as a `float`, between a `double` computation and a `double` field. The seam came back as 0.66332507133483887 where the exact value is 0.66332504433416373. One word, and it is a real defect — the box's ring is cut at the reported radius while the sphere's ring is pinned to the sphere and the cutting plane and lands at the true one, so a single circle becomes two rings that do not quite coincide.

It was written up as the cause. It is not the cause. With the parameter widened the two rings agree to 5.6 × 10⁻¹⁷ — and there are still ten duplicate pairs, and the sew census is byte-for-byte what it was. The vertices were never *nearly* coincident by accident; they were *duplicates*, and making them agree perfectly only made them perfect duplicates.

> Third time in five chapters, and this time the reasoned-out explanation was about my own fix. A number that matches the expected magnitude is not thereby the cause. The fix stayed — it is right — but the claim came out of the commit message and the source comment now says which failure it does not cure.

So: where do ten duplicate vertices come from? Hand-stepping the mutual imprint one direction at a time answered it in one line. The first pass over the sphere is clean — a hundred and fifty-eight vertices, no duplicates. The second pass accepts exactly **one** cut, and that one cut adds all ten.

That call is the reconcile step: a face whose boundary already lies on the seam circle has nothing left to imprint, but it may not agree with the *other* operand on how finely the seam is divided, so the pass splits its arcs at the partner's vertices. Its comment says that splitting at a point which is already a vertex is a no-op, which is what makes it safe to run every pass. That is true. It simply was not what was happening.

The arc it was splitting spanned **6.0333 radians — ninety-six per cent of the circle**. Its own endpoints are 0.2499 radians apart. It was the complement: the arc that shares both endpoints and goes the long way round, swallowing ten vertices belonging to eleven other faces, which the reconcile pass then dutifully split it at.

The selector that chose it tests one of the two candidate arcs and flips if the answer comes back "outside". That is sound exactly as long as the containment test is trustworthy. For a curved face, containment is decided in the surface's (u,v) domain — and a sphere's parametrisation is singular at ±uAxis, where the longitude is an atan2 with no value. `makeSphere` puts uAxis on X while the *tessellation's* grid poles are on Z, so the singularity does not sit at a pole of the mesh where one might look for it. It sits in the middle of ordinary-looking patches near ±X, which is precisely where a box's ±X faces cut their seam.

Of the seventy-two seam arcs, seventy-one discriminated cleanly. One reported **both** candidates outside — and "if not inside, flip" turns "I cannot tell" into a confident wrong answer.

| | span | endpoints apart | containment says |
|---|---|---|---|
| the true arc | 0.2499 | 0.2499 | outside |
| the complement | 6.0333 | 0.2499 | outside |

The repair is to ask about both candidates, and when they discriminate to trust them exactly as before. When they do not, the tie breaks on something coarser that cannot be poisoned by the parametrisation: the arc lying in the face cannot leave the bounding box of the face's own boundary, and the complement leaves it immediately. Coarse is the point — the box never evaluates (u,v) at all. That it agrees with the containment test wherever the containment test works was measured rather than hoped: all seventy-one, in both directions, sixty-five keep and six flip.

**What was proven.** Box against sphere sews. Concentric at radius 1.2 it gives a union of a hundred and two faces, an intersection of seventy-eight and a difference of seventy-eight, all watertight, both validators clean, and inclusion–exclusion exact — union plus intersection equals the two operands, to zero. At radius 0.8, with the sphere strictly inside the box, the answers can be named rather than measured: the union **is** the box's six faces, the intersection **is** the sphere's ninety-six, and the difference is the box carrying a spherical void, a hundred and two faces across two shells. Offset box/sphere and box/cylinder still return empty, and watertight-or-empty holds across the whole sweep of offsets and radii.

One note on the test, because it failed in an instructive way before it passed. The first draft asserted the span after `imprintMutually` and passed with the fix reverted — by then the reconcile pass has already chopped the offending arc into eleven ordinary-looking pieces, and nothing spans the long way round any more. The assertion has to be made after a one-way imprint, at the only moment the defect is visible. A test that cannot fail is worse than no test, and the only reason this one was caught is that reverting the fix is a step in the routine rather than a formality.

> Two arcs sharing two endpoints satisfy every invariant this kernel owns. Chapter 39 said that when a change alters connectivity, no whole-body invariant reports on it. This is the same sentence with a different noun: the complement is not a broken arc, it is a *different* arc that is equally valid locally and wrong globally. Assert the quantity that distinguishes them, and assert it where it is still distinguishable.

---

## 41. Refusing to cut is not the safe option

Concentric box and sphere worked. Move the sphere half a unit along X and all three operators went back to empty. That is a suspicious shape for a bug — the geometry is not more degenerate off-centre, it is *less* symmetric — so the first move was a survey rather than a hypothesis: fourteen configurations, each reporting whether the imprint succeeded, whether both operands stayed valid, and how many duplicate vertices and complement arcs it left behind.

The survey came back clean everywhere. No complement arcs, no duplicates worth the name, both operands closed and both validators satisfied in all fourteen. Whatever was wrong was not the imprint failing; it was the imprint *succeeding* and still leaving something undone.

An edge census of what the Boolean handed its assembler said the rest: twenty-four edges with one face instead of two, nothing doubled, nothing reused. Not corruption — **absence**. Faces that should have been there were not. And every one of those edges lay in the plane x = 1, the face the sphere exits through.

Listing the box faces the Boolean kept gave five: the far face, uncut, and the four sides. The exit face contributed nothing at all. Looking at the imprinted box directly explained why in one line — it was still a single face, sixteen vertices, spanning the whole plane, classified Inside from its centre and dropped whole.

The reason it was never cut is a sentence in `imprintCurve`: handle two crossings, handle the special case of both on one edge, refuse everything else. Here the circle at x = 1 has radius 1.0909 — larger than the square's inradius of 1, smaller than its half-diagonal of 1.414 — so it leaves and re-enters through all four edges. **Eight crossings.** Refused.

> Refusing an imprint is not a conservative choice. The imprint exists to guarantee that no face straddles the other solid, and a refusal leaves exactly that state behind, with no other part of the pipeline positioned to notice. The face is then classified as a whole, from one sample point, and everything on the far side of the circle is discarded with it. A cut that cannot be made should not be silently skipped by the one step whose contract it violates.

Concentric had worked only by luck of scale: with the sphere centred, the exit circle has radius 0.663, smaller than the face, so it takes the fully-interior path that was already implemented. The offset case is not a harder version of the centred one. It is a different branch that was never written.

Ordering the eight crossings around the *circle* — not along the boundary, which is unrelated once more than one edge is involved — gives eight arcs that alternate inside the face and outside it. The four inside ones are the cuts, one per corner. Two details are load-bearing rather than incidental. Every crossing has to be resolved to a vertex *before* any face is cut, because two crossings land on each side of the square and a fraction along an edge goes stale the moment that edge is split; each is captured as an absolute parameter instead, and each shared edge is split from its far end inward so the remaining parameters stay inside the half that keeps the edge's identity. And all four cuts have to happen in one call, because a face that has been cut once carries the arc on its boundary, and the already-segmented guard — which is right to stop a face being bitten along its own rim — would refuse the other three.

The exit face now comes apart into four corner pieces and one middle piece, and each piece agrees with a closed-form containment test. Offset box against sphere sews at every radius and offset tried. So does the offset cylinder through a box, which had been the last pair the curved-Boolean baseline still pinned as empty, and which turns out to have been waiting on exactly the same cut.

**What was proven.** Five faces on the exit plane, four classified Outside and one Inside, each checked against the sphere's own equation rather than against the classifier being tested. Union and intersection watertight with both validators clean across five offset box/sphere configurations and the offset cylinder, with inclusion–exclusion holding at around 3 × 10⁻⁸ throughout. The concentric answers are asserted unchanged, including the two that can be named rather than measured. Two thousand five hundred and twenty-two tests ran, two thousand five hundred and seventeen passed, the rest hardware skips.

One thing this chapter does **not** claim. Difference does not conserve volume on some offsets: D plus I falls short of A by a few parts in ten thousand. It would have been easy to let the watertight assertions imply otherwise. Two measurements settle what it is. Refining does not shrink it — across subdivisions zero to four it converges on −2.61 × 10⁻⁴ rather than tending to zero, so it is a missing piece of volume and not a tessellation artifact. And reverting this chapter's change and re-measuring on configurations that already sewed beforehand gives −4.06, −3.39 and −2.03 × 10⁻⁴: it is **older than this work**, which merely makes it reachable in more places. It is pinned with bounds on both sides, so it can neither drift nor be quietly fixed without the test saying so.

> A result that is watertight, valid and wrong is the worst of the three. The invariants this kernel owns are all topological, and volume is not among them — so when a change makes a new operator reachable, the honest move is to measure the quantity none of the validators look at, and to write down what it says even when it is not the answer you wanted.

---

## 42. Where the ring happened to start

The residual left over from the last chapter was two thousandths of a unit out of eight, and the reason to chase it rather than tolerate it was a single measurement already in hand: refining did not shrink it. Across five subdivision levels it settled on −2.61 × 10⁻⁴ and stayed there. A sampling error tends to zero. This was volume the triangulation had never enclosed.

Three hypotheses, and the first two were wrong — which is worth recording, because each was plausible enough to have been written up.

*Lost arcs.* A chord refines to itself, so a body whose analytic arcs went missing has exactly this signature: a fixed deficit that refinement cannot close. Counting Circle-valued edges in both results took a minute and killed it — 142 in the difference, 142 in the intersection, identical in every configuration tried.

*The fan apex, via reversal.* Difference reverses the vertex ring of every face it takes from the second operand, which moves the vertex a fan starts from. Reversing every ring of a whole sphere and re-measuring gave a difference of exactly zero, at every refinement level. Also dead.

The third attempt abandoned hypotheses and just took the triangles apart. Every triangle the tessellator emits for the difference and for the intersection, sorted by which surface it lies on, signed volumes summed per group:

| | box triangles | sphere triangles |
|---|---|---|
| intersection | +2.796080609 | **+2.189830002** |
| difference | +5.203919401 | **−2.192173031** |
| sum | 8.000000010 ✓ | **−0.002343029** |

The box faces partition the box perfectly. The shared patch — the same fifty-six faces, the same vertices to the last bit, verified — does not cancel. One hundred and sixteen triangles on each side, and they are not the same one hundred and sixteen triangles.

Which sends the second hypothesis back, not as reversal but as its cause. The apex was `ring[0]`: wherever the ring happened to start. For a planar face that is a free choice, since every fan of a planar polygon encloses the same volume. For a curved patch it is not free at all — a four-vertex patch on a sphere is not planar, and its two diagonals span different surfaces enclosing different amounts of space. Difference reverses the ring; a reversed ring starts at a different vertex; the same patch is therefore triangulated one way inside the intersection and the other way inside the difference.

The earlier experiment had said zero because it reversed the rings of a *closed* sphere, where each patch's two diagonal choices are mirrored somewhere else on the body and the error cancels against itself. A Boolean keeps a *partial* patch. There is nothing left for it to cancel against.

> The whole-body test was not wrong, it was answering a different question. An invariant measured over a complete, symmetric object can be satisfied by errors that annihilate in pairs. The configuration that matters is the one where the symmetry has been cut away.

The repair is to stop letting the ring's starting position decide. A fan from a *fixed* apex emits the same diagonals whichever way the ring is traversed — only the winding flips, which is exactly what a reversed face wants — so the apex is now the lexicographically smallest vertex position, a property of the geometry rather than of the data structure. The same physical vertex wins in both bodies, both fans pick the same diagonals, and the two copies cancel to zero instead of to two thousandths.

**What was proven.** Difference plus intersection equals the first operand, asserted at three refinement levels across seven configurations — several levels, because a triangulation that never enclosed the volume does not recover it by refining, so a single level could be met by luck. The shared patch is asserted to cancel to 10⁻⁹ directly, which is the assertion that names the mechanism; a volume identity alone could be satisfied by two errors that offset. Union plus intersection still equals both operands. Planar Booleans are asserted *exactly* — four, twelve and four — since the apex rule cannot touch them and anything that moves those numbers has broken something else. Two thousand five hundred and twenty-six tests ran and two thousand five hundred and twenty-one passed, the rest hardware skips; a change inside the routine every Body is tessellated through moved nothing else at all.

The characterization written one chapter earlier, which pinned this residual with bounds so it could not drift, is deleted rather than relaxed. It did its job — the failing message said *the gap may be FIXED; verify and replace this characterization with a real conservation assertion*, and that is what happened — and a test that permits a defect is worse than no test once the defect is gone.

---

## 43. The third time is the confession

A box against a sphere at offset 0.5 worked. The same pair at offset 0.7 was empty in all three operators. Nothing about the second configuration is more degenerate than the first; the sphere is simply half a unit further along the same axis.

The survey said the imprint succeeded, both operands stayed valid, and no arcs or vertices were duplicated. The classifier said every face of both bodies agreed with a closed-form oracle — zero disagreements, at every offset tried. So the failure was upstream of the sew and downstream of nothing.

Counting what the box *should* have come out as settled where to look. With the sphere at 0.7 the exit circle has radius 1.1619, between the face's inradius of 1 and its half-diagonal of 1.414, so it leaves and re-enters through all four edges — the eight-crossing case from two chapters ago, worth five pieces. Four side faces cut into two each, one far face untouched: fourteen faces. The box had ten. The exit face had not been cut at all.

The crossing search reported zero crossings on every one of that face's sixteen boundary edges. At offset 0.5, the same search on the same face reported eight — each one at

    s = 0.9999999999999999

A few units in the last place inside the endpoint. At offset 0.7 the same eight crossings land a few units in the last place on the *other* side of 1, and a solver that requires `s > 0 && s < 1` reports nothing at all.

That the crossings sit on the endpoints is not the fixture being awkward. It is forced. Two seam circles cut by two box faces that share an edge are both sections of the same sphere, so their common points are exactly where the sphere crosses the shared edge — they *must* meet there. Whichever face is imprinted first splits the edge at that point, and the second circle then arrives at a vertex, every time, for every offset. Whether the root lands at 0.99999999999999989 or 1.00000000000000011 is the only thing left to chance, and it decides whether the Boolean returns a solid or nothing.

> The same rule has now been needed three times: on a cylinder's uprights, on a sphere's bounding arcs, and here on the planar path, which is the oldest of the three. It is not three bugs. Each solver was written for the case where the circle cuts cleanly through the middle of an edge, and it is the imprint's OWN progress that stops that being true — every cut a face makes lands a vertex on some neighbour's boundary. A precondition that the algorithm invalidates as it runs will keep being assumed until it is written down as false.

The repair is the same one, applied a third time: a length tolerance on the segment's own ends, accept a root within it, clamp, and let the caller's existing snap reuse the vertex that is already there. Two roots collapsing onto the same end count once rather than twice.

**What was proven.** Offset 0.7 sews, at 125, 49 and 53 faces, watertight with both validators clean, and *both* conservation identities hold at float precision at every refinement — union plus intersection equals the operands, difference plus intersection equals the box, all within 3.2 × 10⁻⁸. The exit face is asserted to segment into five pieces at both offsets, so the assertion no longer depends on which side of an endpoint the arithmetic falls. One test states the forcing argument directly, checking that the box carries a vertex exactly where the two seams must meet, rather than trusting that the fixture happens to produce one. And two guards keep the slack honest: a sphere strictly inside the box still cuts nothing, and an exactly tangent sphere — touching one face at a single point — is still not treated as a cut. Two thousand five hundred and thirty-one tests ran, two thousand five hundred and twenty-six passed, the rest hardware skips.

The baseline that opened this arc is retired in the same commit. It was written to pin box/sphere, box/cylinder and sphere/sphere as empty, with failure messages instructing whoever flipped them to replace it. Every fixture in it has now flipped, so it becomes an assertion that they stay working. One result among them is still empty — the offset cylinder's *difference*, whose union and intersection both sew — and it is named on its own line rather than folded into the loop, because a gap averaged into a passing test is a gap nobody will find again.

---

## 44. A sliver seven thousandths wide

The last chapter ended pointing at two gaps. The first turned out not to be one, and the second was hiding somewhere no tolerance would have reached.

**The gap that wasn't.** "The offset cylinder's difference alone fails" had been written down twice, and it was an artifact of the fixture. Surveying radius against offset — twenty-four configurations rather than the one the baseline happened to use — every generic pair sews for all three operators, and every failure is an *exact tangency*. The baseline's cylinder has radius exactly 1 inside a box of half-width 1, so its surface touches the side walls along four lines.

The census says what that means. For the difference, four edges are each claimed by four faces, and those edges are precisely the four tangent generatrices. That is not a defect: box minus a tangent cylinder pinches to zero thickness along each of those lines, so the exact answer is a non-manifold solid. `fromFaces` refuses it and the Boolean returns empty, which is the watertight-or-empty contract doing its job.

> A conclusion drawn from one fixture is a conclusion about that fixture. Both times this was written down as a property of the *operator*, and both times the survey that would have corrected it was four lines of code.

**The gap that was.** Box against sphere at offsets 0.1 and 0.3 had correct segmentation, and every face of both operands agreed with a closed-form classifier. The sew census showed seam edges with three users, and the +X face's *outer* corner edges used once — meaning the annulus, whose material lies entirely outside the sphere, was being kept.

It was kept because it classified `OnBoundary`, and `selectFace` reads that as a coincident-face pair. The tolerance is 10⁻⁵. The sample point was **0.028 inside the sphere** — nearly three thousand times that. No tolerance would have rescued it, which is what ruled out the first explanation and forced the question of where the point actually was.

| | |
|---|---|
| annulus sample, distance from seam centre | 0.750609 |
| hole polygon inradius | 0.743719 |
| true seam circle radius | **0.793725** |

Outside the polygon by seven thousandths. Inside the circle by four hundredths.

Every ring the sample search tests against is a chordal polygon standing in for the face's real boundary, and for a seam that boundary is a circle. Between the two lies a sliver that is outside the polygon and inside the curve, and a point there passes `pointInPlanarPolygon` while not being on the face at all. `classifyPoint` then judges it against the *sphere* — the real surface, not the approximation — and the two disagree, exactly as they must.

The routine took the first candidate that passed. A comfortable candidate existed; it simply came later in the list. Taking the one with the greatest clearance from every boundary ring instead moves the sample from radius 0.7506 to 1.0607, and both configurations sew with volume conserved at float precision.

> Two representations of the same boundary, consulted by two different steps, will disagree in the band between them. A test that only asks whether a point is on the right side of the approximation will keep handing back points that live in that band. Ask how far it is from the boundary, not which side of it.

**What was tried and thrown away.** The cap a seam leaves on a sphere is bounded by a planar circle, so a fan from its boundary lays flat triangles across the disk — a lid where the surface should bulge — and refining does not help, because subdividing a boundary that lies in a plane only produces more points in that plane. Measured, the lid put the probe point exactly on a triangle at subdivisions 0, 1 and 2 alike. Giving curved faces an interior apex projected onto their own surface removed it cleanly.

It was reverted. It did not fix the configurations it was built for, and it broke six tests that deliberately pin `toMesh(0)` to the exact chord arithmetic of a faceted solid — that is, it quietly redefines what subdivision zero *means*. That is a decision about the tessellation contract and deserves to be taken on its own terms, not smuggled in as a side effect of a classification fix.

**What was proven.** The sample is asserted against the analytic seam circle rather than the polygon that admitted the bad point, across four configurations. The annulus classifies Outside against a closed-form oracle. Both offsets sew with union-plus-intersection and difference-plus-intersection holding at float precision. The sample stays deterministic across repeated construction, and an unholed planar face still receives its centroid bit for bit — stated for planar faces only, and deliberately, since on a curved face `Surface::normal` is the axis rather than a face normal and the early return never fired there anyway. Exact tangencies are pinned as returning clean empty results. Two thousand five hundred and thirty-six tests ran and two thousand five hundred and thirty-one passed.


---

## 45. One cause wearing two faces

The last configuration failing for anything other than tangency — a sphere of radius 0.8 pushed 0.3 along X through a unit box — was wrong in two visible ways at once. Six vertices on the sphere were duplicated, coincident with the originals to within 3 × 10⁻¹⁶, so the sew welded each pair, every seam edge acquired a third user and the assembler refused. And one seam arc traced the complement: 5.3716 radians the long way round where the truth is 0.9116.

They are one defect. A complement arc runs past the ring vertices belonging to its neighbours, and the pass that reconciles the two operands' discretizations then splits it at each of them — which is exactly where six new vertices came from, on top of six that already existed.

The cause is a test that was not merely uncertain but *wrong*. The arc selector had already been taught, two chapters ago, to try both candidates and to break a tie on the face's bounding box. That handles the case where containment cannot decide. Here it decided, confidently, and picked the complement. Containment on a curved face is evaluated in the surface's (u,v) domain, a sphere's parametrisation is singular at ±uAxis, and this seam sits at 61° of latitude — close enough to invert the answer. Of eight seam arcs, seven were decided correctly and one was not; the face that got it wrong is the mirror image of a face that got it right, with identical spans.

The repair is to promote the bounding box from tie-breaker to veto, and the reason that is sound rather than expedient is worth stating exactly. An arc lying inside a face cannot leave the box that bounds the face's boundary. Being in the box is therefore *necessary*. A containment verdict the box contradicts cannot be true, whatever the parametrisation thinks — so it is discarded, the confidently wrong answer becomes an undecided one, and the tie-break already in place resolves it. Across the eight arcs the box and the (u,v) test agree on seven, and on the eighth the box is right.

> A test that is wrong is worse than a test that abstains, and no amount of tie-breaking helps, because a tie-break only runs when the test admits it does not know. What rescues it is a second test that cannot be wrong in the same way — a *necessary* condition, cheap enough to be exact, that never touches the machinery the first one failed in.

**And something that was written, worked, and was removed.** The duplicate vertices were fixed first, directly: a guard stopping the reconcile pass from splitting where a vertex already exists. It was correct, and the tests passed. Then the veto landed, and re-measuring showed the duplicates were gone *without* the guard, across all fourteen configurations. It was doing no work. Reverting it alone gives identical results and zero duplicates everywhere — so it is not in the commit. Shipping it would have implied it was holding something up, and the next person to read it would have believed that.

The duplicate-vertex assertion stays. It is a real invariant, it is how this was found, and nothing in the source now claims to enforce it directly — which is the honest arrangement when a symptom is cured by fixing its cause.

**What was proven.** Every box/sphere configuration that is not an exact tangency now sews, for all three operators, with both conservation identities holding at float precision. That is asserted as a *sweep* rather than a list of the configurations anyone happened to think of, so one that stops working is caught whether or not it was ever named. The complement check is by span, since the wrong arc meets the same endpoints and satisfies every validator this kernel owns. Tangencies are pinned as returning cleanly empty — their true results are not manifold solids, and empty is the contract rather than an omission. Two thousand five hundred and forty-six tests ran and two thousand five hundred and forty-one passed.


---

## 46. A rare follow-up

With every box/sphere configuration sewing, the next question was not which pair to try but what a *sequence* of operations does — because a feature history is a chain, and the editor had just been wired to make one.

The canonical test is a plate drilled several times. A four-by-four plate, four bores of radius 0.4, drilled one after another, each operation consuming the result of the last.

The first hole was perfect. The second returned empty.

Everything upstream was clean, and unusually so: the imprint succeeded, both operands stayed closed and valid, there were no duplicate vertices, and the plate's twenty-four faces classified against the drill with **zero** disagreements and zero on-boundary cases. The sew census showed eight edges with one face instead of two — three adjacent facets of the second bore's own wall, missing.

Checking the *drill's* faces rather than the plate's found them. Three of its fifty, sampled at mid-plate, sitting plainly inside the plate, came back **Outside** it. They were dropped from the difference, and the eight edges they should have carried were left dangling.

The two bores are 2.4 apart with radius 0.4. They do not touch. So the second hole was not failing because of the first hole's geometry — it was failing because of the first hole's *existence*.

`classifyPoint` is a parity ray cast against the tessellation. And the tessellator collected inner rings like this:

```cpp
std::vector<uint32_t> inner;
for (uint32_t il : fc.innerLoops) {
    inner = buildRing(m_loops[il].first);
    if (inner.size() >= 3) break;       // ← one hole, and stop
}
```

One hole per face. The comment beside the bridging code said it outright — *multiple holes on one face are a rare follow-up* — and after the first bore, the plate's top and bottom faces each carry one hole, so the second bore gives them two. The second hole never reached the triangles. The face was drawn solid across it, the ray crossed material that was not there, and the drill's own wall read as outside the plate.

> A hole that does not reach the tessellation is not a hole. Every representation this kernel keeps of a face — the loops, the validators, the Euler count — knew about the second hole; the only one that mattered for classification was the one that dropped it. It is worth asking, of any structure kept in two forms, which form the decisions are actually read from.

The repair is to bridge every hole rather than one: right to left, each joined by a two-way cut from its rightmost vertex to the first polygon edge a ray meets going in +X. The ray is cast against the polygon built *so far* and not against the outer ring, so a hole bridging leftward may land on a hole already merged — which is correct, and is exactly why the order matters. A hole with no edge to its right is left out rather than spliced across the boundary: wrong, but bounded, where a blind splice corrupts the entire polygon and takes the face with it.

**What was proven.** The chain runs — four holes, then two unions, every step watertight with both validators clean. The numbers are exact rather than merely plausible: each of the four identical bores removes 0.493630, and each boss adds 2.000000. That identical bores must remove identical volume is the assertion that matters, because anything else means a hole was lost or counted twice, and it is checked bore against bore rather than against a formula. The face is confirmed to actually carry three holes, so the volume test cannot pass for some unrelated reason. And every bore's axis is asserted to classify Outside — which is the property that was really broken, stated directly rather than through its consequences. Two thousand five hundred and fifty-one tests ran and two thousand five hundred and forty-six passed.

A comment that calls something a rare follow-up is a prediction, and this one had been sitting in the file since the hole path was written. It was wrong the first time anybody drilled two holes in a plate.


---

## 47. A primitive you could not model with

Every remaining item on the list was a *capability* rather than a defect: pairs of surfaces the intersector simply declined. Cylinder against cylinder, sphere against cylinder, anything against a cone. Picking between them was a matter of measuring what each one cost.

The measurement was blunt. A box against a cone imprints **nothing**: six faces stay six, seventeen stay seventeen, and all three operators return empty. Not degraded — untouched. `intersectSurfaces` knew four pairs, and none of them involved a cone, so no seam was ever offered and no face was ever cut. A cone is one of the primitives the editor places from a keystroke. It was a primitive you could not model with.

Two families of section are curves this kernel can already carry, and they are the two a box needs:

A plane perpendicular to the axis cuts a circle — and the interesting part is the radius. A cylinder's sections all share one; a cone's ring at axial distance *v* from the apex has radius slope · *v*. The radius is a function of where the plane falls. That is the entire difference between the two surfaces, and it is what the test asserts, at four heights, because a constant radius would still be a perfectly valid Circle and geometrically wrong.

A plane holding both the apex and the axis cuts the two rulings lying in it. The apex itself is a point rather than a zero-radius circle, and a plane behind it meets nothing at all, the cone being single-napped.

> The boundary was worth writing as carefully as the capability. An oblique plane cuts an ellipse, a parabola or a hyperbola — none of which is a Line or a Circle — so it stays Unsupported rather than approximated by something nearby. Concretely: a box face *parallel* to the axis but missing the apex cuts a hyperbola, so a cone wider than the box around it is still out of scope. A test asserts that, alongside the ones asserting what now works. Declining is a feature, and an untested decline is the kind that quietly stops declining.

**What was proven.** Box against cone sews for all three operators at four radius and offset combinations, watertight, both validators clean, both conservation identities at float precision. And the intersection is checked to be a *frustum of the right size* — bounded below by the faceted solid the kernel actually stores and above by the smooth cone it approximates — because every topological check this kernel owns would happily accept any closed shape at all. Two thousand five hundred and fifty-eight tests ran and two thousand five hundred and fifty-three passed.

**An aside that took longer than the feature.** Partway through, the full suite began dying with a segfault, always at the same Vulkan test. It would have been easy to assume it was mine and easier still to re-run until it went green. It is neither. Five runs of that test alone are clean, three runs of its whole suite alone are clean, and it only dies as part of the full run — so it depends on state accumulated by the thousands of tests before it. A clean run peaks at 145 MB, so it is not exhaustion. The cone path under valgrind across three hundred cone booleans reports zero errors, so it is not corruption from the new code. And it had already happened two chapters earlier, when no cone code existed. It is pre-existing, intermittent at about one run in four, and it is now written down under its own name rather than folded into a passing summary.

> A suite that is green on the second attempt is not green. The honest report is the number of tests that ran, the number that passed, and the name of the thing that intermittently kills it.


---

## 48. The cap that would not cut

Two cylinders standing side by side imprinted nothing and returned empty, for the same reason the cone did: the intersector had never been taught the pair. When the axes are **parallel** the answer is not hard. The whole problem collapses into the plane perpendicular to the shared direction, where it is two circles; solve those and extrude each solution point back along the axis into a ruling. The distance between the axes decides which of six answers you get, from *far apart* through *externally tangent* to *the same surface twice*. Axes that are skew or crossing are a quartic space curve — the Steinmetz curve — and stay out of scope rather than being approximated by something nearby.

That much was routine. What it uncovered was not.

With the rulings arriving, the cylinders' side walls cut correctly — and their **caps** still would not. The caps stayed two faces throughout, every operator still returned empty, and twenty-four boundary edges were left with one face instead of two.

The seam was being offered. `intersectSurfaces` reported a circle of radius 0.7 centred on the cap, which is exactly right. The imprint refused it, and reported **zero crossings** with a boundary the circle demonstrably crosses twice.

A cylinder's cap is a planar face, but its boundary is the cylinder's **rim** — a circular arc, not a straight edge. So the crossing search took the arc path, and that path asks: *where does this boundary arc cross the imprint circle's plane?* Which is the right question, and was written for the case it was needed in — a sphere's lat-lon patch, whose bounding arcs genuinely do pass through the cutting plane. Here the arc lies **in** that plane. The projection of the arc's frame onto the plane normal vanishes, the formulation collapses to a radius of zero, and the code correctly concludes that an arc parallel to the cut plane cannot cross it. It simply is not the question that needed asking.

> Two coplanar circles do not meet by crossing each other's plane. They meet the way circles have always met, in the plane they share. The bug was not an error in the arithmetic; it was a correct answer to a question that stopped applying when the configuration changed underneath it.

And the case is not about cylinders. It is the boundary of *every* planar face bounded by arcs rather than straight edges — a cylinder's cap, a cone's base, a disk. Any of them, offered a coplanar seam circle, would have refused it in the same way.

**What was proven.** Parallel-cylinder booleans sew across six configurations, including coplanar caps, differing heights, an axial offset, and a small off-centre bore — all watertight, both validators clean, both conservation identities at float precision. The intersection is checked against the closed-form **lens prism**: the lens area of two overlapping circles times the shared height, computed from the radii and centre distance rather than from anything else the kernel believes. The rulings are verified to lie on *both* cylinders, since a Line of the right kind in the wrong place passes a type check and nothing else. Every degenerate distance is named — tangent outside, tangent inside, apart, nested, concentric, coincident. And the cap cut is asserted on its own, because it is invisible in a boolean's face count and it is the half that stayed broken after the obvious half was fixed. Two thousand five hundred and sixty-three tests ran, two thousand five hundred and fifty-eight passed.

Reverting each half separately fails three tests each, and different ones — which is the cheapest evidence that they really are two defects and not one described twice.


---

## 49. Bands

A sphere centred on a cylinder's axis makes the whole arrangement a surface of revolution, so whatever the two share must be one too — rings, not a general curve. The algebra is one line: a common point sits at the cylinder's radius from the axis and the sphere's radius from its centre, so *rc² + z² = R²*, and there are two rings at *z = ±√(R² − rc²)*. They merge into one where the sphere is exactly inscribed in the bore, and there is nothing at all when it is narrower. Move the centre off the axis and the symmetry is gone; the meeting becomes a quartic and is declined.

It needed a new kind of answer — `TwoCircles`, the curved counterpart of the two rulings a plane already cuts on a cylinder. There is exactly one place in the kernel that switches on the kind, and that switch is exhaustive, so adding the value made the compiler point at the line that needed writing. That is the cheapest kind of change to make: the one where the type system does the searching.

Then the sweep. Radii 1.20 through 1.40 worked. Radii 1.05 through 1.15 returned empty. 1.45 and 1.50 returned empty; 1.55 upward worked again.

**Bands.** Not a threshold, not a tolerance, not a monotone failure — alternating stripes across a continuous parameter, which is the shape of a defect that is triggered by a coincidence rather than by a magnitude. Laying the ring's latitude beside the sphere's own grid latitudes made the pattern plain: every failing band straddled one of them.

The sliver was the clue. Where the seam falls near a grid latitude it cuts a patch close to its edge, leaving a narrow face between the two. Twenty-four such faces, and twenty-four faces missing from the result.

The first check said classification was perfect — zero disagreements. That check was worthless, and it is worth saying why. It compared the classifier's verdict against an oracle **evaluated at the sample point**. If the sample point is not representative of the face, the oracle and the classifier agree about a point that does not matter and the face is still wrong. The right question is not *does the classifier agree about this point* but *does this point speak for this face*.

Asking that gave it up immediately. Every one of the sphere's one hundred and twenty face samples sat at a distance of 1.0735 from the centre — on a sphere of radius 1.10. Not one of them was on the sphere.

A curved face was being sampled at the centroid of its outline, and the centroid of a ring of points on a curved surface sags inside it by the difference between a chord and its arc. Usually harmless. For the narrow faces it is not: it moved the sample from radius 1.008, outside the cylinder where that face's material actually is, to 0.978, inside it. Twenty-four faces classified Inside, dropped from the union, seventy-two boundary edges left dangling, empty result. At radius 1.30 the same sag flips nothing at all — which is precisely why the neighbouring configurations worked and made the whole thing look like a property of the radius.

> This is the third time the same shape of bug has appeared: the point chosen to speak for a face was a point not on the face. The face centroid sat in the opening of a hole; the sample sat in the sliver between a seam's polygon and its true circle; and now it sags off a curved surface entirely. Each time the fix was local and each time the principle was the same one, which suggests the principle deserves to be checked wherever such a point is chosen, rather than waiting for the next configuration to expose it.

**What was proven.** Ball-on-rod sews across six radii with both conservation identities at float precision, and the intersection is checked against its closed form: a cylindrical core capped at each end by a spherical dome.

**Two contracts changed, and neither was suppressed.** The baseline pinned sphere-against-cylinder as out of scope; it is now partly in, so the test states exactly where the boundary lies — on the axis it is two rings, off the axis it is still declined. And a test asserted that a face with no holes is sampled at its centroid, *including curved faces*. What that had been pinning, all along, was the sample point failing to lie on its own face. It now asserts the stronger property for curved faces and keeps the bit-for-bit centroid guarantee for planar ones.

**And two errors of my own, both caught by measuring rather than by reasoning.** The volume check predicted 12.88 and got 8.13, because I had written the intersection as a sphere with two caps sliced off; it is the opposite shape, a cylindrical core with two caps added. And a radius was compared against the literal 1.1 where the body stores a float — 2.4 × 10⁻⁸ away, which a 10⁻⁹ tolerance duly rejected. Neither was the kernel's fault, and both would have been easy to blame on it.


---

## 50. Going to look

Three chapters in a row ended with the same sentence in different words: the point chosen to speak for a face was a point the face does not contain. The outline's average sat in the opening of a hole. The sample sat in the sliver between a seam's chordal polygon and the circle it stands for. The curved sample sagged off its surface by the difference between a chord and its arc.

Three is enough. The fourth configuration was going to turn up eventually, and the choice was whether to be told about it or to go and look.

Looking is cheap here, because there is only one place to look. `faceSamplePoint` is the sole chooser of such a point that feeds a decision — `classifyFace` reads it, and the Boolean's face selection reads it — and everything else named `centroid` in this kernel is a mesh construction where the average is the intended answer rather than a stand-in for something else. Within that one function, exactly one case had never been examined: a **curved** face carrying a hole. The curved path returned the projected centroid and asked nothing about holes at all, so on a face whose middle is an opening it returned the middle of the opening.

Then the more interesting question, which is not *is it wrong* but *can anyone reach it*. An unexercised guard is its own kind of debt — two chapters ago a fix that worked was removed for exactly that reason — so the answer had to be measured rather than assumed. Sweeping every configuration this kernel can imprint, two hundred and fifty-four bodies across box against sphere, cylinder, cone, cylinder pairs, sphere pairs and chained plates, produces **zero** curved faces carrying an inner loop. The interior-circle path is gated to planes, and every curved pair that could cut an interior seam is a quartic the intersector declines. Through a Boolean, it cannot happen.

Through `fromFaces` it can, and `fromFaces` is public. A cylindrical patch with a hole punched in the middle, built through the ordinary API, was sampled at the exact centre of that hole.

> That distinction is the whole decision. A defect nobody can construct is a guard waiting to rot; a defect constructible through a public entry point is a defect. The measurement did not tell me whether to fix it — it told me which of those two I was looking at.

The repair gives curved holed faces the search the planar ones already had: the same candidate ladder, containment decided in the surface's own (u,v) domain where a trimmed boundary is an ordinary polygon, and every candidate projected onto the surface before being judged, so the winner lies on the face in both senses at once. Where the parametrisation cannot answer — it is singular at a sphere's poles — the projected centroid stands, which is exactly what it was before.

**What was proven.** The sample on a holed cylindrical patch is on the cylinder to nine decimal places and clear of the opening; reverting the fix puts it back at the hole's centre. Unholed curved faces and every planar face are unchanged. The choice is deterministic across repeated construction. And one test asserts the *reachability* finding itself — that no Boolean this kernel can perform produces such a face today — so a later reader knows why the guard exists and does not delete it as dead, and so the day that stops being true is noticed rather than discovered. Two thousand five hundred and seventy-three tests ran, two thousand five hundred and sixty-eight passed.

There is no fourth instance to report, which is the point. The pattern was closed by looking for it rather than by being shown it, and that is the first time in this book that has happened.

---

## 51. The last bit

The previous chapter ended by ruling something out. Every operand face was used exactly once across the union and the intersection — a true multiset check, matched by distance rather than by a formatted key, at every tolerance from 10⁻⁹ to 10⁻⁴, with nothing left over on either side. So the Boolean was partitioning its operands correctly, and the volume it was losing was not a face going missing.

That left a stranger fact standing. At subdivision 0 the conservation identity held to 2.4 × 10⁻⁷. At subdivision 2 the same configuration was out by 1.2 × 10⁻². Same faces, same edges, same vertices; more points on the arcs, and a hundredth of the body gone.

I had a lead from the chapter before, and it was wrong. One face had been recorded as fanned in the operand and ear-clipped in the result, and the convexity test that decides between them compares turns against an *absolute* threshold of 10⁻¹², which is four orders of magnitude below the noise floor of the float array it reads. That is a genuine defect and the measurement confirmed it exactly: at the midpoints of a subdivided straight edge, where the turn is algebraically zero, the operand read +0.0 and the result read +1.47 × 10⁻⁸ and −1.48 × 10⁻⁸. The negative one counted as a reversal and sent an otherwise convex ring to the ear-clipper.

Then I fixed it and nothing happened. Not "a smaller discrepancy" — the volumes were identical to every digit printed, the triangle counts unchanged, the fourteen violations across two thousand configurations unmoved iteration for iteration. Both routes cover the same polygon; the branch differed and the region did not.

> Twice now a well-formed explanation has been dismantled by the measurement that was supposed to confirm it. The pattern in both is the same: I found something that *is* wrong, near the thing that *is* failing, and let proximity do the work of evidence.

So I stopped guessing and instrumented the tessellator to report, per face, the area its triangles actually cover against the area of its own ring. Across two thousand configurations there was no mismatch anywhere — every face's triangulation covers its own boundary exactly. Which killed the whole category: the tessellation was not wrong, so the *inputs* to it had to differ.

They did not. The rings came out identical, point for point, in traversal order, in both bodies. The edges matched in kind and in curvature. The vertices matched. What differed was which triangles were drawn between them — and by then the arithmetic had nowhere left to hide.

A curved patch is fanned from one apex, and the apex is pinned to the lexicographically smallest position in the ring. That was a deliberate repair, made in an earlier chapter for a good reason: a fan from a *fixed* apex draws the same diagonals whichever way the ring is walked, so the same patch appearing in an intersection and in a difference cancels exactly instead of approximately. The rule is right. Its implementation compared the **float** positions with `==`.

On a seam, coordinates being equal is not a coincidence — it is the construction. Every point imprinted onto a planar face carries that face's plane coordinate exactly, so ring after ring holds several points sharing an *x*, the comparison falls to a tie, and the tie is decided by whatever sits in the last bit. In the fixture, a B-rep vertex and an arc midpoint agreed on *x* to fifteen significant figures. In the imprinted operand the midpoint was 2.8 × 10⁻¹⁶ more negative and took the apex. In the union the two were bitwise equal, the comparison fell through to *y*, and the vertex took it. Two different fans over the same ring; and on a curved patch the two diagonals span different surfaces and enclose different volumes.

The amplification is the part worth keeping. Those two doubles differ by 2.8 × 10⁻¹⁶ — but they straddle a float rounding boundary, so narrowed they differ by 1.2 × 10⁻⁷, four hundred million times more. The exact data was sitting in the B-rep the whole time. Only the copy the decision was read from had lost the distinction between *equal* and *nearly equal*.

The repair reads the doubles and compares them on a grid of 10⁻¹² of the ring's own scale. Rounding to a grid rather than comparing within a tolerance matters: it keeps the comparison a total order, so the minimum does not depend on where the ring happens to start, which is the property the fixed apex existed to provide in the first place.

**What was proven.** Across two thousand configurations of the fuzzer's own generator the conservation violations went from fourteen to one, and the worst from 8.8 × 10⁻⁴ to 1.3 × 10⁻⁶. The fuzzer's gate had been written to tolerate exactly this — up to four violations at up to 2 × 10⁻³, bounded on both sides so the disagreement could neither grow nor be quietly fixed — and that allowance is now spent and the gate reads zero. The new tests assert the property directly rather than through a total: every triangle in the results is a triangle the operands had. And the mechanism is pinned on its own, in four points on a cylinder, where a single double ulp that crosses a float boundary must not change which diagonal is drawn.

The convexity finding was kept, but only after it earned a measurement of its own. It moves no volume and no area — reverting it leaves every identity unchanged to the digit — yet without it thirteen of twelve hundred and twenty triangles still differ between operand and result. It buys triangle-level identity and nothing else, and the comment says so, because an overclaimed fix is worse than a modest one.

> The lesson is not about floats. A tie-break is a decision, and this one was being made on values that the construction guarantees are equal. A rule that only behaves when its inputs are bitwise identical is not a rule; it is a coin flip wearing one.

And the older lesson got its sharpest instance yet. Every validator the kernel owns passed throughout: watertight, integral, χ = 2, and — as of the previous chapter — a verified exact partition of the operands' faces. The faces were identical, the edges were identical, the rings were identical point for point. Only the diagonals differed, and not one check in this kernel weighs anything.

Two thousand five hundred and seventy-eight tests ran; two thousand five hundred and seventy-three passed, with five hardware skips and no failures.

---

## 52. The slit

The lead left at the end of the last chapter was a single number: one planar face, somewhere in two thousand configurations, whose triangles covered fifty-seven per cent more area than its own boundary encloses. Everything else about it was clean.

Finding it took one sweep. Iteration 1781, a sphere of radius 0.7688 turned a quarter turn about X against a box, and the offending body was not a Boolean result at all — it was the imprinted box operand, which made it much easier to look at.

The ear clipper had given up. It stalls by design rather than dropping a remnant, because a missing patch would put a hole in a shell that is supposed to be closed, and fanning whatever is left is the bounded cost of never doing that. Here it stalled with eighteen of thirty-nine vertices remaining, and printing them said why in one glance: the polygon walked out along a chain of points and back along the same coordinates. Every candidate ear was reported blocked.

The two chains were different vertices at identical positions — v121 against v109, v120 against v108, v34 against v9. The clipper excludes a point that coincides with one of an ear's own corners, because the bridge it builds for holes walks in and out along the same cut and would otherwise block itself. It recognised those points by their *index*. A slit's two sides have different indices, so each one sat on a corner of every candidate ear and counted as obstructing it.

Two of the three pairs were not even bitwise equal — 2.2 × 10⁻¹⁶ and 1.1 × 10⁻¹⁶ apart, with the third at exactly zero — which is the same distinction between *equal* and *nearly equal* that the previous chapter turned on.

Where did a slit come from? The seam circle where the sphere meets that box face was carried by **two** curve records. One spanned parameters from −0.548 to −1.154. A later one spanned +0.548 to −1.988, straight over the top of it. The first cut had split the face and its arc had become boundary; the second cut then retraced that boundary instead of crossing the interior. A cut is supposed to be a diagonal, and ending on two loop vertices does not make it one.

That looked like the real defect, upstream, and three rules were built to stop it: refuse a cut whose span runs through any vertex of its own loop; refuse one that retraces an edge of that loop; shorten the cut to the first vertex it meets. The first two work — the duplicate vertices go, the area comes right.

All three cost the same five configurations of two thousand, each losing every one of its three Boolean operations.

I nearly shipped that, on the reasoning that those five must have been building slits too and were better off returning a clean empty. Then I checked, and **not one of the five had a single duplicate vertex**. The guards were refusing perfectly good cuts. The trade was five working configurations for one malformed one, and it was the wrong way round.

> The habit that saved this was not scepticism about the fix. It was insisting that "these were broken anyway" is a claim with a measurement attached, and then taking the measurement. It said the opposite of what I expected, which is the only reason it was worth taking.

So the repair belongs where the malformed loop is read, not where it is made. A loop that walks out and back is a self-touching polygon, and the standard reading of one is to split it at the pinch: at a repeated position *i* < *j*, the ranges [*i*,*j*) and [*j*,*i*) are each closed loops, and recursing separates every slit. Pieces enclosing no area are dropped and the rest are triangulated independently.

The first attempt at that kept only the single substantial piece, with a comment claiming a pinch never actually divides a face. The instrumentation printed *two* substantial pieces, of eighteen points and nine, with the remaining twelve forming the zero-area slit between them. Both carry material. The comment was wrong before it was written.

**What was proven.** Across the two-thousand-configuration corpus, faces whose triangles fail to cover their own boundary went from four hundred and two to **zero**, and the cost is nothing at all: 2101 sews before, 2101 after, the same 3897 empties, and the conservation identities unchanged at one violation of 1.3 × 10⁻⁶.

**And what was not.** The imprinted box's total area is still above the box's own — 0.167 at the finest subdivision, down from 0.213. That residue is not the tessellator's to fix. It is drawing faithfully what the B-rep holds, and what the B-rep holds is two different faces over the same material, because the duplicated seam is still there. The test bounds it on both sides and says so, and will fail the day it is fixed upstream so that the bound gets retired rather than quietly kept.

Two thousand five hundred and eighty-two tests ran; two thousand five hundred and seventy-seven passed, with five hardware skips.

---

## 53. The cone that was not one

The invariant `booleanToBody` promises is that its result is watertight or it is empty. Across two thousand configurations, exactly two results were neither, and the previous chapter left them as a lead. Both were the same configuration — a cone of radius 1.3289 and height 2.2657 against a box — and both failed the same way: a vertex sitting a fiftieth of a unit off the surface of its own face.

The first useful thing the probe said was that the Boolean had nothing to do with it. The imprinted **cone alone**, before any operator ran, already failed `checkGeometry`. Eight of its side faces carried vertices between 0.019 and 0.025 off their own cone, and the Union and Difference merely inherited them.

The second useful thing was a comparison. A cylinder's two rims are sixteen **Circle** edges. A cone's base is sixteen **Line** edges.

That is the whole defect. A cone's side face declares `SurfaceKind::Cone` — an exact conical surface, a correctness fix in its own right from an earlier chapter, since tagging it a cylinder had been a false statement rather than an approximation. A face's boundary has to lie on the face's surface. The circle where the cone meets its base plane does. The chord between two points of that circle does not: it dips inside the cone everywhere except at its two endpoints. `makeCylinder` upgrades its rims to arcs after `fromFaces` for precisely this reason, in four lines with a comment explaining why. `makeCone` never got those four lines.

Nothing caught it for the same reason nothing catches most of what this book is about. `checkGeometry` tests **vertices** against their faces' surfaces, and a chord's two endpoints are on the cone. The edge between them is not — but no vertex lives there. Until an imprint puts one there, which is what an imprint is for.

> The sharpest way to state what was wrong is not that the base was chords. It is that the body asserted a smooth cone in two places and an inscribed pyramid in a third. The surfaces said cone. `massProperties` integrated a cone and returned πr²h/3 exactly, at every segment count, and had done all along. Only the base edges bounded something else. Two out of three is not a rounding error, it is a contradiction, and the one that disagreed was the one nobody had looked at.

The repair is the cylinder's four lines.

**What was proven.** Across the two-thousand-configuration corpus: non-empty results failing validation went from two to **zero**, so watertight-or-empty now holds throughout. The volume-conservation identities went from one violation of 1.3 × 10⁻⁶ to **none at all** — the last one, and it was this cone. And the count of configurations that successfully sew went **up**, from 2101 to 2107, because a cone whose base agrees with its sides imprints cleanly where an inconsistent one bailed. A correctness fix that also gains six results is a rare shape and worth noticing: the six were not being declined for a good reason.

**A contract changed and is stated rather than suppressed.** A test asserted that a cone's area is its exact lateral surface plus an inscribed n-gon base, with a comment explaining that `fromFaces` derives Line edges. That was a faithful description of the body as built, and the body as built was wrong. The cone's area is now exact — lateral plus a true disk, independent of segment count, as the cylinder's has been for some time.

**And one thing that did not change, pinned so it is not mistaken for this.** The tessellated volume of a cone converges in the segment count and hardly at all in subdivision: ninety per cent of the true cone at eight segments, still only ninety-four at the finest refinement, against ninety-nine point nine at sixty-four. A conical face is fanned from a point on its base rather than from the apex, so its triangles chord across the base arc however finely that arc is refined. That is the tessellator under-refining the inside of a curved patch, already named among the open items, and it is not introduced here. Before this change the coarse figure was 1.885618, which is exactly the inscribed pyramid — with a chord base that is genuinely what the body enclosed, and the tessellation was faithful to it. Now the body is right and the tessellation lags it, which is the better way round.

Two thousand five hundred and eighty-eight tests ran; two thousand five hundred and eighty-three passed, with five hardware skips.

---

*This edition ends here, but the logbook does not. The curved Boolean works now, centred or offset, exact on the analytic volume; the ground it stands on has been checked, which it had not been; and the representation underneath is double from storage through parameters, primitives and the file format, verified by measurement at four orders of scale rather than by reading its own type declarations. Three habits earned their place in this stretch. Measure the subject against an oracle that does not share its weaknesses, or the agreement means nothing. When a change alters how a face is bounded, measure its area, because every other invariant this kernel owns will happily certify a face that is geometrically wrong. And a precision claim must be tested at more than one scale, because the signature of a lost digit is a relative error that holds steady while the absolute one grows. A fourth habit was not earned so much as re-learned, twice in four chapters: an explanation reasoned out from structure, however well it fits, is not a finding until it has been measured — and the accusation lands on innocent code as easily as guilty. An eighth is the cheapest of them all: when a body states the same thing in more than one place — its surfaces, its edges, its integrated mass — ask whether they agree, because the one that disagrees is the one nobody has looked at. A seventh came from a guard that was nearly shipped: when a fix costs something, the claim that what it costs was broken anyway is a measurement, not a reading of the code — taken here, it said the opposite, and five working configurations were saved by asking. A sixth is the newest and the least comfortable: a decision made by comparing coordinates for exact equality is not a decision at all when the construction guarantees those coordinates are equal — the last bit answers, and the last bit is noise. A fifth arrived with the sphere and is the sharpest of them: when a change alters the *connectivity* of something, no invariant over the whole body will report on it. A closed shell, clean validators, the right Euler number and vertices exact to the last bit all held while the seam they were supposed to describe lay in six disconnected pieces. Count the thing itself. What remains is named: exact tangencies, whose true results are not manifold solids and which therefore return cleanly empty by contract rather than by omission; surface pairs still outside the analytic scope altogether — cylinders whose axes are skew or crossing, a sphere against an off-axis cylinder, a cone against anything curved, each of them a quartic and each declined rather than approximated; a tessellator that under-refines the inside of a curved patch, now known to be held there by degenerate triangles that turn out to be load-bearing; and a chained Boolean that still fails as the left operand. The tolerance band is no longer on that list — it is doing the job it exists for, and tightening it would be a decision about what counts as coincident rather than a repair. Beyond those, one decision rather than a defect — an attribute layer — which gets more expensive with everything built on top of it.*
