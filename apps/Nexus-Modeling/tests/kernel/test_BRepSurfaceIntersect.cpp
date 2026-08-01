// Foundation — analytic surface/surface intersection (the geometric core of the
// B-rep boolean's imprint step). Each intersection is returned as an exact
// brep::Curve; these tests verify the curve lies on BOTH input surfaces (via the
// implicit surfaceDistance) and that non-intersecting configurations return None.

#include <nexus/geometry/BRepSurfaceIntersect.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {


namespace {
Surface plane(Vec3 o, Vec3 n) { Surface s; s.kind = SurfaceKind::Plane; s.origin = o; s.normal = n; return s; }
Surface sphere(Vec3 c, float r) { Surface s; s.kind = SurfaceKind::Sphere; s.origin = c; s.radius = r; return s; }
Surface cylinder(Vec3 o, Vec3 ax, float r) { Surface s; s.kind = SurfaceKind::Cylinder; s.origin = o; s.normal = ax; s.radius = r; return s; }
// A cone stores origin = APEX, normal = axis (apex -> base), radius = SLOPE.
Surface cone(Vec3 apex, Vec3 ax, float slope) { Surface s; s.kind = SurfaceKind::Cone; s.origin = apex; s.normal = ax; s.radius = slope; return s; }

// Max distance of the intersection curve (sampled) to both surfaces.
float curveOnBoth(const SurfaceIntersection& r, const Surface& a, const Surface& b)
{
    float maxd = 0.f;
    const bool circle = (r.kind == SurfaceIntersectionKind::Circle);
    for (int i = 0; i < 24; ++i) {
        const float t = circle ? (6.2831853f * static_cast<float>(i) / 24.f)
                               : static_cast<float>(i - 12);
        const Vec3 p = r.curve.eval(t);
        maxd = std::max(maxd, std::max(std::abs(surfaceDistance(a, p)), std::abs(surfaceDistance(b, p))));
    }
    return maxd;
}
}  // namespace

TEST(BRepSurfaceIntersect, PlanePlaneIsLineOnBothPlanes)
{
    const Surface a = plane({0, 0, 0}, {0, 0, 1});
    const Surface b = plane({0, 0, 0}, {1, 0, 0});
    const auto r = intersectSurfaces(a, b);
    ASSERT_EQ(r.kind, SurfaceIntersectionKind::Line);
    EXPECT_LT(curveOnBoth(r, a, b), 1e-4f);
}

TEST(BRepSurfaceIntersect, ParallelPlanesDoNotMeet)
{
    EXPECT_EQ(intersectSurfaces(plane({0, 0, 0}, {0, 0, 1}), plane({0, 0, 3}, {0, 0, 1})).kind,
              SurfaceIntersectionKind::None);
}

TEST(BRepSurfaceIntersect, PlaneSphereIsCircleOnBoth)
{
    const Surface p = plane({0, 0, 0.5f}, {0, 0, 1});
    const Surface s = sphere({0, 0, 0}, 2.f);
    const auto r = intersectSurfaces(p, s);
    ASSERT_EQ(r.kind, SurfaceIntersectionKind::Circle);
    EXPECT_NEAR(r.curve.radius, std::sqrt(2.f * 2.f - 0.5f * 0.5f), 1e-4f);
    EXPECT_LT(curveOnBoth(r, p, s), 1e-4f);
    // Argument order is handled symmetrically.
    EXPECT_EQ(intersectSurfaces(s, p).kind, SurfaceIntersectionKind::Circle);
}

TEST(BRepSurfaceIntersect, PlaneMissingSphereReturnsNone)
{
    EXPECT_EQ(intersectSurfaces(plane({0, 0, 5}, {0, 0, 1}), sphere({0, 0, 0}, 2.f)).kind,
              SurfaceIntersectionKind::None);
}

