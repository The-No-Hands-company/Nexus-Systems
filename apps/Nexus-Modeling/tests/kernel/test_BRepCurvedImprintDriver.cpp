// Phase 3 of the true-analytic-curved-boolean arc — DRIVER WIRING.
//
// imprintOneWay offered the imprint only the Line branch of a surface/surface
// intersection and dropped Circle and TwoLines on the floor, so a curved operand was
// never cut however capable the imprint became. It now offers every branch
// intersectSurfaces can express, letting imprintCurve be the authority on whether a
// given curve applies to a given face.
//
// Wiring it exposed two defects that could not fire while the seams were being
// discarded, and both are guarded here:
//
//  1. The fully-interior-circle (inner loop / hole) imprint was NOT IDEMPOTENT. Unlike
//     the arc-bite path, which consumes its precondition by splitting the face, a hole
//     leaves the circle just as interior as it found it — so the fixpoint driver, which
//     re-offers every tool surface on every pass, appended the same ring forever. Its
//     explosion guard watched the FACE count, which a hole does not change. Measured:
//     1,599,992 vertices on a six-face box against a sixteen-segment cylinder, stopped
//     only by the iteration cap.
//
//  2. The latitude/vertical-edge crossing was ILL-CONDITIONED. A latitude circle meets
//     an axis-parallel cylinder edge at the single axial level where that edge — sitting
//     at exactly the cylinder's radius along its whole length — touches the circle, so
//     |p(s) − centre| = r has a DOUBLE root, whose float solution carries √ε error.
//     Measured 1.22e-4 on a unit-scale cylinder: the split vertex landed at z = −0.9999
//     for a circle lying at z = −1, and checkGeometry failed on the arc through it.
//     Solved as an axial level set instead, which is linear and exact.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepSurfaceIntersect.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {


namespace {

// The clean cylinder-through-box: the cylinder is strictly inside the box's XY
// footprint, so it pierces only the ±Z faces and every seam is a latitude circle.
Body throughBox() { return makeBox(2.f, 2.f, 2.f); }
Body throughCylinder() { return makeCylinder(0.5f, 4.f, 16); }

size_t liveFaces(const Body& b)
{
    size_t n = 0;
    for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f)
        if (b.face(f).alive) ++n;
    return n;
}

// Worst endpoint error over every edge: how far each edge's curve, at its stored
// parameter range, misses the vertex it is supposed to meet.
float worstEndpointError(const Body& b)
{
    double worst = 0.f;
    for (uint32_t e = 0; e < static_cast<uint32_t>(b.edgeCount()); ++e) {
        if (!b.edge(e).alive) continue;
        const Edge& ed = b.edge(e);
        if (ed.curve == kInvalid) continue;
        const Curve& c = b.curve(ed.curve);
        const Vec3 d0 = c.eval(ed.t0) - b.vertex(ed.v0).point;
        const Vec3 d1 = c.eval(ed.t1) - b.vertex(ed.v1).point;
        worst = std::max(worst, std::sqrt(d0.dot(d0)));
        worst = std::max(worst, std::sqrt(d1.dot(d1)));
    }
    return worst;
}

}  // namespace

// THE Phase 3 assertion: both operands come out imprinted. Before the wiring the
// cylinder's 16 curved faces received none of their 16 latitude seams.
TEST(BRepCurvedImprintDriver, BothOperandsOfACylinderThroughBoxAreImprinted)
{
    Body A = throughBox(), B = throughCylinder();
    const size_t fa = liveFaces(A), fb = liveFaces(B);

    ASSERT_TRUE(imprintMutually(A, B)) << "the imprint blew its budget or failed to converge";

    EXPECT_GT(liveFaces(B), fb) << "the cylinder's curved faces were not cut by the box's planes";
    EXPECT_GE(liveFaces(A), fa);
    EXPECT_GT(A.vertexCount(), throughBox().vertexCount())
        << "the box's planar faces were not cut by the cylinder";
}

