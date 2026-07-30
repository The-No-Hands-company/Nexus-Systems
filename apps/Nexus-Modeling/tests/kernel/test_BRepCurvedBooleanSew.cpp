// Phase 4e of the true-analytic-curved-boolean arc — THE CURVED SEAM CLOSES.
//
// A cylinder driven straight through a box is the canonical curved CSG case, and all
// three ops returned an empty body. Phases 1–4d built everything the sew needs — arcs
// that survive assembly, circle imprint onto a cylindrical face, the driver wiring, a
// seam ring shared vertex-for-vertex, inner loops carried through, faces classified by
// their material, side faces split at both levels — and it was still empty. What
// remained was not in the sew at all.
//
// The imprint PUNCHED A HOLE. Where a seam circle fell entirely inside a face, that face
// got an inner loop and nothing else: the ring's arcs had one coedge each, so the box
// came out of the imprint as an open shell with two circular openings. Two things follow,
// and both were fatal.
//
// First, classification stops working. classifyPoint counts crossings of a ray against
// the tessellated shell, and parity is only meaningful for a closed one — a ray that
// enters through an opening and leaves through material crosses once and reports the
// point as inside. Measured on box(2,2,2) against cylinder(r=0.5,h=4,16): five of the
// sixteen faces in the cylinder's lower stub, a clear half-unit BELOW a box that ends at
// z=-1, came back Inside. The pristine box classifies the same five points Outside.
//
// Second, and even with classification fixed: the disk inside the ring is the material
// that CAPS the intersection. Discarded at the imprint, box ∩ cylinder can never be a
// closed solid, because the two faces that close it do not exist anywhere.
//
// An imprint segments a boundary; it never removes material. So a fully-interior circle
// now splits the face into the ring AND the disk, sharing the ring's arcs edge for edge,
// and the operand stays closed. These tests pin that invariant, the three ops it unblocks
// (exact face counts, both validators, closed), the inclusion-exclusion volume identity,
// and — via refinement, the only thing that can tell an arc from its chord — that the
// analytic circles survive into the result.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace nexus::geometry::brep::testing {


namespace {

// The clean cylinder-through-box: the cylinder's footprint lies strictly inside the box's
// ±X/±Y walls, so each of the two seams is a FULL circle interior to a box face, and the
// cylinder runs past both ±Z faces. 16 segments, so the chord-level answer is exact.
constexpr float kR = 0.5f;
constexpr float kH = 4.f;
constexpr uint32_t kSeg = 16;

Body theBox() { return makeBox(2.f, 2.f, 2.f); }
Body theCylinder() { return makeCylinder(kR, kH, kSeg); }

double signedVolume(const Mesh& m)
{
    Mesh t = m;
    (void)t.topology().triangulate();
    const auto& p = t.attributes().positions();
    double v = 0.0;
    for (size_t i = 0; i < t.topology().faceCount(); ++i) {
        const auto& idx = t.topology().face(i).indices;
        if (idx.size() != 3) continue;
        const auto &a = p[idx[0]], &b = p[idx[1]], &c = p[idx[2]];
        v += (static_cast<double>(a.x) *
                  (static_cast<double>(b.y) * c.z - static_cast<double>(b.z) * c.y) -
              static_cast<double>(a.y) *
                  (static_cast<double>(b.x) * c.z - static_cast<double>(b.z) * c.x) +
              static_cast<double>(a.z) *
                  (static_cast<double>(b.x) * c.y - static_cast<double>(b.y) * c.x)) /
             6.0;
    }
    return std::abs(v);
}

size_t arcEdgeCount(const Body& b)
{
    size_t n = 0;
    for (uint32_t e = 0; e < static_cast<uint32_t>(b.edgeCount()); ++e) {
        if (!b.edge(e).alive) continue;
        const uint32_t c = b.edge(e).curve;
        if (c != kInvalid && b.curve(c).kind == CurveKind::Circle) ++n;
    }
    return n;
}

// Area of a regular n-gon inscribed in a circle of radius r — the exact cross-section of
// this cylinder's chord-level (subdivisions=0) tessellation.
double ngonArea(double r, double n) { return 0.5 * n * r * r * std::sin(6.283185307179586 / n); }

}  // namespace