TEST(BRepSurfaceIntersect, PlanePerpendicularToCylinderIsCircle)
{
    const Surface p = plane({0, 0, 1}, {0, 0, 1});
    const Surface c = cylinder({0, 0, 0}, {0, 0, 1}, 1.5f);
    const auto r = intersectSurfaces(p, c);
    ASSERT_EQ(r.kind, SurfaceIntersectionKind::Circle);
    EXPECT_NEAR(r.curve.radius, 1.5f, 1e-4f);
    EXPECT_LT(curveOnBoth(r, p, c), 1e-4f);
}

// A plane whose normal is truly collinear with the cylinder axis (even scaled /
// anti-parallel) is exactly perpendicular → a Circle; a near-perpendicular plane
// (normal NOT collinear with the axis) is an ellipse, not a circle, and must NOT
// be reported as one — where the old nearlyEqual(|n·ax|,1) float band would.
TEST(BRepSurfaceIntersect, ExactPlaneCylinderPerpendicularity)
{
    // Exactly perpendicular (axis +Z, plane normal −2Z, collinear) → Circle.
    const Surface pPerp = plane({0, 0, 1}, {0, 0, -2});
    const Surface c = cylinder({0, 0, 0}, {0, 0, 1}, 1.5f);
    EXPECT_EQ(intersectSurfaces(pPerp, c).kind, SurfaceIntersectionKind::Circle);

    // Near-perpendicular: normal (0, 1, 16000000) — its float |n·ax| rounds to 1
    // (the old band → "circle") but the normal is NOT collinear with the axis
    // (int64 cross-x = 1·1 − 16000000·0 = 1 ≠ 0), so the true section is an
    // ellipse. The exact test declines the bogus Circle.
    const long long az = 16000000;
    ASSERT_NE(1LL * 1 - az * 0, 0);  // int64: normal not collinear with (0,0,1)
    const Surface pTilt = plane({0, 0, 1}, {0.f, 1.f, static_cast<float>(az)});
    const float la = std::sqrt(1.f + static_cast<float>(az) * static_cast<float>(az));
    EXPECT_NEAR(std::abs(static_cast<float>(az) / la), 1.f, 1e-6f);  // float |n̂·ax| ≈ 1
    EXPECT_NE(intersectSurfaces(pTilt, c).kind, SurfaceIntersectionKind::Circle);
}

TEST(BRepSurfaceIntersect, SphereSphereIsCircleOnBoth)
{
    const Surface a = sphere({0, 0, 0}, 2.f);
    const Surface b = sphere({3, 0, 0}, 2.f);
    const auto r = intersectSurfaces(a, b);
    ASSERT_EQ(r.kind, SurfaceIntersectionKind::Circle);
    EXPECT_LT(curveOnBoth(r, a, b), 1e-4f);
}

TEST(BRepSurfaceIntersect, DisjointSpheresReturnNone)
{
    EXPECT_EQ(intersectSurfaces(sphere({0, 0, 0}, 1.f), sphere({5, 0, 0}, 1.f)).kind,
              SurfaceIntersectionKind::None);
}

// Two truly-parallel (collinear-normal) planes never yield a Line.
TEST(BRepSurfaceIntersect, ParallelPlanesNoLine)
{
    // Same normal, different offset → disjoint parallel → None.
    EXPECT_EQ(intersectSurfaces(plane({0, 0, 0}, {0, 0, 1}), plane({0, 0, 3}, {0, 0, 1})).kind,
              SurfaceIntersectionKind::None);
    // Anti-parallel normals are still collinear → parallel.
    EXPECT_EQ(intersectSurfaces(plane({0, 0, 0}, {0, 0, 2}), plane({0, 0, 3}, {0, 0, -5})).kind,
              SurfaceIntersectionKind::None);
}

