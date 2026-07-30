// Phase 5 of the true-analytic-curved-boolean arc — THE OFFSET CYLINDER'S ARC-BITE SEAM.
//
// Phase 4e closed the CENTRED cylinder-through-box: the cylinder's footprint lies strictly
// inside the box's side walls, so each seam is a full circle interior to a box face. Push
// the cylinder sideways until its footprint reaches a wall and the seam stops being a
// circle interior to a face and becomes an ARC BITE — a circle that enters and leaves
// through the face's boundary, cutting a lens off it. Every one of those booleans returned
// empty, and three separate things were behind it. This file pins all three.
//
// (1) THE BITE ITSELF WAS DEFERRED, and the reason it was deferred is worth recording.
//     Both crossings land on the SAME boundary edge — measured on box(2,2,2) against a
//     cylinder of radius 0.5, the circle crosses the +X edge at y = ±0.4 and nowhere else —
//     so the natural topology is a two-sided face, arc plus the chord it cuts. A loop of
//     two coedges is rejected by checkIntegrity, which requires three. Weakening that rule
//     was not the answer; splitting the chord at its MIDPOINT is. The bite then has three
//     edges, the two crossings are no longer adjacent in the loop (separately what
//     cutFaceBetween requires), and the extra vertex lies exactly on the original straight
//     boundary, so it costs a redundant vertex and no geometric error at all.
//
// (2) THE BOX'S SIDE WALL WAS NEVER CUT. intersectSurfaces returned Unsupported for a plane
//     PARALLEL to the cylinder's axis. That section is not a conic at all — the plane slices
//     the cylinder along two straight generatrices — and TwoLines already existed in the
//     enum for it. Without it the box's +X face and the cylinder's side faces both stayed
//     whole across the boundary they share.
//
// (3) THE REMAINDER FACE WAS CLASSIFIED INSIDE THE CYLINDER. An arc bite leaves a CONCAVE
//     face, and faceSamplePoint assumed that a face without holes has its outline average
//     on it — true for a convex face, false for this one. Worse, the polygon it tested was
//     built from bare vertices, which replaces the arc with its chord and so encloses the
//     very lens that was removed. Both had to go: the material test now refines curved
//     boundary edges, and the no-holes shortcut only applies when the centroid genuinely
//     passes it. Measured: the remainder's outline average is (0.333, 0), which is 0.367
//     from the cylinder's axis of radius 0.5 — inside it — while the face's own material is
//     almost entirely outside.
//
// (4) THE TWO OPERANDS DISCRETIZED THE SEAM DIFFERENTLY — the box got ONE arc from entry
//     to exit where the cylinder's rim had twelve, so nothing could partner. Fixed by
//     subdividing the bite's arc at the other operand's vertices, with a face already
//     segmented along the circle RECONCILING its discretization on a later round rather
//     than merely refusing (the mutual imprint cuts one operand at a time, so on the round
//     that made the cut there was nothing yet to match).
//
// With those, the offset-cylinder boolean SEWS, and inclusion-exclusion is EXACT on the
// analytic volume at every offset. (An earlier revision of this file claimed a residual
// there; that was toMesh under-refining curved patches, not the boolean — see the note on
// InclusionExclusionIsExactOnTheAnalyticVolume.) The measurement that established it also
// turned up a separate pre-existing defect — a reversed CURVED face could not say it was
// reversed, so a difference through a cylinder had the wrong analytic volume — fixed at the
// end of this file.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/BRepSurfaceIntersect.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshTopologyValidation.h>

#include <gtest/gtest.h>

#include <cmath>
#include <utility>
#include <vector>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {

constexpr uint32_t kSeg = 16;

// A cylinder of radius r along +Z, pushed dx along +X. At dx = 0.7, r = 0.5 its footprint
// straddles the box's +X wall at x = 1, which is what makes the seam an arc bite.
Body offsetCylinder(float r, float dx)
{
    Body b = makeCylinder(r, 4.f, kSeg);
    b.translate({dx, 0.f, 0.f});
    return b;
}

Surface planeAt(Vec3 origin, Vec3 normal)
{
    Surface s;
    s.kind = SurfaceKind::Plane;
    s.origin = origin;
    s.normal = normal;
    return s;
}

Surface cylinderAt(Vec3 axisPoint, Vec3 axis, float radius)
{
    Surface s;
    s.kind = SurfaceKind::Cylinder;
    s.origin = axisPoint;
    s.normal = axis;
    s.radius = radius;
    return s;
}

