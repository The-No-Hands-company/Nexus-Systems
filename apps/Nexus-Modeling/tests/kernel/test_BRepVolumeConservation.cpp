// Volume is the quantity none of this kernel's invariants weigh.
//
// checkIntegrity, checkGeometry, isClosed and the Euler characteristic are all
// topological. A Boolean result can satisfy every one of them and still enclose the wrong
// amount of space, and for a while Difference did exactly that — watertight, valid, and a
// few parts in ten thousand short.
//
// The identity that catches it is D + I == A. The difference and the intersection
// partition the first operand, so their volumes must add up to it exactly. The shared
// patch — the part of the second operand's surface lying inside the first — appears once
// in each, with opposite orientation, and its contributions have to cancel.
//
// They did not, and the reason was one line in the tessellator. A face was fan-triangulated
// from `ring[0]` — wherever the ring happened to START. For a planar face that is
// harmless: every fan of a planar polygon encloses the same volume. For a CURVED patch it
// is the whole answer, because a four-vertex patch on a sphere is not planar and its two
// diagonals span different surfaces. Difference reverses the vertex ring of every face it
// takes from the second operand, a reversed ring starts at a different vertex, and so the
// same patch was triangulated one way in the intersection and the other way in the
// difference.
//
// MEASURED on box(2³) against sphere(r1.2) offset 0.5, splitting the emitted triangles by
// which surface they lie on: the box triangles summed to 8.000000010, exactly as they
// should, while the 116 shared sphere triangles came to +2.189830002 in the intersection
// and -2.192173031 in the difference. The residual was the entire deficit.
//
// A fan from a FIXED apex emits the same diagonals whichever way the ring is traversed —
// only the winding flips, which is what is wanted — so the apex is now chosen by geometry
// (the lexicographically smallest vertex position) rather than by ring order, and the two
// copies cancel to zero rather than approximately.
//
// The residual was NOT a refinement artifact, which is what made it worth chasing rather
// than tolerating: across subdivisions 0 to 4 it converged on -2.61e-04 instead of tending
// to zero. Refining a tessellation cannot recover volume that the triangulation never
// enclosed.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace nexus::geometry::brep::testing {

namespace {

double meshVolume(const Body& b, uint32_t sub)
{
    return MeshMassProperties::compute(b.toMesh(sub)).volume;
}

Body sphereAt(double r, double dx)
{
    Body b = makeSphere(static_cast<float>(r), 8, 12);
    b.translate({dx, 0., 0.});
    return b;
}

// Signed volume of one triangle about the origin.
double signedVol(const nexus::render::Vec3& a, const nexus::render::Vec3& b,
                 const nexus::render::Vec3& c)
{
    return (static_cast<double>(a.x) * (static_cast<double>(b.y) * c.z - static_cast<double>(b.z) * c.y) -
            static_cast<double>(a.y) * (static_cast<double>(b.x) * c.z - static_cast<double>(b.z) * c.x) +
            static_cast<double>(a.z) * (static_cast<double>(b.x) * c.y - static_cast<double>(b.y) * c.x)) /
           6.0;
}

// Does the triangle lie in one of the faces of the axis-aligned box of half-extent 1?
bool onBoxPlane(const nexus::render::Vec3& a, const nexus::render::Vec3& b,
                const nexus::render::Vec3& c)
{
    for (int ax = 0; ax < 3; ++ax) {
        for (int s = -1; s <= 1; s += 2) {
            const double t = s;
            auto co = [ax](const nexus::render::Vec3& p) {
                return ax == 0 ? p.x : (ax == 1 ? p.y : p.z);
            };
            if (std::abs(co(a) - t) < 1e-6 && std::abs(co(b) - t) < 1e-6 &&
                std::abs(co(c) - t) < 1e-6)
                return true;
        }
    }
    return false;
}

// Sum the signed volumes of a body's triangles, split by whether they lie on the box.
void splitContribution(const Body& body, double& box, double& other)
{
    box = other = 0.0;
    const Mesh m = body.toMesh(0);
    const auto& pos = m.attributes().positions();
    const auto& topo = m.topology();
    for (size_t f = 0; f < topo.faceCount(); ++f) {
        const auto& idx = topo.face(f).indices;
        for (size_t i = 1; i + 1 < idx.size(); ++i) {
            const auto& a = pos[idx[0]];
            const auto& b = pos[idx[i]];
            const auto& c = pos[idx[i + 1]];
            (onBoxPlane(a, b, c) ? box : other) += signedVol(a, b, c);
        }
    }
}

}  // namespace

// THE identity. Difference and intersection partition the first operand.
TEST(BRepVolumeConservation, DifferencePlusIntersectionEqualsTheFirstOperand)
{
    struct Case { double r, dx; };
    const Case cases[] = {{1.2, 0.0}, {1.2, 0.5}, {1.2, 0.9}, {0.8, 0.0},
                          {0.8, 0.5}, {0.8, 0.7}, {0.8, 0.9}};

    for (const Case& c : cases) {
        const Body A = makeBox(2.f, 2.f, 2.f);
        const Body B = sphereAt(c.r, c.dx);
        const Body I = booleanToBody(A, B, BooleanOp::Intersection);
        const Body D = booleanToBody(A, B, BooleanOp::Difference);
        ASSERT_GT(I.faceCount(), 0u) << "R=" << c.r << " dx=" << c.dx << ": intersection empty";
        ASSERT_GT(D.faceCount(), 0u) << "R=" << c.r << " dx=" << c.dx << ": difference empty";

        Body Ai = A, Bi = B;
        ASSERT_TRUE(imprintMutually(Ai, Bi));
        // asserted at several refinements: a triangulation that never enclosed the volume
        // does not recover it by refining, so a single level could be met by luck
        for (const uint32_t sub : {0u, 2u, 4u}) {
            const double lhs = meshVolume(D, sub) + meshVolume(I, sub);
            const double rhs = meshVolume(Ai, sub);
            EXPECT_NEAR(lhs, rhs, 1e-6 * rhs)
                << "R=" << c.r << " dx=" << c.dx << " sub" << sub << ": D+I=" << lhs
                << " but A=" << rhs << " (relative " << (lhs - rhs) / rhs << ")";
        }
    }
}