// A genuine shallow-angle plane pair: the normals are non-collinear (they DO
// intersect in a Line), but the intersection is so shallow that the old
// `|n̂A × n̂B|² < 1e-12` float threshold mis-classifies it as parallel and misses
// the Line. The exact collinearity test (orient2D minors) gets it right; an int64
// ground truth confirms the normals are truly non-collinear.
TEST(BRepSurfaceIntersect, ShallowAnglePlanesStillIntersect)
{
    const long long ax = 16000000;  // float32-exact integer normal components
    // nA = (ax,0,0), nB = (ax,1,0): raw cross z = ax·1 − 0·ax = ax ≠ 0 → non-collinear.
    const long long crossZ = ax * 1 - 0 * ax;
    ASSERT_NE(crossZ, 0);  // int64 ground truth: the planes genuinely intersect

    const Surface a = plane({0, 0, 0}, {static_cast<float>(ax), 0, 0});
    const Surface b = plane({0, 0, 0}, {static_cast<float>(ax), 1, 0});

    // The naive float threshold the fix replaces would call this parallel:
    const float la = std::sqrt(static_cast<float>(ax) * static_cast<float>(ax));
    const float lb = std::sqrt(static_cast<float>(ax) * static_cast<float>(ax) + 1.f);
    const Vec3 nA{static_cast<float>(ax) / la, 0.f, 0.f};
    const Vec3 nB{static_cast<float>(ax) / lb, 1.f / lb, 0.f};
    const float L2 = std::pow(nA.y * nB.z - nA.z * nB.y, 2.f) +
                     std::pow(nA.z * nB.x - nA.x * nB.z, 2.f) +
                     std::pow(nA.x * nB.y - nA.y * nB.x, 2.f);
    EXPECT_LT(L2, 1e-12f);  // the old threshold → "parallel" (WRONG)

    // The exact SSI returns the real Line, with a finite non-degenerate direction.
    const auto r = intersectSurfaces(a, b);
    EXPECT_EQ(r.kind, SurfaceIntersectionKind::Line);
    const Vec3 d = r.curve.dir;
    const float dl = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
    EXPECT_NEAR(dl, 1.f, 1e-4f);  // unit direction, not NaN/zero
    // These planes share the z-axis (both pass through the origin, normals in xy)
    // → the intersection line is the z-axis.
    EXPECT_NEAR(std::abs(d.z), 1.f, 1e-4f);

    // Deterministic.
    EXPECT_EQ(intersectSurfaces(a, b).curve.dir.z, r.curve.dir.z);
}

// ── The cone pairs ───────────────────────────────────────────────────────────
//
// Until these landed, EVERY pair involving a cone except plane∩cone fell through
// intersectSurfaces' dispatch to Unsupported — and imprintOneWay treats Unsupported
// exactly as it treats None, so the boolean's imprint did no work, reported success,
// and the watertight-or-empty invariant turned the result into a clean-looking empty
// body. Measured: cone(r=1,h=2) unioned with a coaxial rod returned EMPTY, and so did
// the difference. The general (skew) case is still a quartic and still says Unsupported,
// which is the honest answer rather than a silent one.

TEST(BRepSurfaceIntersect, ConeCylinderCoaxialIsTheRingWhereTheNappeReachesTheRadius)
{
    // Apex at the origin, opening along +z with slope 0.5: radius 0.5*t at height t.
    // A coaxial cylinder of radius 0.3 is met where 0.5*t == 0.3, i.e. t == 0.6.
    const Surface a = cone({0, 0, 0}, {0, 0, 1}, 0.5f);
    const Surface b = cylinder({0, 0, -5}, {0, 0, 1}, 0.3f);
    const auto r = intersectSurfaces(a, b);
    ASSERT_EQ(r.kind, SurfaceIntersectionKind::Circle);
    EXPECT_NEAR(r.curve.origin.z, 0.6f, 1e-5f);
    EXPECT_NEAR(r.curve.radius, 0.3f, 1e-5f);
    EXPECT_LT(curveOnBoth(r, a, b), 1e-4f);
    EXPECT_LT(curveOnBoth(r, b, a), 1e-4f);  // argument order must not matter
    EXPECT_EQ(intersectSurfaces(b, a).kind, SurfaceIntersectionKind::Circle);
}