// Total area of the mesh triangles lying in the plane z == level.
double areaAtZ(const Mesh& m, float level)
{
    Mesh t = m;
    (void)t.topology().triangulate();
    const auto& p = t.attributes().positions();
    double a = 0.0;
    for (size_t i = 0; i < t.topology().faceCount(); ++i) {
        const auto& idx = t.topology().face(i).indices;
        if (idx.size() != 3) continue;
        const auto &v0 = p[idx[0]], &v1 = p[idx[1]], &v2 = p[idx[2]];
        if (std::fabs(v0.z - level) > 1e-4f || std::fabs(v1.z - level) > 1e-4f ||
            std::fabs(v2.z - level) > 1e-4f)
            continue;
        const double e1x = v1.x - v0.x, e1y = v1.y - v0.y;
        const double e2x = v2.x - v0.x, e2y = v2.y - v0.y;
        a += 0.5 * std::abs(e1x * e2y - e1y * e2x);
    }
    return a;
}

double signedVolume(const Mesh& m)
{
    Mesh t = m;
    (void)t.topology().triangulate();
    const auto& p = t.attributes().positions();
    double v = 0.0;
    for (size_t i = 0; i < t.topology().faceCount(); ++i) {
        const auto& idx = t.topology().face(i).indices;
        if (idx.size() != 3) continue;
        const auto &a = p[idx[0]], &b2 = p[idx[1]], &c = p[idx[2]];
        v += (static_cast<double>(a.x) *
                  (static_cast<double>(b2.y) * c.z - static_cast<double>(b2.z) * c.y) -
              static_cast<double>(a.y) *
                  (static_cast<double>(b2.x) * c.z - static_cast<double>(b2.z) * c.x) +
              static_cast<double>(a.z) *
                  (static_cast<double>(b2.x) * c.y - static_cast<double>(b2.y) * c.x)) /
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

}  // namespace

// ── (1) THE SAME-EDGE ARC BITE ───────────────────────────────────────────────────
// A circle entering and leaving through one boundary edge splits the face in two, and the
// exact entity counts are asserted because they are what says the chord was halved rather
// than a two-sided loop being smuggled past the validators.
TEST(ArcBiteSeam, SameEdgeBiteSplitsTheFaceAndStaysValid)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = offsetCylinder(0.5f, 0.7f);
    ASSERT_TRUE(imprintMutually(box, cyl));

    // Each bitten face gains the two crossings, the chord midpoint, and then a vertex per
    // cylinder rim vertex the seam arc is subdivided at (fix 4) — so the count is not a
    // fixed number, but the box must have grown and every vertex must still be on the box.
    EXPECT_GT(box.vertexCount(), 8u + 3u * 2u) << "the seam arc was not subdivided";
    for (uint32_t v = 0; v < static_cast<uint32_t>(box.vertexCount()); ++v) {
        if (!box.vertex(v).alive) continue;
        const auto& p = box.vertex(v).point;
        EXPECT_LE(std::max(std::max(std::fabs(p.x), std::fabs(p.y)), std::fabs(p.z)), 1.f + 1e-5f)
            << "an imprint moved a vertex outside the box";
    }
    EXPECT_TRUE(box.isClosed()) << "the bite opened the shell";
    EXPECT_EQ(box.checkIntegrity().boundaryEdges, 0u);
    EXPECT_TRUE(box.checkIntegrity().ok) << box.checkIntegrity().reason;
    EXPECT_TRUE(box.checkGeometry().ok) << box.checkGeometry().reason;

    // Each bite contributes one seam arc, shared by the lens and the remainder — but that
    // arc is then subdivided at the cylinder's rim vertices, so it is a CHAIN of arc edges.
    EXPECT_GE(arcEdgeCount(box), 2u) << "the bites produced no arc edges at all";

    // The crossings sit exactly where the geometry says: 1 = dx + r*cos, so y = ±0.4.
    size_t atCrossing = 0;
    for (uint32_t v = 0; v < static_cast<uint32_t>(box.vertexCount()); ++v) {
        if (!box.vertex(v).alive) continue;
        const auto& p = box.vertex(v).point;
        if (std::fabs(p.x - 1.f) < 1e-5f && std::fabs(std::fabs(p.y) - 0.4f) < 1e-4f)
            ++atCrossing;
    }
    EXPECT_GE(atCrossing, 4u) << "the four ±Z crossing vertices are not at y = ±0.4";
}

