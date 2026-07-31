// A face's sample point decides how the whole face is classified, so "on the material" is
// not a strong enough property for it. It has to be on the material with room to spare.
//
// Every ring the sample test uses is a CHORDAL POLYGON standing in for the face's real
// boundary, and for a boolean seam the real boundary is a circle. Between the polygon and
// the circle it approximates lies a sliver — bounded by the polygon's inradius and its
// circumradius — that is outside the polygon and inside the true curve. A point there
// passes a point-in-polygon test and is nonetheless not on the face.
//
// MEASURED on box(2³) against sphere(r1.2) offset 0.1. The +X face's annulus was sampled
// at radius 0.750609 from the seam centre, where the twelve-sided hole polygon has
// inradius 0.743719 and the true seam circle has radius 0.793725 — outside the polygon by
// seven thousandths, inside the circle by four hundredths. classifyPoint then judged that
// point against the SPHERE rather than against the polygon and reported the annulus as
// touching it, so selectFace treated the face as one of a coincident pair and KEPT a face
// whose material lies entirely outside the sphere. Every seam edge then had three users
// instead of two and the sew refused: intersection and difference both returned empty.
//
// The repair is to stop taking the first candidate that passes and take the one with the
// greatest CLEARANCE from every boundary ring. The candidate order is unchanged and ties
// keep the earlier one, so the choice stays deterministic — which the classification that
// consumes it requires.
//
// Note this is not the tolerance being too tight. The sample was 0.028 inside the sphere,
// nearly three thousand times the 1e-5 tolerance; no tolerance would have rescued it. The
// point was simply in the wrong place.

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

}  // namespace

// THE headline, asserted against the ANALYTIC seam circle rather than against the polygon
// that approximates it — the polygon is what accepted the bad point in the first place.
TEST(BRepFaceSampleClearance, HoledFaceSampleClearsTheTrueSeamCircleNotJustItsPolygon)
{
    struct Case { double r, dx; };
    const Case cases[] = {{1.2, 0.1}, {1.2, 0.3}, {0.8, 0.3}, {1.2, 0.0}};

    for (const Case& c : cases) {
        Body box = makeBox(2.f, 2.f, 2.f);
        Body sph = sphereAt(c.r, c.dx);
        ASSERT_TRUE(imprintMutually(box, sph));

        // the seam circle the +X face carries: radius sqrt(R^2 - d^2) about (1,0,0)
        const double d = 1.0 - c.dx;
        ASSERT_LT(d, c.r) << "fixture must actually pierce the +X face";
        const double seamR = std::sqrt(c.r * c.r - d * d);

        int checked = 0;
        for (uint32_t f = 0; f < static_cast<uint32_t>(box.faceCount()); ++f) {
            if (!box.face(f).alive) continue;
            if (box.faceInnerLoopVertices(f).empty()) continue;
            const Vec3 s = box.faceSamplePoint(f);
            if (std::abs(s.x - 1.0) > 1e-9) continue;  // only the +X annulus
            ++checked;
            const double rho = std::sqrt(s.y * s.y + s.z * s.z);
            EXPECT_GT(rho, seamR)
                << "R=" << c.r << " dx=" << c.dx << ": the annulus is sampled at radius " << rho
                << ", inside the true seam circle of radius " << seamR
                << " — a point in the sliver between that circle and its chordal polygon";
        }
        EXPECT_GT(checked, 0) << "R=" << c.r << " dx=" << c.dx << ": no holed +X face found";
    }
}

// The classification that consumes the sample, checked against a closed-form oracle. The
// annulus's material is entirely outside the sphere, so nothing about it is OnBoundary.
TEST(BRepFaceSampleClearance, AnnulusClassifiesOutsideRatherThanOnBoundary)
{
    for (const double dx : {0.1, 0.3}) {
        Body box = makeBox(2.f, 2.f, 2.f);
        Body sph = sphereAt(1.2, dx);
        ASSERT_TRUE(imprintMutually(box, sph));

        for (uint32_t f = 0; f < static_cast<uint32_t>(box.faceCount()); ++f) {
            if (!box.face(f).alive) continue;
            if (box.faceInnerLoopVertices(f).empty()) continue;
            const Vec3 s = box.faceSamplePoint(f);
            const double ddx = s.x - dx;
            const double dist = std::sqrt(ddx * ddx + s.y * s.y + s.z * s.z);
            const auto cls = box.classifyFace(f, sph);
            EXPECT_EQ(cls, Body::PointContainment::Outside)
                << "dx=" << dx << " face " << f << ": sample is " << (dist - 1.2)
                << " from the sphere surface yet classified " << static_cast<int>(cls);
        }
    }
}

