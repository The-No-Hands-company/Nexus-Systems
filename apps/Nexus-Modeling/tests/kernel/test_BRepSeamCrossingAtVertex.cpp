// Two seam circles cut by two faces that share an edge MEET on that edge. Not by
// coincidence — necessarily. Both are sections of the same sphere, so their common points
// are exactly where the sphere crosses the shared edge. Whichever face is imprinted first
// splits the edge there, and the second circle then meets that edge precisely AT a vertex.
//
// So a crossing landing exactly on an endpoint is the normal case for a box against a
// sphere, not a degenerate one, and the solver has to count it. The planar
// circle-versus-segment solver required a strictly interior root, which makes the outcome
// depend on which side of the endpoint the arithmetic happens to land.
//
// MEASURED on box(2³) against sphere(r1.2). At offset 0.5 all eight crossings on the exit
// face came back at s = 0.9999999999999999 — a few ulp inside — were accepted, and the
// face was cut into its five pieces. At offset 0.7 the same eight landed a few ulp the
// other side of 1, every boundary edge reported ZERO crossings, and the face was left
// uncut and straddling: ten box faces where fourteen were owed, and all three operators
// empty. The two configurations differ by nothing structural. Only by rounding.
//
// This is the same rule the cylinder's uprights needed and then the sphere's bounding arcs
// needed, arrived at for the third time on the oldest of the three paths. The reason it
// keeps recurring is that each solver was written for the case where the circle cuts
// cleanly through the middle of an edge, and the imprint's own progress is what stops that
// being true — every cut a face makes lands a vertex on some neighbour's boundary.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace nexus::geometry::brep::testing {

namespace {

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

size_t facesOnPlaneX(const Body& b, double px)
{
    size_t n = 0;
    for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
        if (!b.face(f).alive) continue;
        const std::vector<uint32_t> vs = b.faceVertices(f);
        if (vs.size() < 3) continue;
        bool all = true;
        for (const uint32_t v : vs)
            if (std::abs(b.vertex(v).point.x - px) > 1e-9) { all = false; break; }
        if (all) ++n;
    }
    return n;
}

}  // namespace

// THE headline, asserted as a face count because that is what the miss produced: the exit
// face segments into five pieces whether the crossings land just inside its boundary
// vertices or just outside them.
TEST(BRepSeamCrossingAtVertex, ExitFaceSegmentsRegardlessOfWhichSideTheRootLandsOn)
{
    // both offsets put the exit circle between the face's inradius (1) and its
    // half-diagonal (1.414), so both cross all four edges — 1.0909 and 1.1619
    for (const double dx : {0.5, 0.7}) {
        Body box = makeBox(2.f, 2.f, 2.f);
        Body sph = sphereAt(1.2, dx);
        ASSERT_TRUE(imprintMutually(box, sph));
        EXPECT_EQ(facesOnPlaneX(box, 1.0), 5u)
            << "dx=" << dx
            << ": the exit face should be four corner pieces and one middle piece";
        EXPECT_EQ(box.faceCount(), 14u)
            << "dx=" << dx << ": 4 corners + 1 middle on +X, 2 on each of four sides, "
                              "1 untouched -X";
        EXPECT_TRUE(box.isClosed());
        EXPECT_TRUE(box.checkIntegrity().ok);
        EXPECT_TRUE(box.checkGeometry().ok);
    }
}

// The crossing that has to count is not an edge case of the fixture: it is forced by the
// geometry. Two box faces sharing an edge cut two seam circles that necessarily meet ON
// that edge, so after the first face is imprinted the second circle meets a VERTEX.
TEST(BRepSeamCrossingAtVertex, AdjacentSeamsShareTheirCrossingOnTheCommonEdge)
{
    const double R = 1.2, dx = 0.7;
    // where the sphere meets the box edge x = 1, y = 1: (1-dx)^2 + 1 + z^2 = R^2
    const double z2 = R * R - (1.0 - dx) * (1.0 - dx) - 1.0;
    ASSERT_GT(z2, 0.0) << "fixture must actually reach that edge";
    const double z = std::sqrt(z2);

    Body box = makeBox(2.f, 2.f, 2.f);
    Body sph = sphereAt(R, dx);
    ASSERT_TRUE(imprintMutually(box, sph));

    // the box must carry a vertex exactly there — it is where both seams cross
    int found = 0;
    for (uint32_t v = 0; v < static_cast<uint32_t>(box.vertexCount()); ++v) {
        if (!box.vertex(v).alive) continue;
        const Vec3 p = box.vertex(v).point;
        if (std::abs(p.x - 1.0) < 1e-9 && std::abs(std::abs(p.y) - 1.0) < 1e-9 &&
            std::abs(std::abs(p.z) - z) < 1e-6)
            ++found;
    }
    EXPECT_EQ(found, 4) << "expected the four (x=1, y=+-1, z=+-" << z
                        << ") points where the two seams meet on the shared edges";
}

