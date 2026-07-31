// A circle can cross a face's boundary more than twice, and until now that face was
// simply not cut.
//
// imprintCurve handled exactly two crossings — one arc bite — plus the special case of
// both landing on a single edge. Anything else was refused, which sounds conservative and
// is not: a refused imprint leaves the face STRADDLING the other solid, which is the one
// state the imprint exists to eliminate. The face is then classified as a whole, from a
// single sample point, and every part of it belonging on the other side of the circle is
// lost with it.
//
// The configuration that produces it is ordinary. Push a sphere off-centre through a box
// and the face it exits is cut in a circle LARGER than that face's inradius but smaller
// than its half-diagonal — so the circle leaves and re-enters through all four edges.
// Eight crossings; four arcs inside the face, one per corner, alternating with four
// outside. Measured on box(2³) against sphere(r1.2) offset 0.5, the +X face came out as
// ONE 16-vertex face spanning the whole plane, classified Inside from its centre at
// (1,0,0) and dropped entire. Twenty-four boundary edges were left with one face instead
// of two and all three operators returned empty.
//
// Concentric box/sphere was unaffected only because there the circle is smaller than the
// face and takes the fully-interior path, which was already implemented.
//
// All the cuts are made in ONE call rather than one per driver pass, because a face that
// has been cut once carries the arc on its boundary, and the already-segmented guard —
// which is right to stop a face being bitten along its own rim — would refuse the rest.

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

// Faces of `b` lying wholly in the plane x == px.
std::vector<uint32_t> facesOnPlaneX(const Body& b, double px)
{
    std::vector<uint32_t> out;
    for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
        if (!b.face(f).alive) continue;
        const std::vector<uint32_t> vs = b.faceVertices(f);
        if (vs.size() < 3) continue;
        bool all = true;
        for (const uint32_t v : vs)
            if (std::abs(b.vertex(v).point.x - px) > 1e-9) { all = false; break; }
        if (all) out.push_back(f);
    }
    return out;
}

}  // namespace

// THE headline. The exit face must come out SEGMENTED — four corner pieces outside the
// sphere and one middle piece inside — not as a single straddling face.
TEST(BRepMultiCrossingBite, EightCrossingFaceIsSegmentedNotLeftStraddling)
{
    Body box = makeBox(2.f, 2.f, 2.f);
    Body sph = sphereAt(1.2, 0.5);
    ASSERT_TRUE(imprintMutually(box, sph));

    const std::vector<uint32_t> exit = facesOnPlaneX(box, 1.0);
    ASSERT_EQ(exit.size(), 5u)
        << "the +X face should be cut into four corner pieces and one middle piece; got "
        << exit.size() << " face(s) on that plane";

    // and each piece must be wholly on one side of the sphere, which is what "no longer
    // straddling" means. The oracle is closed form: the sphere is |p - (0.5,0,0)| < 1.2.
    int inside = 0, outside = 0;
    for (const uint32_t f : exit) {
        const Vec3 s = box.faceSamplePoint(f);
        const double dx = s.x - 0.5;
        const double r = std::sqrt(dx * dx + s.y * s.y + s.z * s.z);
        const auto cls = box.classifyFace(f, sph);
        if (r < 1.2) {
            ++inside;
            EXPECT_EQ(cls, Body::PointContainment::Inside) << "face " << f;
        } else {
            ++outside;
            EXPECT_EQ(cls, Body::PointContainment::Outside) << "face " << f;
        }
    }
    EXPECT_EQ(outside, 4) << "four corner pieces lie outside the sphere";
    EXPECT_EQ(inside, 1) << "one middle piece lies inside it";

    EXPECT_TRUE(box.isClosed());
    EXPECT_TRUE(box.checkIntegrity().ok);
    EXPECT_TRUE(box.checkGeometry().ok);
}

// The Boolean the segmentation unblocks. Correctness is inclusion-exclusion against the
// MUTUALLY IMPRINTED operands — the result carries the seam vertices, the pristine
// operands do not.
TEST(BRepMultiCrossingBite, OffsetBoxSphereBooleansAreWatertightAndConserveVolume)
{
    struct Case { double r, dx; };
    const Case cases[] = {{1.2, 0.5}, {1.2, 0.9}, {0.8, 0.5}, {0.8, 0.7}, {0.8, 0.9}};

    for (const Case& c : cases) {
        const Body A = makeBox(2.f, 2.f, 2.f);
        const Body B = sphereAt(c.r, c.dx);
        const Body U = booleanToBody(A, B, BooleanOp::Union);
        const Body I = booleanToBody(A, B, BooleanOp::Intersection);

        ASSERT_GT(U.faceCount(), 0u) << "R=" << c.r << " dx=" << c.dx << " union empty";
        ASSERT_GT(I.faceCount(), 0u) << "R=" << c.r << " dx=" << c.dx << " intersection empty";
        for (const Body* r : {&U, &I}) {
            EXPECT_TRUE(r->isClosed()) << "R=" << c.r << " dx=" << c.dx;
            EXPECT_TRUE(r->checkIntegrity().ok) << "R=" << c.r << " dx=" << c.dx;
            EXPECT_TRUE(r->checkGeometry().ok) << "R=" << c.r << " dx=" << c.dx;
        }

        Body Ai = A, Bi = B;
        ASSERT_TRUE(imprintMutually(Ai, Bi));
        for (const uint32_t sub : {0u, 2u}) {
            const double lhs = meshVolume(U, sub) + meshVolume(I, sub);
            const double rhs = meshVolume(Ai, sub) + meshVolume(Bi, sub);
            EXPECT_NEAR(lhs, rhs, 1e-5 * rhs)
                << "R=" << c.r << " dx=" << c.dx << " sub" << sub;
        }
    }
}