// ── THE INVARIANT ────────────────────────────────────────────────────────────────
// An imprint segments the boundary and never removes material, so both operands come out
// of it CLOSED. This is what classification rests on, and it is the one that was broken.
TEST(CurvedBooleanSew, ImprintLeavesBothOperandsClosed)
{
    Body a = theBox();
    Body b = theCylinder();
    const double volA = signedVolume(a.toMesh(4)), volB = signedVolume(b.toMesh(4));
    ASSERT_TRUE(imprintMutually(a, b));

    EXPECT_TRUE(a.isClosed()) << "the imprinted box has an opening — the seam disk was removed";
    EXPECT_TRUE(b.isClosed()) << "the imprinted cylinder has an opening";
    EXPECT_EQ(a.checkIntegrity().boundaryEdges, 0u);
    EXPECT_EQ(b.checkIntegrity().boundaryEdges, 0u);
    EXPECT_TRUE(a.checkIntegrity().ok) << a.checkIntegrity().reason;
    EXPECT_TRUE(b.checkIntegrity().ok) << b.checkIntegrity().reason;
    EXPECT_TRUE(a.checkGeometry().ok) << a.checkGeometry().reason;
    EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;
    // Segmentation is volume-preserving: nothing was added or taken away.
    EXPECT_NEAR(signedVolume(a.toMesh(4)), volA, 1e-5);
    EXPECT_NEAR(signedVolume(b.toMesh(4)), volB, 1e-5);
}

// The consequence that made it fatal, pinned directly: a point outside the imprinted
// operand must still classify as outside. The five that failed sat in the cylinder's
// lower stub at z=-1.5, a half unit below a box ending at z=-1; the ray reached the box's
// interior through the opening the imprint had left and counted one crossing on its way
// out. So the truth is checked against the PRISTINE box, which was never in doubt.
TEST(CurvedBooleanSew, ImprintDoesNotChangeWhatThePointClassifierSays)
{
    const Body pristine = theBox();
    Body a = theBox();
    Body b = theCylinder();
    ASSERT_TRUE(imprintMutually(a, b));

    size_t checked = 0;
    for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
        if (!b.face(f).alive) continue;
        const Vec3 s = b.faceSamplePoint(f);
        EXPECT_EQ(a.classifyPoint(s), pristine.classifyPoint(s))
            << "face " << f << " sample (" << s.x << "," << s.y << "," << s.z
            << "): imprinting the box changed how it classifies a point";
        ++checked;
    }
    EXPECT_GT(checked, 40u) << "the cylinder should be split into many faces by now";
}

// ── WHAT THE INVARIANT UNBLOCKS ──────────────────────────────────────────────────
// All three ops sew, with the face counts the geometry dictates. Union: the box's four
// walls + its two rings + the cylinder's two stubs (16 each) + its two caps. Intersection
// (the plug): the 16 faces of the cylinder's middle band + a disk at each end.
// Difference (the box with a tunnel): four walls + two rings + the 16 tunnel walls.
TEST(CurvedBooleanSew, CylinderThroughBoxSewsForAllThreeOps)
{
    const Body box = theBox(), cyl = theCylinder();
    struct Case { BooleanOp op; size_t faces; const char* what; };
    const Case cases[] = {
        {BooleanOp::Union, 4 + 2 + 2 * kSeg + 2, "union"},
        {BooleanOp::Intersection, kSeg + 2, "intersection (the plug)"},
        {BooleanOp::Difference, 4 + 2 + kSeg, "difference (the box with a tunnel)"},
    };
    for (const Case& c : cases) {
        const Body r = booleanToBody(box, cyl, c.op);
        ASSERT_GT(r.faceCount(), 0u) << c.what << " came back empty";
        EXPECT_TRUE(r.isClosed()) << c.what << " is not closed";
        EXPECT_TRUE(r.checkIntegrity().ok) << c.what << ": " << r.checkIntegrity().reason;
        EXPECT_TRUE(r.checkGeometry().ok) << c.what << ": " << r.checkGeometry().reason;
        EXPECT_EQ(r.checkIntegrity().boundaryEdges, 0u) << c.what;
        EXPECT_EQ(r.faceCount(), c.faces) << c.what << " has the wrong number of faces";
    }
}

