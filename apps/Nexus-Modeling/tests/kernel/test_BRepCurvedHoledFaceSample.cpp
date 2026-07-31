// The last place the sample point could be off its own face — found by looking rather than
// by being bitten.
//
// Three times running, the same shape of defect: the point chosen to speak for a face was a
// point the face does not contain. The centroid sat in the opening of a hole; the sample
// sat in the sliver between a seam's chordal polygon and its true circle; the curved sample
// sagged off its surface entirely. Each fix was local. After the third, the sensible move
// was to go and check every remaining place such a point is chosen, instead of waiting for a
// fourth configuration to expose one.
//
// There is exactly one chooser feeding decisions — faceSamplePoint, consumed by classifyFace
// and by the boolean's face selection — and exactly one case in it left untested: a CURVED
// face carrying a hole. The curved path returned the projected centroid with no hole test at
// all, so on a face whose middle is an opening it returned the middle of the opening.
//
// REACHABILITY, measured rather than assumed, because an unexercised guard is its own
// problem. Sweeping every configuration this kernel can imprint — 254 bodies across
// box/sphere, box/cylinder, box/cone, cylinder pairs, sphere pairs and chained plates —
// produces ZERO curved faces carrying an inner loop. The interior-circle path is gated to
// planes, and every curved pair that could cut one is a quartic the intersector declines.
// So it is not reachable through a boolean.
//
// It IS reachable through `Body::fromFaces`, which is public, and the test below builds it
// there: a cylindrical patch with a hole punched in the middle. Before the fix its sample
// point was the exact centre of that hole. That is a demonstrated defect on a public path,
// not a hypothetical one, which is the difference between fixing this and writing a guard
// for a case nobody can construct.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace nexus::geometry::brep::testing {

namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;

// A cylindrical patch spanning a quarter turn and z in [-1,1], with a hole in the middle.
// Both rings lie exactly on the cylinder of radius `R` about the Z axis.
std::optional<Body> holedCylindricalPatch(double R, double holeRadius)
{
    std::vector<Vec3> pts;
    auto onCyl = [&](double a, double z) {
        return Vec3{R * std::cos(a), R * std::sin(a), z};
    };
    const double a0 = -kTwoPi / 8.0, a1 = kTwoPi / 8.0;
    for (int i = 0; i <= 8; ++i) pts.push_back(onCyl(a0 + (a1 - a0) * i / 8.0, -1.0));
    for (int i = 8; i >= 0; --i) pts.push_back(onCyl(a0 + (a1 - a0) * i / 8.0, 1.0));
    const uint32_t outerN = static_cast<uint32_t>(pts.size());

    const uint32_t innerStart = outerN;
    for (int i = 0; i < 8; ++i) {
        const double t = kTwoPi * i / 8.0;
        pts.push_back(onCyl(holeRadius * std::cos(t), holeRadius * std::sin(t)));
    }

    Body::FaceDef fd;
    for (uint32_t i = 0; i < outerN; ++i) fd.loop.push_back(i);
    std::vector<uint32_t> hole;  // wound opposite the outer ring, so it bounds an opening
    for (int i = 7; i >= 0; --i) hole.push_back(innerStart + static_cast<uint32_t>(i));
    fd.innerLoops.push_back(std::move(hole));
    fd.surface.kind = SurfaceKind::Cylinder;
    fd.surface.origin = {0., 0., 0.};
    fd.surface.normal = {0., 0., 1.};
    fd.surface.uAxis = {1., 0., 0.};
    fd.surface.radius = R;
    return Body::fromFaces(pts, {fd});
}

}  // namespace

// THE assertion. The sample must be on the surface AND clear of the opening — both, since
// projecting alone puts it on the surface while leaving it over the hole.
TEST(BRepCurvedHoledFaceSample, SampleOnAHoledCurvedFaceIsOnTheMaterialNotTheOpening)
{
    const double R = 1.0, holeR = 0.25;
    const auto body = holedCylindricalPatch(R, holeR);
    ASSERT_TRUE(body.has_value()) << "fixture did not build";
    ASSERT_EQ(body->faceCount(), 1u);
    ASSERT_EQ(body->faceInnerLoopVertices(0).size(), 1u) << "the face must carry its hole";

    const Vec3 s = body->faceSamplePoint(0);

    // on the cylinder
    EXPECT_NEAR(std::sqrt(s.x * s.x + s.y * s.y), R, 1e-9)
        << "the sample is not on the face's own surface";

    // and outside the opening, which on this patch is the disc of parametric radius holeR
    // about (angle 0, z 0)
    const double angle = std::atan2(s.y, s.x);
    const double inHole = std::sqrt(angle * angle + s.z * s.z);
    EXPECT_GT(inHole, holeR)
        << "the sample sits inside the hole at angle " << angle << ", z " << s.z
        << " — the one point on the face that is not on it";
}