// Segmentation is area-preserving, exactly as for the interior circle: lens plus remainder
// still tile the original face. This is the assertion that a wrongly-selected arc (the
// complement, bulging outward) cannot pass — the topology and validators cannot tell them
// apart, only the area can.
TEST(ArcBiteSeam, BiteAndRemainderStillTileTheWholeFace)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = offsetCylinder(0.5f, 0.7f);
    ASSERT_TRUE(imprintMutually(box, cyl));

    EXPECT_NEAR(areaAtZ(box.toMesh(8), 1.f), 4.0, 2e-3) << "the +Z face no longer tiles to 4";
    EXPECT_NEAR(areaAtZ(box.toMesh(8), -1.f), 4.0, 2e-3) << "the -Z face no longer tiles to 4";

    // And the solid is untouched — an imprint segments, it never adds or removes material.
    Mesh m = box.toMesh(8);
    (void)m.topology().triangulate();
    const auto& p = m.attributes().positions();
    double vol = 0.0;
    for (size_t i = 0; i < m.topology().faceCount(); ++i) {
        const auto& idx = m.topology().face(i).indices;
        if (idx.size() != 3) continue;
        const auto &a = p[idx[0]], &b = p[idx[1]], &c = p[idx[2]];
        vol += (static_cast<double>(a.x) * (static_cast<double>(b.y) * c.z - static_cast<double>(b.z) * c.y) -
                static_cast<double>(a.y) * (static_cast<double>(b.x) * c.z - static_cast<double>(b.z) * c.x) +
                static_cast<double>(a.z) * (static_cast<double>(b.x) * c.y - static_cast<double>(b.y) * c.x)) / 6.0;
    }
    EXPECT_NEAR(std::abs(vol), 8.0, 1e-4) << "the bite changed the box's volume";
}

// The imprint runs to a fixpoint and re-offers every tool surface on every pass, so a bite
// that does not consume its own precondition grows without bound. Both sub-faces carry the
// arc afterwards, and neither may be bitten again.
TEST(ArcBiteSeam, BitingIsIdempotent)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = offsetCylinder(0.5f, 0.7f);
    ASSERT_TRUE(imprintMutually(box, cyl));
    const size_t v1 = box.vertexCount(), e1 = box.edgeCount(), f1 = box.faceCount();

    Body again = box;
    Body tool = cyl;
    ASSERT_TRUE(imprintMutually(again, tool));
    EXPECT_EQ(again.vertexCount(), v1) << "re-imprinting added vertices";
    EXPECT_EQ(again.edgeCount(), e1) << "re-imprinting added edges";
    EXPECT_EQ(again.faceCount(), f1) << "re-imprinting added faces";
    EXPECT_TRUE(again.checkIntegrity().ok) << again.checkIntegrity().reason;
}

// A circle merely TANGENT to the boundary touches it at one point and cuts nothing. That
// must remain a no-op on the bitten face rather than a degenerate split.
TEST(ArcBiteSeam, TangentCircleDoesNotBite)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = offsetCylinder(0.5f, 0.5f);  // footprint reaches x=1 exactly
    ASSERT_TRUE(imprintMutually(box, cyl));
    EXPECT_EQ(arcEdgeCount(box), 0u) << "a tangency produced an arc edge";
    EXPECT_TRUE(box.isClosed());
    EXPECT_TRUE(box.checkIntegrity().ok) << box.checkIntegrity().reason;
    EXPECT_TRUE(box.checkGeometry().ok) << box.checkGeometry().reason;
}

// ── (2) PLANE PARALLEL TO A CYLINDER'S AXIS ──────────────────────────────────────
TEST(ArcBiteSeam, PlaneParallelToTheAxisCutsTwoGeneratrices)
{
    const Surface cyl = cylinderAt({0.7f, 0.f, 0.f}, {0.f, 0.f, 1.f}, 0.5f);

    // Cuts it: axis at x=0.7, radius 0.5, so the plane x=1 is 0.3 from the axis and the
    // generatrices sit at y = ±sqrt(0.25 − 0.09) = ±0.4.
    const SurfaceIntersection two = intersectSurfaces(planeAt({1.f, 0.f, 0.f}, {1.f, 0.f, 0.f}), cyl);
    ASSERT_EQ(two.kind, SurfaceIntersectionKind::TwoLines);
    EXPECT_NEAR(std::fabs(two.curve.origin.y), 0.4f, 1e-5f);
    EXPECT_NEAR(std::fabs(two.curve2.origin.y), 0.4f, 1e-5f);
    EXPECT_NEAR(two.curve.origin.y, -two.curve2.origin.y, 1e-5f) << "not a symmetric pair";
    EXPECT_NEAR(two.curve.origin.x, 1.f, 1e-5f) << "a generatrix is off the cutting plane";
    // Both run along the axis.
    EXPECT_NEAR(std::fabs(two.curve.dir.z), 1.f, 1e-5f);
    EXPECT_NEAR(std::fabs(two.curve2.dir.z), 1.f, 1e-5f);

    // Tangent: exactly one generatrix.
    EXPECT_EQ(intersectSurfaces(planeAt({1.2f, 0.f, 0.f}, {1.f, 0.f, 0.f}), cyl).kind,
              SurfaceIntersectionKind::Line);
    // Misses entirely, on both axes.
    EXPECT_EQ(intersectSurfaces(planeAt({1.5f, 0.f, 0.f}, {1.f, 0.f, 0.f}), cyl).kind,
              SurfaceIntersectionKind::None);
    EXPECT_EQ(intersectSurfaces(planeAt({0.f, 1.f, 0.f}, {0.f, 1.f, 0.f}), cyl).kind,
              SurfaceIntersectionKind::None);
    // Perpendicular still gives the latitude circle, and a SKEW plane is still out of scope.
    EXPECT_EQ(intersectSurfaces(planeAt({0.f, 0.f, 1.f}, {0.f, 0.f, 1.f}), cyl).kind,
              SurfaceIntersectionKind::Circle);
    EXPECT_EQ(intersectSurfaces(planeAt({0.f, 0.f, 0.f}, {1.f, 0.f, 1.f}), cyl).kind,
              SurfaceIntersectionKind::Unsupported);
}