// The mechanism itself, not just its consequence: the shared patch must cancel EXACTLY
// between the two results. This is the assertion that names the defect — a volume test
// alone could be satisfied by two errors that happen to offset.
TEST(BRepVolumeConservation, SharedCurvedPatchCancelsExactlyBetweenDifferenceAndIntersection)
{
    for (const double dx : {0.0, 0.5, 0.9}) {
        const Body A = makeBox(2.f, 2.f, 2.f);
        const Body B = sphereAt(1.2, dx);
        const Body I = booleanToBody(A, B, BooleanOp::Intersection);
        const Body D = booleanToBody(A, B, BooleanOp::Difference);
        ASSERT_GT(I.faceCount(), 0u);
        ASSERT_GT(D.faceCount(), 0u);

        double boxI = 0, sphI = 0, boxD = 0, sphD = 0;
        splitContribution(I, boxI, sphI);
        splitContribution(D, boxD, sphD);

        EXPECT_NEAR(sphI + sphD, 0.0, 1e-9)
            << "dx=" << dx << ": the shared sphere patch contributes " << sphI
            << " to the intersection and " << sphD
            << " to the difference — the same patch, triangulated differently";
        EXPECT_NEAR(boxI + boxD, 8.0, 1e-6)
            << "dx=" << dx << ": the box faces should partition the box exactly";
    }
}

// The other identity, which held throughout and must keep holding.
TEST(BRepVolumeConservation, UnionPlusIntersectionEqualsBothOperands)
{
    struct Case { double r, dx; };
    const Case cases[] = {{1.2, 0.0}, {1.2, 0.5}, {1.2, 0.9}, {0.8, 0.5}, {0.8, 0.9}};

    for (const Case& c : cases) {
        const Body A = makeBox(2.f, 2.f, 2.f);
        const Body B = sphereAt(c.r, c.dx);
        const Body U = booleanToBody(A, B, BooleanOp::Union);
        const Body I = booleanToBody(A, B, BooleanOp::Intersection);
        ASSERT_GT(U.faceCount(), 0u) << "R=" << c.r << " dx=" << c.dx;
        ASSERT_GT(I.faceCount(), 0u) << "R=" << c.r << " dx=" << c.dx;

        Body Ai = A, Bi = B;
        ASSERT_TRUE(imprintMutually(Ai, Bi));
        for (const uint32_t sub : {0u, 2u}) {
            const double lhs = meshVolume(U, sub) + meshVolume(I, sub);
            const double rhs = meshVolume(Ai, sub) + meshVolume(Bi, sub);
            EXPECT_NEAR(lhs, rhs, 1e-6 * rhs) << "R=" << c.r << " dx=" << c.dx << " sub" << sub;
        }
    }
}

// A purely PLANAR pair is untouched by the apex rule — every fan of a planar polygon
// encloses the same volume — and its answers are exact, so they are asserted exactly.
TEST(BRepVolumeConservation, PlanarBooleansAreUnaffectedAndExact)
{
    const Body A = makeBox(2.f, 2.f, 2.f);
    Body B = makeBox(2.f, 2.f, 2.f);
    B.translate({1.0, 0., 0.});  // overlap is a 1 x 2 x 2 slab

    const Body U = booleanToBody(A, B, BooleanOp::Union);
    const Body I = booleanToBody(A, B, BooleanOp::Intersection);
    const Body D = booleanToBody(A, B, BooleanOp::Difference);
    ASSERT_GT(U.faceCount(), 0u);
    ASSERT_GT(I.faceCount(), 0u);
    ASSERT_GT(D.faceCount(), 0u);

    EXPECT_NEAR(meshVolume(I, 0), 4.0, 1e-9) << "the overlap slab is 1 x 2 x 2";
    EXPECT_NEAR(meshVolume(U, 0), 12.0, 1e-9) << "8 + 8 - 4";
    EXPECT_NEAR(meshVolume(D, 0), 4.0, 1e-9) << "8 - 4";
    EXPECT_NEAR(meshVolume(D, 0) + meshVolume(I, 0), 8.0, 1e-9);
}

// A pristine primitive's own volume must not have moved: the apex rule reorders the
// triangles of a curved patch and must not change what they enclose overall.
TEST(BRepVolumeConservation, PrimitiveVolumesAreUnchangedByTheApexRule)
{
    // a closed lat-lon sphere tessellates to a known inscribed polyhedron
    const Body S = makeSphere(1.2f, 8, 12);
    EXPECT_NEAR(meshVolume(S, 0), 6.648928, 1e-5);
    const Body C = makeCylinder(1.f, 2.f, 16);
    EXPECT_NEAR(meshVolume(C, 0), 6.122935, 1e-5);
    const Body X = makeBox(2.f, 2.f, 2.f);
    EXPECT_NEAR(meshVolume(X, 0), 8.0, 1e-12);
}

}  // namespace nexus::geometry::brep::testing