TEST(BRepSurfaceIntersect, ConeCylinderOffAxisIsUnsupportedNotNone)
{
    // A parallel but offset axis makes the section a quartic. The distinction being
    // asserted is the whole point: reporting None here would tell the imprint the
    // surfaces do not meet, which is false, and the boolean would silently drop a seam.
    const Surface a = cone({0, 0, 0}, {0, 0, 1}, 0.5f);
    const Surface b = cylinder({0.4f, 0, -5}, {0, 0, 1}, 0.3f);
    EXPECT_EQ(intersectSurfaces(a, b).kind, SurfaceIntersectionKind::Unsupported);
    // Skew, likewise.
    const Surface c = cylinder({0, 0, 1}, {1, 0, 0}, 0.3f);
    EXPECT_EQ(intersectSurfaces(a, c).kind, SurfaceIntersectionKind::Unsupported);
}

TEST(BRepSurfaceIntersect, ConeSphereOnAxisGivesTwoRingsAndBothLieOnBothSurfaces)
{
    // Slope 1 (45°), apex at the origin along +z. A sphere centred ON the axis at
    // distance d only reaches the nappe when its radius beats the perpendicular
    // distance d/√2 — so R = 2.5 at d = 3 cuts it twice and R = 0.5 would not touch
    // it at all. Substituting radius = t: 2t² − 6t + (9 − 6.25) = 0 → t = (3 ∓ √3.5)/2.
    const Surface a = cone({0, 0, 0}, {0, 0, 1}, 1.0f);
    const Surface b = sphere({0, 0, 3}, 2.5f);
    const auto r = intersectSurfaces(a, b);
    ASSERT_EQ(r.kind, SurfaceIntersectionKind::TwoCircles);
    const float t0 = (3.f - std::sqrt(3.5f)) * 0.5f, t1 = (3.f + std::sqrt(3.5f)) * 0.5f;
    EXPECT_NEAR(r.curve.origin.z, t0, 1e-5f);
    EXPECT_NEAR(r.curve2.origin.z, t1, 1e-5f);
    // Ring radius is slope*t, and slope is 1 here.
    EXPECT_NEAR(r.curve.radius, t0, 1e-5f);
    EXPECT_NEAR(r.curve2.radius, t1, 1e-5f);
    EXPECT_LT(curveOnBoth(r, a, b), 1e-4f);
    SurfaceIntersection second = r;
    second.curve = r.curve2;
    EXPECT_LT(curveOnBoth(second, a, b), 1e-4f);
}

TEST(BRepSurfaceIntersect, ConeSphereSwallowingTheApexGivesOneRingNotTwo)
{
    // The second root is behind the apex, which is not on the nappe. Returning it would
    // imprint a phantom ring on the mirror cone the Surface convention does not include.
    const Surface a = cone({0, 0, 0}, {0, 0, 1}, 1.0f);
    const Surface b = sphere({0, 0, 0.2f}, 1.0f);  // contains the apex
    const auto r = intersectSurfaces(a, b);
    ASSERT_EQ(r.kind, SurfaceIntersectionKind::Circle);
    EXPECT_GT(r.curve.origin.z, 0.f);
    EXPECT_LT(curveOnBoth(r, a, b), 1e-4f);
}

TEST(BRepSurfaceIntersect, ConeSphereTooFarAwayIsNone)
{
    const Surface a = cone({0, 0, 0}, {0, 0, 1}, 0.2f);
    // Far off to the side of the narrow nappe, still centred on the axis line but
    // behind the apex, so nothing on the nappe reaches it.
    EXPECT_EQ(intersectSurfaces(a, sphere({0, 0, -3}, 0.5f)).kind, SurfaceIntersectionKind::None);
    // Off the axis is a quartic, not "no intersection".
    EXPECT_EQ(intersectSurfaces(a, sphere({3, 0, 4}, 0.5f)).kind,
              SurfaceIntersectionKind::Unsupported);

    // A sphere sitting on the axis INSIDE a wide nappe touches nothing. Slope 1 puts the
    // surface d/√2 away from the axis point at depth d, so R = 0.5 at d = 2 falls well
    // short — being centred on the axis is not the same as reaching the cone.
    const Surface wide = cone({0, 0, 0}, {0, 0, 1}, 1.0f);
    EXPECT_EQ(intersectSurfaces(wide, sphere({0, 0, 2}, 0.5f)).kind,
              SurfaceIntersectionKind::None);
    // Just over the threshold it does reach — the boundary is R√2 = d.
    EXPECT_NE(intersectSurfaces(wide, sphere({0, 0, 2}, 1.45f)).kind,
              SurfaceIntersectionKind::None);
}