// The strongest correctness check available: |A∪B| + |A∩B| = |A| + |B|, measured on the
// tessellations so it holds level by level. A boolean that dropped, duplicated or
// misplaced any face breaks it. The absolute values are pinned too, against the exact
// 16-gon arithmetic — the identity alone would survive both results being wrong the same
// way.
TEST(CurvedBooleanSew, VolumesSatisfyInclusionExclusionAndTheExactChordArithmetic)
{
    const Body box = theBox(), cyl = theCylinder();
    const Body U = booleanToBody(box, cyl, BooleanOp::Union);
    const Body I = booleanToBody(box, cyl, BooleanOp::Intersection);
    const Body D = booleanToBody(box, cyl, BooleanOp::Difference);
    ASSERT_GT(U.faceCount(), 0u);
    ASSERT_GT(I.faceCount(), 0u);
    ASSERT_GT(D.faceCount(), 0u);

    for (uint32_t s : {0u, 1u, 2u, 4u}) {
        const double vBox = signedVolume(box.toMesh(s)), vCyl = signedVolume(cyl.toMesh(s));
        const double vU = signedVolume(U.toMesh(s)), vI = signedVolume(I.toMesh(s));
        const double vD = signedVolume(D.toMesh(s));
        EXPECT_NEAR(vU + vI, vBox + vCyl, 1e-5) << "inclusion-exclusion fails at subdivisions " << s;
        EXPECT_NEAR(vD + vI, vBox, 1e-5) << "(A−B) + (A∩B) != A at subdivisions " << s;
    }

    // Chord level: the cylinder is a 16-gon prism of length 4, and the box keeps the 2
    // units of it that lie inside.
    const double cap = ngonArea(kR, kSeg);
    EXPECT_NEAR(signedVolume(I.toMesh(0)), cap * 2.0, 1e-5);
    EXPECT_NEAR(signedVolume(D.toMesh(0)), 8.0 - cap * 2.0, 1e-5);
    EXPECT_NEAR(signedVolume(U.toMesh(0)), 8.0 + cap * kH - cap * 2.0, 1e-5);
}

// Whether the ANALYTIC circles survived the sew cannot be seen in any topological or
// volumetric invariant at a fixed tessellation, because a chord shares its arc's
// endpoints and refines to itself. Refinement is the only witness: the plug is exactly
// half the cylinder's length, so its volume must track exactly half of the cylinder's own
// refinement series, level for level. Were any rim arc flattened to a chord on the way
// out, the result would refine less than the cylinder does and this would drift.
TEST(CurvedBooleanSew, TheSeamArcsSurviveTheSewAndRefineWithTheCylinder)
{
    const Body box = theBox(), cyl = theCylinder();
    const Body I = booleanToBody(box, cyl, BooleanOp::Intersection);
    ASSERT_GT(I.faceCount(), 0u);

    // Two rings of arcs (one per seam), and the cylinder's own two rims are 2·kSeg.
    EXPECT_EQ(arcEdgeCount(cyl), 2u * kSeg);
    EXPECT_EQ(arcEdgeCount(I), 2u * kSeg) << "the plug's two end circles are not arcs";

    double previous = 0.0;
    for (uint32_t s : {0u, 1u, 2u, 4u, 8u}) {
        const double vI = signedVolume(I.toMesh(s));
        EXPECT_NEAR(vI, signedVolume(cyl.toMesh(s)) * 0.5, 1e-5)
            << "the plug does not refine with the cylinder at subdivisions " << s;
        EXPECT_GT(vI, previous) << "refinement did not grow at subdivisions " << s
                               << " — an arc was flattened to its chord";
        previous = vI;
    }
    // And it is converging on the true circular cross-section, not the 16-gon one.
    EXPECT_GT(signedVolume(I.toMesh(8)), ngonArea(kR, kSeg) * 2.0 + 0.01);
}

// A geometric no-op must stay one. The union with a cylinder that swallows the box whole
// still IS that cylinder, and segmenting faces along seams that bound no change of
// material may add faces but must not move a single point of the solid.
TEST(CurvedBooleanSew, SegmentingAlongASeamThatChangesNothingPreservesTheSolid)
{
    const Body cyl = makeCylinder(1.f, 2.f, 12);  // corner radius √0.5 < 12-gon inradius
    const Body box = makeBox(1.f, 1.f, 1.f);      // strictly inside it
    const Body u = booleanToBody(box, cyl, BooleanOp::Union);
    ASSERT_GT(u.faceCount(), 0u);
    EXPECT_TRUE(u.isClosed());
    EXPECT_TRUE(u.checkIntegrity().ok) << u.checkIntegrity().reason;
    EXPECT_TRUE(u.checkGeometry().ok) << u.checkGeometry().reason;
    for (uint32_t s : {0u, 2u, 4u})
        EXPECT_NEAR(signedVolume(u.toMesh(s)), signedVolume(cyl.toMesh(s)), 1e-5)
            << "the union is no longer the cylinder at subdivisions " << s;
    EXPECT_GE(arcEdgeCount(u), arcEdgeCount(cyl)) << "arcs may be added by a seam, never lost";
}

}  // namespace nexus::geometry::brep::testing