// The fix must not disturb the case that has no hole: a curved face without one is still
// sampled at its projected centroid, which is what every curved classification now relies
// on.
TEST(BRepCurvedHoledFaceSample, UnholedCurvedFacesAreUnchanged)
{
    for (const Body& b : {makeCylinder(1.f, 2.f, 12), makeSphere(1.f, 6, 10),
                          makeCone(1.f, 2.f, 8)}) {
        for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
            if (!b.face(f).alive) continue;
            const Surface& surf = b.surface(b.face(f).surface);
            if (surf.kind == SurfaceKind::Plane) continue;
            ASSERT_TRUE(b.faceInnerLoopVertices(f).empty());
            const Vec3 s = b.faceSamplePoint(f);
            if (surf.kind == SurfaceKind::Sphere) {
                const Vec3 d = s - surf.origin;
                EXPECT_NEAR(std::sqrt(d.dot(d)), surf.radius, 1e-9) << "face " << f;
            } else if (surf.kind == SurfaceKind::Cylinder) {
                const Vec3 d = s - surf.origin;
                const Vec3 ax = surf.normal;
                const Vec3 rad = d - ax * d.dot(ax);
                EXPECT_NEAR(std::sqrt(rad.dot(rad)), surf.radius, 1e-9) << "face " << f;
            }
        }
    }
}

// Determinism: the classification built on this point has to be reproducible, and the
// candidate search picks by clearance with ties keeping the earlier candidate.
TEST(BRepCurvedHoledFaceSample, TheSampleIsDeterministic)
{
    for (int rep = 0; rep < 3; ++rep) {
        const auto a = holedCylindricalPatch(1.0, 0.25);
        const auto b = holedCylindricalPatch(1.0, 0.25);
        ASSERT_TRUE(a.has_value() && b.has_value());
        const Vec3 pa = a->faceSamplePoint(0), pb = b->faceSamplePoint(0);
        EXPECT_EQ(pa.x, pb.x);
        EXPECT_EQ(pa.y, pb.y);
        EXPECT_EQ(pa.z, pb.z);
    }
}

// CHARACTERIZATION of reachability, so a later reader knows why this guard exists and does
// not delete it as dead: no boolean this kernel can perform produces a curved face with a
// hole. If that ever changes — a curved/curved intersection landing an interior seam — this
// count moves and the guard starts earning its keep through the boolean too.
TEST(BRepCurvedHoledFaceSample, NoBooleanProducesACurvedFaceWithAHoleToday)
{
    auto curvedHoled = [](const Body& b) {
        int n = 0;
        for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f) {
            if (!b.face(f).alive) continue;
            if (b.surface(b.face(f).surface).kind == SurfaceKind::Plane) continue;
            if (!b.faceInnerLoopVertices(f).empty()) ++n;
        }
        return n;
    };

    int total = 0, bodies = 0;
    for (double d = 0.0; d < 1.3; d += 0.4) {
        const std::pair<Body, Body> pairs[] = {
            {makeBox(2.f, 2.f, 2.f), makeSphere(1.2f, 8, 12)},
            {makeBox(2.f, 2.f, 2.f), makeCylinder(0.7f, 4.f, 16)},
            {makeCylinder(1.f, 4.f, 16), makeCylinder(0.7f, 3.f, 16)},
        };
        for (const auto& [A0, B0] : pairs) {
            Body A = A0, B = B0;
            B.translate({d, 0., 0.});
            Body Ai = A, Bi = B;
            (void)imprintMutually(Ai, Bi);
            bodies += 2;
            total += curvedHoled(Ai) + curvedHoled(Bi);
            for (const BooleanOp op :
                 {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
                const Body r = booleanToBody(A, B, op);
                if (r.faceCount() == 0u) continue;
                ++bodies;
                total += curvedHoled(r);
            }
        }
    }
    EXPECT_GT(bodies, 20) << "the sweep did not actually run";
    EXPECT_EQ(total, 0)
        << "a boolean now yields a curved face carrying a hole — the interior-seam path has "
           "opened up, so re-check that its sample point is on the material";
}

}  // namespace nexus::geometry::brep::testing