TEST(ArcBiteSeam, TheBoxSideWallIsCutByTheGeneratrices)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = offsetCylinder(0.5f, 0.7f);
    ASSERT_TRUE(imprintMutually(box, cyl));

    // Two vertical seams on the +X wall at y = ±0.4, spanning the box's full height.
    size_t onWall = 0;
    for (uint32_t v = 0; v < static_cast<uint32_t>(box.vertexCount()); ++v) {
        if (!box.vertex(v).alive) continue;
        const auto& p = box.vertex(v).point;
        if (std::fabs(p.x - 1.f) < 1e-5f && std::fabs(std::fabs(p.y) - 0.4f) < 1e-4f &&
            std::fabs(std::fabs(p.z) - 1.f) < 1e-5f)
            ++onWall;
    }
    EXPECT_EQ(onWall, 4u) << "the +X wall was not cut at y = ±0.4 top and bottom";
    EXPECT_TRUE(box.isClosed());
    EXPECT_TRUE(box.checkIntegrity().ok) << box.checkIntegrity().reason;
}

// ── (3) A CONCAVE, ARC-BOUNDED FACE IS CLASSIFIED BY ITS MATERIAL ────────────────
// The assertion that matters: the remainder of a bitten face has its material outside the
// cylinder, so it must classify Outside even though its outline average does not.
TEST(ArcBiteSeam, BittenRemainderClassifiesOutsideAndTheLensInside)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = offsetCylinder(0.5f, 0.7f);
    ASSERT_TRUE(imprintMutually(box, cyl));

    size_t inside = 0, outside = 0;
    for (uint32_t f = 0; f < static_cast<uint32_t>(box.faceCount()); ++f) {
        if (!box.face(f).alive) continue;
        (box.classifyFace(f, cyl) == Body::PointContainment::Inside ? inside : outside)++;
    }
    // Exactly three of the box's faces have their material inside the cylinder: the lens
    // bitten from each ±Z face, and the middle strip of the +X wall between the two
    // generatrices. Everything else is outside it.
    EXPECT_EQ(inside, 3u) << "wrong number of box faces classified inside the cylinder";
    EXPECT_EQ(inside + outside, box.faceCount()) << "a face was neither";

    // And the mechanism, pinned without depending on where the centroid happens to land:
    // on every bitten ±Z face whose outline average is NOT on its material, the sample point
    // must be — and must classify differently. Before fix (3) there was no such point and
    // the whole face was called Inside.
    size_t checkedRemainder = 0;
    for (uint32_t f = 0; f < static_cast<uint32_t>(box.faceCount()); ++f) {
        if (!box.face(f).alive) continue;
        const Vec3 c = box.faceCentroid(f);
        if (std::fabs(std::fabs(c.z) - 1.f) > 1e-5f) continue;  // a ±Z face
        if (cyl.classifyPoint(c) != Body::PointContainment::Inside) continue;
        const Vec3 sp = box.faceSamplePoint(f);
        if (sp.x == c.x && sp.y == c.y && sp.z == c.z) continue;  // centroid was fine here
        ++checkedRemainder;
        EXPECT_NE(cyl.classifyPoint(sp), Body::PointContainment::Inside)
            << "face " << f << ": the sample point is not on the face's material";
    }
    EXPECT_GT(checkedRemainder, 0u)
        << "no face had a centroid off its own material — the fixture no longer exercises this";
}