TEST(BRepSurfaceIntersect, ConeConeApexToApexMeetsWhereTheirRadiiAgree)
{
    // Opposed nappes on one axis, slope 1 each, apexes at z = 0 and z = 4. Their radii
    // agree midway, at z = 2, where both are 2.
    const Surface a = cone({0, 0, 0}, {0, 0, 1}, 1.0f);
    const Surface b = cone({0, 0, 4}, {0, 0, -1}, 1.0f);
    const auto r = intersectSurfaces(a, b);
    ASSERT_EQ(r.kind, SurfaceIntersectionKind::Circle);
    EXPECT_NEAR(r.curve.origin.z, 2.f, 1e-5f);
    EXPECT_NEAR(r.curve.radius, 2.f, 1e-5f);
    EXPECT_LT(curveOnBoth(r, a, b), 1e-4f);
}

TEST(BRepSurfaceIntersect, ConeConeNestedSameDirectionMeetsOnceAndParallelNappesNever)
{
    // Same direction, different slopes: the steeper one catches the shallower one up.
    // Apexes at 0 and 1, slopes 1 and 2 → 1*t == 2*(t−1) → t = 2, radius 2.
    const Surface a = cone({0, 0, 0}, {0, 0, 1}, 1.0f);
    const Surface b = cone({0, 0, 1}, {0, 0, 1}, 2.0f);
    const auto r = intersectSurfaces(a, b);
    ASSERT_EQ(r.kind, SurfaceIntersectionKind::Circle);
    EXPECT_NEAR(r.curve.origin.z, 2.f, 1e-5f);
    EXPECT_NEAR(r.curve.radius, 2.f, 1e-5f);
    EXPECT_LT(curveOnBoth(r, a, b), 1e-4f);

    // Equal slopes, same direction, offset apexes: nested nappes that never meet.
    const Surface c = cone({0, 0, 1}, {0, 0, 1}, 1.0f);
    EXPECT_EQ(intersectSurfaces(a, c).kind, SurfaceIntersectionKind::None);

    // A shared apex with equal slope IS the same surface, not an intersection curve —
    // reported Unsupported, as cylinder∩cylinder reports its coincident case.
    EXPECT_EQ(intersectSurfaces(a, cone({0, 0, 0}, {0, 0, 1}, 1.0f)).kind,
              SurfaceIntersectionKind::Unsupported);
    // A shared apex with DIFFERENT slopes touches only at that apex.
    const auto pt = intersectSurfaces(a, cone({0, 0, 0}, {0, 0, 1}, 2.0f));
    EXPECT_EQ(pt.kind, SurfaceIntersectionKind::Point);
    EXPECT_NEAR(pt.point.z, 0.f, 1e-6f);
}

TEST(BRepSurfaceIntersect, SurfaceDistanceMeasuresTheConeInsteadOfReturning1e30)
{
    // Every assertion above rests on surfaceDistance, which answered 1e30 for a cone
    // until the cone pairs landed — so a cone seam could not be verified by the one
    // helper whose job is verifying seams. A test that could not fail is not a test.
    const Surface a = cone({0, 0, 0}, {0, 0, 1}, 0.5f);
    EXPECT_NEAR(surfaceDistance(a, {0.5f, 0.f, 1.f}), 0.f, 1e-6f);   // exactly on the nappe
    EXPECT_GT(surfaceDistance(a, {1.0f, 0.f, 1.f}), 0.f);            // outside
    EXPECT_LT(surfaceDistance(a, {0.1f, 0.f, 1.f}), 0.f);            // inside
    EXPECT_NEAR(surfaceDistance(a, {0.f, 0.f, -2.f}), 2.f, 1e-6f);   // behind the apex
}

}  // namespace nexus::geometry::brep::testing