// What the segmentation unblocks, with both conservation identities — U+I == A+B and
// D+I == A — since a watertight result can still be the wrong size.
TEST(BRepSeamCrossingAtVertex, OffsetBoxSphereConservesVolumeAtBothOffsets)
{
    for (const double dx : {0.5, 0.7, 0.9}) {
        const Body A = makeBox(2.f, 2.f, 2.f);
        const Body B = sphereAt(1.2, dx);
        const Body U = booleanToBody(A, B, BooleanOp::Union);
        const Body I = booleanToBody(A, B, BooleanOp::Intersection);
        const Body D = booleanToBody(A, B, BooleanOp::Difference);
        ASSERT_GT(U.faceCount(), 0u) << "dx=" << dx << " union empty";
        ASSERT_GT(I.faceCount(), 0u) << "dx=" << dx << " intersection empty";
        ASSERT_GT(D.faceCount(), 0u) << "dx=" << dx << " difference empty";
        for (const Body* r : {&U, &I, &D}) {
            EXPECT_TRUE(r->isClosed()) << "dx=" << dx;
            EXPECT_TRUE(r->checkIntegrity().ok) << "dx=" << dx;
            EXPECT_TRUE(r->checkGeometry().ok) << "dx=" << dx;
        }

        Body Ai = A, Bi = B;
        ASSERT_TRUE(imprintMutually(Ai, Bi));
        for (const uint32_t sub : {0u, 2u}) {
            EXPECT_NEAR(meshVolume(U, sub) + meshVolume(I, sub),
                        meshVolume(Ai, sub) + meshVolume(Bi, sub),
                        1e-6 * (meshVolume(Ai, sub) + meshVolume(Bi, sub)))
                << "dx=" << dx << " sub" << sub << ": U+I != A+B";
            EXPECT_NEAR(meshVolume(D, sub) + meshVolume(I, sub), meshVolume(Ai, sub),
                        1e-6 * meshVolume(Ai, sub))
                << "dx=" << dx << " sub" << sub << ": D+I != A";
        }
    }
}

// The slack must not invent crossings where the circle genuinely misses the edge, nor
// double-count one that lands on an endpoint from both roots.
TEST(BRepSeamCrossingAtVertex, ClearMissesAndTangenciesAreStillNotCut)
{
    // a sphere strictly inside the box touches no face at all
    {
        Body box = makeBox(2.f, 2.f, 2.f);
        Body sph = sphereAt(0.5, 0.0);
        ASSERT_TRUE(imprintMutually(box, sph));
        EXPECT_EQ(box.faceCount(), 6u) << "an enclosed sphere must not cut the box";
        EXPECT_EQ(sph.faceCount(), 96u) << "nor the box the sphere";
    }
    // an exactly tangent sphere touches one face at a single point — a measure-zero
    // contact, which must not be treated as a cut
    {
        Body box = makeBox(2.f, 2.f, 2.f);
        Body sph = sphereAt(0.8, 0.2);  // reaches x = 1.0 exactly
        ASSERT_TRUE(imprintMutually(box, sph));
        EXPECT_EQ(box.faceCount(), 6u) << "a tangency is not a cut";
        EXPECT_TRUE(box.isClosed());
        EXPECT_TRUE(box.checkIntegrity().ok);
    }
}

// A concentric sphere is the configuration where every seam is fully interior to its face,
// so no crossing arises at all. Its answers are pinned exactly and must not move.
TEST(BRepSeamCrossingAtVertex, InteriorSeamConfigurationsAreUnchanged)
{
    const Body A = makeBox(2.f, 2.f, 2.f);
    const Body B = makeSphere(1.2f, 8, 12);
    EXPECT_EQ(booleanToBody(A, B, BooleanOp::Union).faceCount(), 102u);
    EXPECT_EQ(booleanToBody(A, B, BooleanOp::Intersection).faceCount(), 78u);
    EXPECT_EQ(booleanToBody(A, B, BooleanOp::Difference).faceCount(), 78u);
}

}  // namespace nexus::geometry::brep::testing