// ── NO REGRESSION ON THE CENTRED CASE ────────────────────────────────────────────
// Phase 4e's cylinder-through-box must still sew, at the same face counts and volumes —
// all three fixes above touch code every boolean runs through.
TEST(ArcBiteSeam, TheCentredCylinderStillSews)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    const Body cyl = makeCylinder(0.5f, 4.f, kSeg);
    struct Case { BooleanOp op; size_t faces; };
    for (const Case& c : {Case{BooleanOp::Union, 4 + 2 + 2 * kSeg + 2},
                          Case{BooleanOp::Intersection, kSeg + 2},
                          Case{BooleanOp::Difference, 4 + 2 + kSeg}}) {
        const Body r = booleanToBody(box, cyl, c.op);
        EXPECT_EQ(r.faceCount(), c.faces);
        EXPECT_TRUE(r.isClosed());
        EXPECT_TRUE(r.checkIntegrity().ok) << r.checkIntegrity().reason;
        EXPECT_TRUE(r.checkGeometry().ok) << r.checkGeometry().reason;
    }
}

// ── (5) THE SEAM IS SHARED, AND THE OFFSET BOOLEAN SEWS ─────────────────────────
// The last thing between the operands was discretization. Cutting a box face along the
// seam circle gives the box ONE arc from entry to exit; the cylinder's rim over the same
// stretch is a chain of facet arcs. Measured at the z = -1 seam: 2 vertices on the box
// against 13 on the cylinder, so 1 edge facing 12 and not one of them able to partner —
// the identical failure the latitude ring had at 8-against-16. Fixed the same way: the
// bite subdivides its arc at the other operand's vertices, and because the mutual imprint
// cuts one operand at a time, a face already segmented along the circle RECONCILES its
// discretization on the next round instead of merely refusing.
TEST(ArcBiteSeam, TheTwoOperandsShareTheSeamVertexForVertex)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = offsetCylinder(0.5f, 0.7f);
    ASSERT_TRUE(imprintMutually(box, cyl));

    // Every point of the seam circle at z = -1 that either operand has, the other must have
    // too. Only the x <= 1 stretch is shared — beyond the wall the seam is not a seam.
    auto seamPoints = [](const Body& b, float z) {
        std::vector<Vec3> out;
        for (uint32_t v = 0; v < static_cast<uint32_t>(b.vertexCount()); ++v) {
            if (!b.vertex(v).alive) continue;
            const Vec3 p = b.vertex(v).point;
            if (std::fabs(p.z - z) > 1e-4f || p.x > 1.f + 1e-4f) continue;
            if (std::fabs(std::hypot(p.x - 0.7f, p.y) - 0.5f) > 1e-3f) continue;
            out.push_back(p);
        }
        return out;
    };
    for (float z : {-1.f, 1.f}) {
        const std::vector<Vec3> bp = seamPoints(box, z), cp = seamPoints(cyl, z);
        EXPECT_GE(bp.size(), 13u) << "z=" << z << ": the box's seam arc was not subdivided";
        EXPECT_EQ(bp.size(), cp.size()) << "z=" << z << ": the two operands disagree on the "
                                           "seam's discretization";
        for (const Vec3& p : bp) {
            float best = 1e30f;
            for (const Vec3& q : cp)
                best = std::min(best, std::hypot(std::hypot(p.x - q.x, p.y - q.y), p.z - q.z));
            EXPECT_LT(best, 1e-5f) << "z=" << z << ": box seam point (" << p.x << "," << p.y
                                   << ") has no counterpart on the cylinder";
        }
    }
}

// And so the boolean produces a solid where it used to produce nothing.
TEST(ArcBiteSeam, OffsetCylinderBooleanSews)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    for (float dx : {0.7f, 1.0f, 1.3f}) {
        const Body cyl = offsetCylinder(0.5f, dx);
        for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
            const Body r = booleanToBody(box, cyl, op);
            ASSERT_GT(r.faceCount(), 0u) << "dx=" << dx << ": still empty";
            EXPECT_TRUE(r.isClosed()) << "dx=" << dx;
            EXPECT_TRUE(r.checkIntegrity().ok) << "dx=" << dx << ": " << r.checkIntegrity().reason;
            EXPECT_TRUE(r.checkGeometry().ok) << "dx=" << dx << ": " << r.checkGeometry().reason;
            EXPECT_EQ(r.checkIntegrity().boundaryEdges, 0u) << "dx=" << dx;
        }
    }
}

