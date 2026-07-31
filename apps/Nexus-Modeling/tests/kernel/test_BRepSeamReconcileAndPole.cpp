// ONE defect, wearing two faces — and the second turned out to be a symptom of the first,
// which is only knowable by measuring after the fix.
//
// On box(2³) against sphere(r0.8) offset 0.3 — the last configuration failing for any
// reason other than tangency — two things were wrong. Six vertices were duplicated on the
// sphere, coincident with existing ones to within 3e-16, so the sew welded each pair, every
// seam edge acquired a third user and fromFaces refused. And one seam arc traced the
// COMPLEMENT: the long way round the circle, 5.3716 rad against a true 0.9116.
//
// Both arcs share their endpoints, so the wrong one satisfies checkGeometry,
// checkIntegrity, isClosed and Euler alike. Only the span tells them apart. And a
// complement arc swallows the ring vertices belonging to its neighbours, so the pass that
// reconciles the two operands' discretizations then splits it at each of them — which is
// where the duplicates came from. One cause, two symptoms.
//
// THE CAUSE. The arc selector had already been taught to test both candidates and to break
// a tie on the face's bounding box. That was not enough here, because the containment test
// was not tied — it was confidently WRONG. Containment on a curved face is decided in the
// surface's (u,v) domain and a sphere's parametrisation is singular at ±uAxis; this seam
// sits at 61° of latitude, near enough for the test to invert. Of eight seam arcs, seven
// were decided correctly and one — on the face MIRRORING one that was decided correctly,
// with identical spans — chose the complement.
//
// So the box is now a VETO as well as a tie-break, and that part is logic rather than a
// heuristic: an arc lying inside the face cannot leave the box bounding that face's
// boundary, so a containment verdict the box contradicts cannot be right. Vetoing it turns
// a confidently wrong answer into an undecided one, which the existing tie-break resolves.
// Measured across the eight arcs: the box and the (u,v) test agree on seven, and the box is
// right on the eighth.
//
// A separate guard against splitting where a vertex already exists was written first, and
// REMOVED after the veto landed: re-measuring showed the duplicates were gone without it,
// across all fourteen configurations, so it was doing no work. The duplicate-vertex
// assertion below stays — it is a real invariant and it is how this was found — but nothing
// in the source now claims to enforce it directly.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace nexus::geometry::brep::testing {

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

Body sphereAt(double r, double dx)
{
    Body b = makeSphere(static_cast<float>(r), 8, 12);
    b.translate({dx, 0., 0.});
    return b;
}

double meshVolume(const Body& b, uint32_t sub)
{
    return MeshMassProperties::compute(b.toMesh(sub)).volume;
}

int coincidentVertexPairs(const Body& b, double band)
{
    int pairs = 0;
    for (uint32_t i = 0; i < static_cast<uint32_t>(b.vertexCount()); ++i) {
        if (!b.vertex(i).alive) continue;
        for (uint32_t j = i + 1; j < static_cast<uint32_t>(b.vertexCount()); ++j) {
            if (!b.vertex(j).alive) continue;
            const Vec3 p = b.vertex(i).point, q = b.vertex(j).point;
            const double d = std::sqrt((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) +
                                       (p.z - q.z) * (p.z - q.z));
            if (d < band) ++pairs;
        }
    }
    return pairs;
}

// Arcs that go the long way round: same endpoints, complementary span.
int complementArcs(const Body& b)
{
    int n = 0;
    for (uint32_t e = 0; e < static_cast<uint32_t>(b.edgeCount()); ++e) {
        if (!b.edge(e).alive) continue;
        const uint32_t cu = b.edge(e).curve;
        if (cu == kInvalid || b.curve(cu).kind != CurveKind::Circle) continue;
        const Curve& c = b.curve(cu);
        const Vec3 p0 = b.vertex(b.edge(e).v0).point, p1 = b.vertex(b.edge(e).v1).point;
        const Vec3 d0{p0.x - c.origin.x, p0.y - c.origin.y, p0.z - c.origin.z};
        const Vec3 d1{p1.x - c.origin.x, p1.y - c.origin.y, p1.z - c.origin.z};
        const double n0 = std::sqrt(d0.x * d0.x + d0.y * d0.y + d0.z * d0.z);
        const double n1 = std::sqrt(d1.x * d1.x + d1.y * d1.y + d1.z * d1.z);
        if (n0 < 1e-12 || n1 < 1e-12) continue;
        double ca = (d0.x * d1.x + d0.y * d1.y + d0.z * d1.z) / (n0 * n1);
        ca = std::min(1.0, std::max(-1.0, ca));
        const double shortWay = std::acos(ca);
        if (shortWay < 1e-9) continue;
        if (std::abs(std::abs(b.edge(e).t1 - b.edge(e).t0) - (kTwoPi - shortWay)) < 1e-6) ++n;
    }
    return n;
}

}  // namespace

// The reconcile pass must not create a vertex where one already is. Asserted at the WELD
// BAND the sew uses, because that is the distance at which a duplicate stops being
// harmless and starts collapsing two edges into one.
TEST(BRepSeamReconcileAndPole, ImprintCreatesNoDuplicateSeamVertices)
{
    struct Case { double r, dx; };
    const Case cases[] = {{0.8, 0.3}, {0.8, 0.5}, {1.2, 0.1}, {1.2, 0.3}, {1.2, 0.5}};
    for (const Case& c : cases) {
        Body box = makeBox(2.f, 2.f, 2.f);
        Body sph = sphereAt(c.r, c.dx);
        ASSERT_TRUE(imprintMutually(box, sph));
        EXPECT_EQ(coincidentVertexPairs(sph, 1e-4), 0)
            << "R=" << c.r << " dx=" << c.dx
            << ": the sphere carries duplicate vertices inside the sew's weld band";
        EXPECT_EQ(coincidentVertexPairs(box, 1e-4), 0) << "R=" << c.r << " dx=" << c.dx;
    }
}