// The offset cylinder through a box — the other pair that the curved-boolean baseline
// pinned as empty. Its side wall is cut by the box exactly the same way.
TEST(BRepMultiCrossingBite, OffsetCylinderThroughBoxSews)
{
    const Body A = makeBox(2.f, 2.f, 2.f);
    Body C = makeCylinder(1.f, 4.f, 16);
    C.translate({0.5, 0., 0.});

    const Body U = booleanToBody(A, C, BooleanOp::Union);
    const Body I = booleanToBody(A, C, BooleanOp::Intersection);
    ASSERT_GT(U.faceCount(), 0u) << "offset box/cylinder union is empty again";
    ASSERT_GT(I.faceCount(), 0u);
    for (const Body* r : {&U, &I}) {
        EXPECT_TRUE(r->isClosed());
        EXPECT_TRUE(r->checkIntegrity().ok);
        EXPECT_TRUE(r->checkGeometry().ok);
    }

    Body Ai = A, Ci = C;
    ASSERT_TRUE(imprintMutually(Ai, Ci));
    for (const uint32_t sub : {0u, 2u}) {
        const double lhs = meshVolume(U, sub) + meshVolume(I, sub);
        const double rhs = meshVolume(Ai, sub) + meshVolume(Ci, sub);
        EXPECT_NEAR(lhs, rhs, 1e-5 * rhs) << "sub" << sub;
    }
}

// A regression guard on the paths this did NOT change: a circle smaller than the face it
// lands on still takes the fully-interior route, and the concentric answers are unmoved.
TEST(BRepMultiCrossingBite, InteriorCircleAndConcentricAnswersAreUnchanged)
{
    const Body A = makeBox(2.f, 2.f, 2.f);
    const Body B = makeSphere(1.2f, 8, 12);
    EXPECT_EQ(booleanToBody(A, B, BooleanOp::Union).faceCount(), 102u);
    EXPECT_EQ(booleanToBody(A, B, BooleanOp::Intersection).faceCount(), 78u);
    EXPECT_EQ(booleanToBody(A, B, BooleanOp::Difference).faceCount(), 78u);

    // sphere strictly inside: the answers can be named rather than measured
    const Body S = makeSphere(0.8f, 8, 12);
    EXPECT_EQ(booleanToBody(A, S, BooleanOp::Union).faceCount(), A.faceCount());
    EXPECT_EQ(booleanToBody(A, S, BooleanOp::Intersection).faceCount(), S.faceCount());
}

// CHARACTERIZATION, not an endorsement. Difference does NOT conserve volume on some
// offset configurations: D + I should equal A exactly, and it comes up short by a few
// parts in ten thousand. It is a PRE-EXISTING defect, measured on configurations that
// already sewed before the multi-crossing bite landed (R=0.8 at dx 0.5/0.7/0.9 gave
// -4.06e-4 / -3.39e-4 / -2.03e-4 beforehand); the bite only makes it reachable in more
// places. It is pinned here so it cannot drift unnoticed and so nobody reads the
// watertight assertions above as saying Difference is correct.
//
// The residual is NOT a tessellation artifact — refining does not shrink it. At R=1.2
// dx=0.5 it converges: -2.93e-4, -2.68e-4, -2.63e-4, -2.62e-4, -2.61e-4 across
// subdivisions 0 to 4. A tessellation error would tend to zero; this settles on a
// constant, so a real piece of volume is missing.
TEST(BRepMultiCrossingBite, DifferenceVolumeResidualIsPinnedAsAKnownGap)
{
    const Body A = makeBox(2.f, 2.f, 2.f);
    const Body B = sphereAt(1.2, 0.5);
    const Body I = booleanToBody(A, B, BooleanOp::Intersection);
    const Body D = booleanToBody(A, B, BooleanOp::Difference);
    ASSERT_GT(I.faceCount(), 0u);
    ASSERT_GT(D.faceCount(), 0u);
    EXPECT_TRUE(D.isClosed()) << "whatever the volume, the contract is watertight-or-empty";

    Body Ai = A, Bi = B;
    ASSERT_TRUE(imprintMutually(Ai, Bi));
    const double rel =
        (meshVolume(D, 4) + meshVolume(I, 4) - meshVolume(Ai, 4)) / meshVolume(Ai, 4);

    EXPECT_LT(rel, 0.0) << "the residual has flipped sign — re-measure before trusting it";
    EXPECT_GT(std::abs(rel), 1e-5)
        << "D + I now agrees with A to better than 1e-5 — the gap may be FIXED; verify and "
           "replace this characterization with a real conservation assertion";
    EXPECT_LT(std::abs(rel), 1e-3) << "the difference residual got materially worse: " << rel;
}

}  // namespace nexus::geometry::brep::testing