// The difference identity is EXACT at every offset and every refinement level: (A-B) and
// (A∩B) tile A to within a hundred-thousandth. That is the strong statement available here
// — it says the seam divides the box's material correctly and nothing is lost or doubled
// on the inside.
TEST(ArcBiteSeam, DifferencePlusIntersectionTilesTheBoxExactly)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    for (float dx : {0.7f, 1.0f, 1.3f}) {
        const Body cyl = offsetCylinder(0.5f, dx);
        const Body I = booleanToBody(box, cyl, BooleanOp::Intersection);
        const Body D = booleanToBody(box, cyl, BooleanOp::Difference);
        ASSERT_GT(I.faceCount(), 0u);
        ASSERT_GT(D.faceCount(), 0u);
        for (uint32_t s : {0u, 1u, 2u, 4u})
            EXPECT_NEAR(signedVolume(D.toMesh(s)) + signedVolume(I.toMesh(s)),
                        signedVolume(box.toMesh(s)), 1e-5)
                << "dx=" << dx << " sub=" << s << ": D + I does not tile the box";
    }
}

// When the seam's endpoints land ON the cylinder's own facet vertices, everything is exact
// — inclusion-exclusion included, and the intersection matches the chord-level arithmetic
// to the last digit. At dx = 1.0 the circle is centred on the wall, so its two crossings
// are at ±90°, which a 16-gon has vertices at.
TEST(ArcBiteSeam, FacetAlignedOffsetIsExactThroughout)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    const Body cyl = offsetCylinder(0.5f, 1.0f);
    const Body U = booleanToBody(box, cyl, BooleanOp::Union);
    const Body I = booleanToBody(box, cyl, BooleanOp::Intersection);
    ASSERT_GT(U.faceCount(), 0u);
    ASSERT_GT(I.faceCount(), 0u);
    for (uint32_t s : {0u, 1u, 2u, 4u})
        EXPECT_NEAR(signedVolume(U.toMesh(s)) + signedVolume(I.toMesh(s)),
                    signedVolume(box.toMesh(s)) + signedVolume(cyl.toMesh(s)), 1e-5)
            << "sub=" << s << ": inclusion-exclusion fails at the facet-aligned offset";
    // Half the 16-gon, two units tall.
    EXPECT_NEAR(signedVolume(I.toMesh(0)), 0.765367, 1e-5);
}

// THE UNION IS EXACT — measured against the ANALYTIC volume rather than a tessellation.
//
// An earlier version of this file claimed the opposite, and the claim was wrong in a way
// worth recording. Inclusion-exclusion measured on toMesh output misses by ~0.0069, and
// that was written up as "the union's account of the cylinder's protruding part" — a cause
// that had been reasoned to, not measured. It is not the boolean. toMesh under-refines the
// INTERIOR of a curved patch (a lone cylinder converges to 3.088 where pi*r^2*h is 3.1416;
// its flat caps refine exactly, its side patches do not), and the two sides of the identity
// carry different amounts of curved surface, so that error does not cancel between them.
//
// Ask the analytic integrator instead and the identity is exact to the last bit at every
// offset that produces a result. Which is the lesson: when an oracle and a subject share a
// known-approximate path, the oracle proves nothing about the subject.
TEST(ArcBiteSeam, InclusionExclusionIsExactOnTheAnalyticVolume)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    for (float dx : {0.7f, 1.0f, 1.3f}) {
        const Body cyl = offsetCylinder(0.5f, dx);
        const Body U = booleanToBody(box, cyl, BooleanOp::Union);
        const Body I = booleanToBody(box, cyl, BooleanOp::Intersection);
        ASSERT_GT(U.faceCount(), 0u) << "dx=" << dx;
        ASSERT_GT(I.faceCount(), 0u) << "dx=" << dx;
        EXPECT_NEAR(U.massProperties().volume + I.massProperties().volume,
                    box.massProperties().volume + cyl.massProperties().volume, 1e-5)
            << "dx=" << dx << ": |A u B| + |A n B| != |A| + |B|";
    }
}