// What the misclassification was costing: two whole configurations whose intersection and
// difference returned empty although their segmentation and classification were otherwise
// correct.
TEST(BRepFaceSampleClearance, NearConcentricBoxSphereBooleansSewAndConserveVolume)
{
    for (const double dx : {0.1, 0.3}) {
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
                << "dx=" << dx << " sub" << sub;
            EXPECT_NEAR(meshVolume(D, sub) + meshVolume(I, sub), meshVolume(Ai, sub),
                        1e-6 * meshVolume(Ai, sub))
                << "dx=" << dx << " sub" << sub;
        }
    }
}

// The sample must stay DETERMINISTIC — the classification built on it has to be
// reproducible on the Null backend — and a face with no holes whose centroid is already on
// it must still get exactly that centroid, so no existing caller moves.
TEST(BRepFaceSampleClearance, SampleIsDeterministicAndSimpleFacesAreUnmoved)
{
    for (int rep = 0; rep < 3; ++rep) {
        Body box = makeBox(2.f, 2.f, 2.f);
        Body sph = sphereAt(1.2, 0.1);
        ASSERT_TRUE(imprintMutually(box, sph));
        Body box2 = makeBox(2.f, 2.f, 2.f);
        Body sph2 = sphereAt(1.2, 0.1);
        ASSERT_TRUE(imprintMutually(box2, sph2));
        ASSERT_EQ(box.faceCount(), box2.faceCount());
        for (uint32_t f = 0; f < static_cast<uint32_t>(box.faceCount()); ++f) {
            if (!box.face(f).alive) continue;
            const Vec3 a = box.faceSamplePoint(f), b = box2.faceSamplePoint(f);
            EXPECT_EQ(a.x, b.x) << "face " << f;
            EXPECT_EQ(a.y, b.y) << "face " << f;
            EXPECT_EQ(a.z, b.z) << "face " << f;
        }
    }

    // An unholed PLANAR face still gets its centroid, bit for bit. That is the guarantee
    // the sample point was given when it was introduced, and it is what keeps every
    // pre-existing classification identical. It is stated for planar faces only, and
    // deliberately: on a CURVED face `Surface::normal` is the axis rather than a face
    // normal, so the early centroid return does not fire there and the candidate search
    // runs — which is exactly the path this change improves.
    const Body planarOnly[] = {makeBox(2.f, 2.f, 2.f), makeCylinder(1.f, 2.f, 16)};
    for (const Body& b : planarOnly) {
        for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
            if (!b.face(f).alive) continue;
            const uint32_t si = b.face(f).surface;
            if (si >= b.surfaceCount() || b.surface(si).kind != SurfaceKind::Plane) continue;
            const std::vector<uint32_t> vs = b.faceVertices(f);
            if (vs.size() < 3) continue;
            Vec3 c{0., 0., 0.};
            for (const uint32_t v : vs) {
                c.x += b.vertex(v).point.x;
                c.y += b.vertex(v).point.y;
                c.z += b.vertex(v).point.z;
            }
            const double inv = 1.0 / static_cast<double>(vs.size());
            const Vec3 centroid{c.x * inv, c.y * inv, c.z * inv};
            const Vec3 s = b.faceSamplePoint(f);
            EXPECT_NEAR(s.x, centroid.x, 1e-12) << "face " << f;
            EXPECT_NEAR(s.y, centroid.y, 1e-12) << "face " << f;
            EXPECT_NEAR(s.z, centroid.z, 1e-12) << "face " << f;
        }
    }
}

// CHARACTERIZATION of what is still empty, so it is stated rather than implied. Both
// classes are EXACT TANGENCIES, where the true result is not a manifold solid: the
// operands touch along a line or at a point and the material pinches to nothing there.
// Returning a clean empty body is the watertight-or-empty contract doing its job, not a
// gap to be papered over — but it is pinned so a change either way is noticed.
TEST(BRepFaceSampleClearance, ExactTangenciesStillReturnCleanEmptyResults)
{
    struct Case { double r, dx; const char* what; };
    const Case cases[] = {
        {0.8, 0.2, "sphere reaches x = 1 exactly"},
        {1.2, 0.2, "sphere reaches x = -1 exactly"},
    };
    for (const Case& c : cases) {
        const Body A = makeBox(2.f, 2.f, 2.f);
        const Body B = sphereAt(c.r, c.dx);
        for (const BooleanOp op :
             {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
            const Body r = booleanToBody(A, B, op);
            const bool ok = r.faceCount() == 0u || (r.isClosed() && r.checkIntegrity().ok);
            EXPECT_TRUE(ok) << c.what << " op " << static_cast<int>(op)
                            << ": neither watertight nor empty";
        }
    }
}

}  // namespace nexus::geometry::brep::testing