// Both imprinted operands must remain valid bodies — this is what defect (2) broke.
TEST(BRepCurvedImprintDriver, ImprintedOperandsStayValid)
{
    Body A = throughBox(), B = throughCylinder();
    ASSERT_TRUE(imprintMutually(A, B));

    EXPECT_TRUE(A.checkIntegrity().ok) << "box: " << A.checkIntegrity().reason;
    EXPECT_TRUE(A.checkGeometry().ok) << "box: " << A.checkGeometry().reason;
    EXPECT_TRUE(B.checkIntegrity().ok) << "cyl: " << B.checkIntegrity().reason;
    EXPECT_TRUE(B.checkGeometry().ok) << "cyl: " << B.checkGeometry().reason;
    EXPECT_TRUE(B.isClosed()) << "cutting a closed cylinder's faces must leave it closed";
}

// Conditioning guard for defect (2): the ill-conditioned quadratic gave 1.22e-4, which
// is a thousand times the coincidence tolerance. The axial level set is exact, so the
// bound here is far tighter than checkGeometry's own tolerance would enforce.
TEST(BRepCurvedImprintDriver, LatitudeCrossingsAreExactlyConditioned)
{
    Body A = throughBox(), B = throughCylinder();
    ASSERT_TRUE(imprintMutually(A, B));

    EXPECT_LT(worstEndpointError(B), 1e-6f)
        << "a curve misses its own endpoint vertex by more than float noise — the "
           "latitude crossing has regressed to the double-root quadratic";
    EXPECT_LT(worstEndpointError(A), 1e-6f);
}

// Idempotence guard for defect (1): imprinting the same interior circle twice must add
// geometry exactly once. Without this the driver's fixpoint loop never terminates.
TEST(BRepCurvedImprintDriver, InteriorCircleHoleImprintIsIdempotent)
{
    Body b = makeBox(2.f, 2.f, 2.f);
    uint32_t planar = kInvalid;
    for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
        const uint32_t s = b.face(f).surface;
        if (s < b.surfaceCount() && b.surface(s).kind == SurfaceKind::Plane) { planar = f; break; }
    }
    ASSERT_NE(planar, kInvalid);

    Curve c;
    c.kind = CurveKind::Circle;
    c.dir = b.surface(b.face(planar).surface).normal;
    c.ref = {1.f, 0.f, 0.f};
    c.radius = 0.4f;
    c.origin = b.faceCentroid(planar);

    ASSERT_NE(b.imprintCurve(planar, c), kInvalid) << "a fully interior circle should cut a hole";
    const size_t v1 = b.vertexCount(), e1 = b.edgeCount(), l1 = b.loopCount();

    for (int repeat = 0; repeat < 5; ++repeat)
        EXPECT_EQ(b.imprintCurve(planar, c), kInvalid) << "repeat " << repeat << " re-cut the hole";

    EXPECT_EQ(b.vertexCount(), v1) << "a repeated hole imprint grew the body";
    EXPECT_EQ(b.edgeCount(), e1);
    EXPECT_EQ(b.loopCount(), l1);
    EXPECT_TRUE(b.checkIntegrity().ok) << b.checkIntegrity().reason;
    EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;
}

// Termination + bounded work on the curved inputs the wiring newly reaches. The
// entity budget is what makes this a guarantee rather than an observation.
TEST(BRepCurvedImprintDriver, ImprintTerminatesAndStaysBoundedOnCurvedInputs)
{
    struct Pair { Body a, b; };
    Pair pairs[] = {
        {makeBox(2.f, 2.f, 2.f), makeCylinder(0.5f, 4.f, 16)},
        {makeBox(2.f, 2.f, 2.f), makeSphere(1.2f, 8, 12)},
        {makeCylinder(1.f, 2.f, 12), makeSphere(0.8f, 6, 10)},
        {makeBox(2.f, 2.f, 2.f), makeSphere(1.6f, 16, 24)},  // ~360 curved faces
    };
    for (Pair& p : pairs) {
        const size_t before = p.a.vertexCount() + p.b.vertexCount();
        (void)imprintMutually(p.a, p.b);  // true or a clean budget bail, never a hang
        // Whatever happened, the result is bounded and structurally sound. The bound is
        // generous; the point is that it exists, since the unguarded runaway reached
        // 1.6 million vertices on a body of this size.
        EXPECT_LT(p.a.vertexCount() + p.b.vertexCount(), before * 200u + 4096u)
            << "imprint growth is unbounded";
        EXPECT_TRUE(p.a.checkIntegrity().ok) << p.a.checkIntegrity().reason;
        EXPECT_TRUE(p.b.checkIntegrity().ok) << p.b.checkIntegrity().reason;
    }
}

}  // namespace nexus::geometry::brep::testing