// A DIFFERENCE THROUGH A CURVED SOLID HAS THE RIGHT ANALYTIC VOLUME.
//
// It did not, and the reason is a word meaning two things. Surface::normal is the outward
// normal for a Plane but the AXIS for a cylinder, sphere or cone — the header says so. The
// boolean reversed a kept face by negating that field, which states the truth for a plane
// and, for a cylinder, merely re-parameterizes the surface while reversing nothing. The
// resulting bore carried an axis pointing the wrong way with its orientation flag unset.
//
// Nothing caught it for a long time because the tessellator derives its normal as the axis
// flipped by Face::reversed, so the two errors cancel exactly there — the mesh, and every
// volume measured from it, came out right. The analytic integrator does not cancel them: it
// needs the axis for the parameterization AND the flag for the orientation. A difference
// through a cylinder reported 6.147 where the truth is 6.653.
//
// FaceDef now carries `reversed`, fromFaces copies it onto the face, and the boolean sets it
// for curved surfaces while leaving the planar negation exactly as it was.
TEST(ArcBiteSeam, DifferenceThroughACurvedSolidHasTheRightAnalyticVolume)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    for (float dx : {0.7f, 1.0f, 1.3f}) {
        const Body cyl = offsetCylinder(0.5f, dx);
        const Body I = booleanToBody(box, cyl, BooleanOp::Intersection);
        const Body D = booleanToBody(box, cyl, BooleanOp::Difference);
        ASSERT_GT(I.faceCount(), 0u) << "dx=" << dx;
        ASSERT_GT(D.faceCount(), 0u) << "dx=" << dx;
        EXPECT_NEAR(D.massProperties().volume + I.massProperties().volume,
                    box.massProperties().volume, 1e-5)
            << "dx=" << dx << ": (A-B) + (A n B) != A on the analytic volume";
    }

    // The bore's faces keep the cylinder's own axis and say they are reversed, rather than
    // carrying a flipped axis and claiming not to be. This is the mechanism, asserted
    // directly, because the volume above would also pass if both were flipped together.
    const Body cyl = offsetCylinder(0.5f, 0.7f);
    const Body D = booleanToBody(box, cyl, BooleanOp::Difference);
    size_t curved = 0, reversedCurved = 0;
    for (uint32_t f = 0; f < static_cast<uint32_t>(D.faceCount()); ++f) {
        if (!D.face(f).alive) continue;
        const uint32_t sid = D.face(f).surface;
        if (sid >= D.surfaceCount() || D.surface(sid).kind != SurfaceKind::Cylinder) continue;
        ++curved;
        EXPECT_GT(D.surface(sid).normal.z, 0.f)
            << "face " << f << ": the bore's stored axis was negated instead of the face "
               "being marked reversed";
        if (D.face(f).reversed) ++reversedCurved;
    }
    EXPECT_GT(curved, 0u) << "the difference has no cylindrical faces to check";
    EXPECT_EQ(reversedCurved, curved) << "a cavity wall is not marked reversed";
}

// Planar reversal is untouched — it was correct, it is what every planar boolean in this
// kernel rests on, and the change was scoped to leave it alone.
TEST(ArcBiteSeam, PlanarDifferenceVolumesAreUnchanged)
{
    const Body A = makeBox(2.f, 2.f, 2.f);
    Body B = makeBox(1.f, 1.f, 1.f);
    B.translate({1.f, 1.f, 1.f});  // overlaps A in a 0.5^3 corner
    EXPECT_NEAR(booleanToBody(A, B, BooleanOp::Difference).massProperties().volume, 8.0 - 0.125,
                1e-5);
    EXPECT_NEAR(booleanToBody(A, B, BooleanOp::Intersection).massProperties().volume, 0.125, 1e-5);
    EXPECT_NEAR(booleanToBody(A, B, BooleanOp::Union).massProperties().volume, 8.0 + 1.0 - 0.125,
                1e-5);
    // hollowBox is itself a difference, and predates all of this.
    EXPECT_NEAR(hollowBox(4.f, 4.f, 4.f, 0.5f).massProperties().volume, 64.0 - 27.0, 1e-5);
}

