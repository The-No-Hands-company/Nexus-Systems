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
// The offset-cylinder BOOLEAN still returns empty; a fourth layer remains, and it is named
// at the bottom of this file. What is proven here is each of the three fixes, on its own
// terms, and that the centred case did not regress.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/BRepSurfaceIntersect.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <cmath>
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

    // Per bitten face: 3 new vertices (both crossings + the chord midpoint), 4 new edges
    // (two crossing splits, the midpoint split, the arc), 1 new face. Two ±Z faces are
    // bitten, and the +X wall is cut by the two generatrices of fix (2).
    EXPECT_EQ(box.vertexCount(), 8u + 3u * 2u) << "unexpected vertex count after two bites";
    EXPECT_TRUE(box.isClosed()) << "the bite opened the shell";
    EXPECT_EQ(box.checkIntegrity().boundaryEdges, 0u);
    EXPECT_TRUE(box.checkIntegrity().ok) << box.checkIntegrity().reason;
    EXPECT_TRUE(box.checkGeometry().ok) << box.checkGeometry().reason;

    // Each bite contributes ONE arc, shared by the lens and the remainder — so four faces
    // carry an arc and there are two arc edges in total, not four.
    EXPECT_EQ(arcEdgeCount(box), 2u) << "each bite should add exactly one arc edge";

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

    // And the mechanism, pinned directly: the outline average of the remainder face lies
    // inside the cylinder while the sample point does not.
    bool checkedRemainder = false;
    for (uint32_t f = 0; f < static_cast<uint32_t>(box.faceCount()); ++f) {
        if (!box.face(f).alive) continue;
        const Vec3 c = box.faceCentroid(f);
        // The -Z remainder: on the z=-1 plane, and its outline average is the point at
        // roughly (1/3, 0) that used to defeat the classifier.
        if (std::fabs(c.z + 1.f) > 1e-5f || std::fabs(c.x - 0.3333f) > 0.02f) continue;
        checkedRemainder = true;
        EXPECT_EQ(cyl.classifyPoint(c), Body::PointContainment::Inside)
            << "fixture drifted: the outline average was expected inside the cylinder";
        EXPECT_NE(cyl.classifyPoint(box.faceSamplePoint(f)), Body::PointContainment::Inside)
            << "the sample point is not on the face's material";
    }
    EXPECT_TRUE(checkedRemainder) << "the remainder face fixture was not found";
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

// CHARACTERIZATION, and the handoff. Both operands of an offset cylinder are now cut
// correctly, valid and closed, and every face is classified by its own material — but the
// boolean still returns empty, so it stays under the watertight-or-empty contract rather
// than returning something leaky.
//
// The remaining cause is located: the two operands do not share the VERTICAL seam. After
// the imprint the offered union face set has 30 one-sided edges and zero reused directed
// edges — nothing is non-manifold, pieces are simply missing their partners — and the
// survivors say where: the box's vertical seams at (1, ±0.4, ±1) against cylinder facet
// vertices such as (1.054, 0.354, ±1), which sits at the cylinder's own radius rather than
// on the x = 1 plane. The box's +X wall is cut along the two generatrices; the cylinder's
// side faces are not cut there at all.
//
// The mechanism is known exactly, and it is NOT the coordination problem the latitude ring
// had. A cylindrical side face is bounded by two rim ARCS and two uprights; the generatrix
// to be imprinted is PARALLEL to the uprights, so its only possible crossings are on the
// arcs — and the Line-imprint path tests only boundary edges whose curve is a Line, so it
// finds no crossings and refuses. Nothing about vertex coordination is involved: once the
// cylinder IS cut, the crossings land at (1, ±0.4, ±1) by construction, which is already
// exactly where the box's are, so the weld pairs them with no protocol at all.
//
// An attempt at this was made and REVERTED, and the reason is the useful part. Teaching the
// Line path to cross an arc (solve the line against the arc's plane, confirm the hit is on
// the circle, read off its parameter) does cut the cylinder — measured, it gained the 12
// seam-plane vertices including all four at (1, ±0.4, ±1), and both operands stayed valid
// and closed. But it also produced four ZERO-AREA faces on the cylinder, each with all its
// vertices on one rim, joining a point on the +0.4 generatrix straight across to one on the
// −0.4 generatrix. Some face is being cut between two crossings that lie on the SAME rim
// rather than one on each. checkIntegrity, checkGeometry, isClosed and euler all pass on
// that body — none of them measures area — and the corruption only showed up as two of the
// box's faces flipping to OnBoundary and two reused directed edges appearing in the offered
// set. So the next attempt needs, in addition to the arc crossing: a guard that the two
// crossings of a generatrix lie on OPPOSITE rims, and an area assertion on the imprinted
// cylinder, because that is the only invariant here with any power.
TEST(ArcBiteSeam, OffsetCylinderBooleanStillBailsToEmptyButCleanly)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    for (float dx : {0.7f, 1.0f, 1.3f}) {
        const Body cyl = offsetCylinder(0.5f, dx);
        for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
            const Body r = booleanToBody(box, cyl, op);
            // Watertight-or-empty holds either way; this is a characterization, so when the
            // shared vertical seam lands it should FAIL and be replaced by the real
            // assertions (closed + the inclusion-exclusion volume identity).
            EXPECT_TRUE(r.faceCount() == 0u || (r.isClosed() && r.checkIntegrity().ok))
                << "dx=" << dx << ": neither empty nor a valid closed solid";
            EXPECT_EQ(r.faceCount(), 0u)
                << "dx=" << dx << ": the offset-cylinder boolean now produces output — the "
                   "shared vertical seam has landed; replace this characterization with a "
                   "watertight + volume-identity assertion";
        }
    }
}

}  // namespace nexus::geometry::brep::testing