// No arc may go the long way round. This has to be asserted by SPAN: the complement meets
// the same endpoints, so every validator this kernel owns accepts it.
TEST(BRepSeamReconcileAndPole, NoSeamArcTracesTheComplementNearTheParametricPole)
{
    struct Case { double r, dx; };
    // r=0.8 dx=0.3 puts the seam at 61 degrees of latitude, near the sphere's ±uAxis
    // singularity — the configuration that inverted the containment test.
    const Case cases[] = {{0.8, 0.3}, {0.8, 0.5}, {0.8, 0.7}, {1.2, 0.1}, {1.2, 0.5}};
    for (const Case& c : cases) {
        Body box = makeBox(2.f, 2.f, 2.f);
        Body sph = sphereAt(c.r, c.dx);
        ASSERT_TRUE(imprintMutually(box, sph));
        EXPECT_EQ(complementArcs(sph), 0)
            << "R=" << c.r << " dx=" << c.dx << ": a seam arc traces the long way round";
        EXPECT_EQ(complementArcs(box), 0) << "R=" << c.r << " dx=" << c.dx;
        EXPECT_TRUE(sph.isClosed());
        EXPECT_TRUE(sph.checkGeometry().ok);
    }
}

// The configuration both defects were hiding in. It was the last box/sphere pair failing
// for any reason other than tangency.
TEST(BRepSeamReconcileAndPole, TheLastNonTangentConfigurationSewsAndConservesVolume)
{
    const Body A = makeBox(2.f, 2.f, 2.f);
    const Body B = sphereAt(0.8, 0.3);
    const Body U = booleanToBody(A, B, BooleanOp::Union);
    const Body I = booleanToBody(A, B, BooleanOp::Intersection);
    const Body D = booleanToBody(A, B, BooleanOp::Difference);

    ASSERT_GT(U.faceCount(), 0u) << "union empty";
    ASSERT_GT(I.faceCount(), 0u) << "intersection empty";
    ASSERT_GT(D.faceCount(), 0u) << "difference empty";
    for (const Body* r : {&U, &I, &D}) {
        EXPECT_TRUE(r->isClosed());
        EXPECT_TRUE(r->checkIntegrity().ok);
        EXPECT_TRUE(r->checkGeometry().ok);
    }

    Body Ai = A, Bi = B;
    ASSERT_TRUE(imprintMutually(Ai, Bi));
    for (const uint32_t sub : {0u, 2u, 4u}) {
        EXPECT_NEAR(meshVolume(U, sub) + meshVolume(I, sub),
                    meshVolume(Ai, sub) + meshVolume(Bi, sub),
                    1e-6 * (meshVolume(Ai, sub) + meshVolume(Bi, sub)))
            << "sub" << sub << ": U+I != A+B";
        EXPECT_NEAR(meshVolume(D, sub) + meshVolume(I, sub), meshVolume(Ai, sub),
                    1e-6 * meshVolume(Ai, sub))
            << "sub" << sub << ": D+I != A";
    }
}

// Every box/sphere pair that is not an exact tangency now sews, for all three operators.
// Stated as a sweep rather than a list, so a configuration that stops working is caught
// even if nobody thought to name it.
TEST(BRepSeamReconcileAndPole, EveryNonTangentBoxSphereConfigurationSews)
{
    int tested = 0;
    for (const double r : {0.8, 1.2}) {
        for (const double dx : {0.0, 0.1, 0.3, 0.5, 0.7, 0.9}) {
            // skip exact tangencies: the true result there is not a manifold solid, and a
            // clean empty body is the contract rather than a failure
            const bool tangent = std::abs(dx + r - 1.0) < 1e-9 || std::abs(dx - r + 1.0) < 1e-9;
            if (tangent) continue;
            const Body A = makeBox(2.f, 2.f, 2.f);
            const Body B = sphereAt(r, dx);
            for (const BooleanOp op :
                 {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
                const Body res = booleanToBody(A, B, op);
                EXPECT_GT(res.faceCount(), 0u)
                    << "R=" << r << " dx=" << dx << " op=" << static_cast<int>(op) << " is empty";
                EXPECT_TRUE(res.faceCount() == 0u || res.isClosed())
                    << "R=" << r << " dx=" << dx << " op=" << static_cast<int>(op);
            }
            ++tested;
        }
    }
    EXPECT_GE(tested, 10) << "the sweep skipped almost everything — check the tangency filter";
}

// PERMANENT: the invariant has to survive the tangencies too, which still return empty.
TEST(BRepSeamReconcileAndPole, TangenciesRemainWatertightOrEmpty)
{
    for (const auto& [r, dx] : {std::pair{0.8, 0.2}, {1.2, 0.2}}) {
        const Body A = makeBox(2.f, 2.f, 2.f);
        const Body B = sphereAt(r, dx);
        for (const BooleanOp op :
             {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
            const Body res = booleanToBody(A, B, op);
            EXPECT_TRUE(res.faceCount() == 0u || (res.isClosed() && res.checkIntegrity().ok))
                << "R=" << r << " dx=" << dx << " produced a leaky body";
        }
    }
}

}  // namespace nexus::geometry::brep::testing