// ── CONTRACTS THE SEAM WORK PUT UNDER STRAIN ────────────────────────────────────
//
// imprintCurve reports kInvalid to mean "this tool curve did not apply here", and the
// mutual imprint's fixpoint reads it as "nothing changed" — it stops iterating when every
// offer comes back kInvalid. Reconciling a seam's discretization (fix 4) modifies the body
// without splitting any face, so for a while it did both: it subdivided and then returned
// kInvalid, which is a fixpoint that can terminate with work still outstanding and a
// caller that has been told the body is untouched. It now returns the face when it changed
// something. This pins the pair of properties that makes that safe.
TEST(ArcBiteSeam, ReconcilingASeamReportsItAndThenSettles)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body cyl = offsetCylinder(0.5f, 0.7f);
    ASSERT_TRUE(imprintMutually(box, cyl));

    // Converged: a further mutual imprint must change nothing at all.
    const size_t v = box.vertexCount(), e = box.edgeCount(), f = box.faceCount();
    const size_t cv = cyl.vertexCount(), ce = cyl.edgeCount(), cf = cyl.faceCount();
    ASSERT_TRUE(imprintMutually(box, cyl));
    EXPECT_EQ(box.vertexCount(), v) << "the imprint had not reached a fixpoint";
    EXPECT_EQ(box.edgeCount(), e);
    EXPECT_EQ(box.faceCount(), f);
    EXPECT_EQ(cyl.vertexCount(), cv);
    EXPECT_EQ(cyl.edgeCount(), ce);
    EXPECT_EQ(cyl.faceCount(), cf);
    EXPECT_TRUE(box.checkIntegrity().ok) << box.checkIntegrity().reason;
    EXPECT_TRUE(cyl.checkIntegrity().ok) << cyl.checkIntegrity().reason;

    // And re-offering the seam circle directly to a seam-bearing face reports kInvalid —
    // having, by then, nothing left to reconcile — without touching the body.
    Curve seam;
    seam.kind = CurveKind::Circle;
    seam.origin = {0.7f, 0.f, 1.f};
    seam.dir = {0.f, 0.f, 1.f};
    seam.ref = {1.f, 0.f, 0.f};
    seam.radius = 0.5f;
    std::vector<Vec3> ring;
    for (uint32_t iv = 0; iv < static_cast<uint32_t>(cyl.vertexCount()); ++iv) {
        if (!cyl.vertex(iv).alive) continue;
        const Vec3 p = cyl.vertex(iv).point;
        if (std::fabs(p.z - 1.f) < 1e-4f) ring.push_back(p);
    }
    size_t offered = 0;
    for (uint32_t fi = 0; fi < static_cast<uint32_t>(box.faceCount()); ++fi) {
        if (!box.face(fi).alive) continue;
        if (std::fabs(box.faceCentroid(fi).z - 1.f) > 1e-4f) continue;
        const size_t vBefore = box.vertexCount(), eBefore = box.edgeCount();
        const uint32_t got = box.imprintCurve(fi, seam, Tolerance{}, &ring);
        ++offered;
        if (got == kInvalid) {
            EXPECT_EQ(box.vertexCount(), vBefore)
                << "face " << fi << ": imprintCurve returned kInvalid but added vertices";
            EXPECT_EQ(box.edgeCount(), eBefore)
                << "face " << fi << ": imprintCurve returned kInvalid but added edges";
        }
    }
    EXPECT_GT(offered, 0u) << "no +Z face was offered the seam circle";
}

// A closed solid must tessellate to a CLOSED mesh, at every refinement level.
//
// This is the invariant the ear-clipper quietly broke. It gives up when it can find no ear,
// and used to emit the remnant only if exactly three vertices were left — otherwise
// dropping it and leaving a hole. Nothing detected that: the Body is still valid, euler is
// still right, the hole is in the MESH. And classifyPoint casts its parity ray against
// exactly this mesh, so a hole flips inside/outside for everything behind it, which is the
// foundation the whole boolean stands on. Concave faces only started reaching that code
// with the arc bite, so this battery covers the bitten bodies and every boolean built from
// them, not just the primitives.
TEST(ArcBiteSeam, EveryClosedBodyTessellatesToAClosedMesh)
{
    std::vector<std::pair<const char*, Body>> bodies;
    bodies.emplace_back("box", makeBox(2.f, 2.f, 2.f));
    bodies.emplace_back("cylinder", makeCylinder(0.5f, 4.f, kSeg));
    bodies.emplace_back("sphere", makeSphere(1.f, 8, 12));
    for (float dx : {0.f, 0.7f, 1.0f, 1.3f}) {
        Body b = makeBox(2.f, 2.f, 2.f);
        Body c = offsetCylinder(0.5f, dx);
        if (!imprintMutually(b, c)) continue;
        bodies.emplace_back("imprinted box", std::move(b));
        bodies.emplace_back("imprinted cylinder", std::move(c));
        const Body box = makeBox(2.f, 2.f, 2.f), cyl = offsetCylinder(0.5f, dx);
        for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
            Body r = booleanToBody(box, cyl, op);
            if (r.faceCount() > 0) bodies.emplace_back("boolean result", std::move(r));
        }
    }

    for (const auto& [name, body] : bodies) {
        if (!body.isClosed()) continue;  // only closed solids owe a closed mesh
        for (uint32_t s : {0u, 1u, 4u}) {
            const Mesh m = body.toMesh(s);
            const auto rep = MeshTopologyValidation::validate(m);
            EXPECT_EQ(rep.boundaryLoops, 0u)
                << name << " at subdivisions " << s
                << ": a closed solid tessellated to a mesh with a hole in it";
        }
    }
    EXPECT_GT(bodies.size(), 10u) << "the battery collapsed — nothing was actually checked";
}

}  // namespace nexus::geometry::brep::testing
