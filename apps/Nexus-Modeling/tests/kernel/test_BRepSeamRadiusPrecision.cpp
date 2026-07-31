// The seam a surface/surface intersection reports must be as precise as the geometry it
// is derived from — the last float in the analytic B-rep's construction chain.
//
// `Surface::radius` is a double, `Curve::radius` is a double, and every branch of
// intersectSurfaces computes its result as a double: sqrt(R² − d²) for a plane section
// of a sphere, the chord formula for sphere against sphere. The single file-local helper
// that assembled the resulting Curve took its radius as a FLOAT, so the value was
// rounded on its way between two double quantities and nothing downstream could see it.
//
// Measured on box(2³) against sphere(r=1.2): the seam came back with radius
// 0.66332507133483887 where the exact value is 0.66332504433416373 — wrong by 2.7e-08,
// which is float resolution at unit scale and has nothing to do with the geometry.
//
// WHY IT MATTERS, and why the assertion here is relative rather than absolute: the two
// operands do not both inherit the error. The box's ring is cut AT the reported radius,
// while the sphere's ring is pinned to the sphere and the cutting plane and lands at the
// true one, so a single circle becomes two concentric rings a few times 1e-8 apart. That
// is far below any weld band, so it does not announce itself as a gap; it announces
// itself much later as topology that will not close.
//
// The test asserts a RELATIVE agreement of 1e-15 at three scales. Both parts are
// load-bearing. A float in the chain is a fixed relative error, so a single-scale
// absolute bound can be met by tightening the model rather than the arithmetic — the
// same trap the double migration hid in for an entire pass. At 1e-15 the float value
// misses by seven orders of magnitude.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepSurfaceIntersect.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

namespace {

Surface planeAt(const Vec3& origin, const Vec3& normal)
{
    Surface s;
    s.kind = SurfaceKind::Plane;
    s.origin = origin;
    s.normal = normal;
    return s;
}

Surface sphereAtCentre(const Vec3& centre, double radius)
{
    Surface s;
    s.kind = SurfaceKind::Sphere;
    s.origin = centre;
    s.radius = radius;
    return s;
}

}  // namespace

// A plane section of a sphere: the exact radius is sqrt(R² − d²), and it is available in
// double from double inputs, so the seam must reproduce it to double precision.
TEST(BRepSeamRadiusPrecision, PlaneSphereSeamRadiusIsExactAtEveryScale)
{
    for (const double scale : {1.0, 1000.0, 1.0e6}) {
        const double R = 1.2 * scale;
        const double d = 1.0 * scale;  // the cutting plane's distance from the centre
        const auto si = intersectSurfaces(planeAt({0., 0., d}, {0., 0., 1.}),
                                          sphereAtCentre({0., 0., 0.}, R));
        ASSERT_EQ(si.kind, SurfaceIntersectionKind::Circle) << "scale " << scale;

        const double exact = std::sqrt(R * R - d * d);
        EXPECT_NEAR(si.curve.radius, exact, 1e-15 * exact)
            << "scale " << scale << ": seam radius " << si.curve.radius << " against the exact "
            << exact << " — relative error " << std::abs(si.curve.radius - exact) / exact
            << "; float resolution here is about 6e-8";
    }
}

// Sphere against sphere, the other Circle branch, through the same helper.
TEST(BRepSeamRadiusPrecision, SphereSphereSeamRadiusIsExactAtEveryScale)
{
    for (const double scale : {1.0, 1000.0, 1.0e6}) {
        const double R = 1.2 * scale;
        const double sep = 1.0 * scale;
        const auto si = intersectSurfaces(sphereAtCentre({0., 0., 0.}, R),
                                          sphereAtCentre({sep, 0., 0.}, R));
        ASSERT_EQ(si.kind, SurfaceIntersectionKind::Circle) << "scale " << scale;

        // equal radii ⇒ the seam plane bisects the centres
        const double s = sep * 0.5;
        const double exact = std::sqrt(R * R - s * s);
        EXPECT_NEAR(si.curve.radius, exact, 1e-15 * exact)
            << "scale " << scale << ": relative error "
            << std::abs(si.curve.radius - exact) / exact;
    }
}

// The consequence that actually bites: a point on the reported seam must lie on the
// sphere. This is the form the defect took in the Boolean — the seam said one radius and
// the sphere said another, so the two operands cut rings that were not the same ring.
TEST(BRepSeamRadiusPrecision, SeamPointsLieOnBothSurfaces)
{
    const double R = 1.2, d = 1.0;
    const auto si = intersectSurfaces(planeAt({0., 0., d}, {0., 0., 1.}),
                                      sphereAtCentre({0., 0., 0.}, R));
    ASSERT_EQ(si.kind, SurfaceIntersectionKind::Circle);

    double worstOnSphere = 0.0, worstOnPlane = 0.0;
    for (int k = 0; k < 32; ++k) {
        const Vec3 p = si.curve.eval(6.283185307179586 * k / 32.0);
        worstOnSphere =
            std::max(worstOnSphere, std::abs(std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z) - R));
        worstOnPlane = std::max(worstOnPlane, std::abs(p.z - d));
    }
    EXPECT_LT(worstOnSphere, 1e-14) << "seam points miss the sphere by " << worstOnSphere;
    EXPECT_LT(worstOnPlane, 1e-14) << "seam points miss the plane by " << worstOnPlane;
}

// A tangency must still be reported as a Point rather than a zero-radius Circle: widening
// the radius must not move where the branch boundary sits.
TEST(BRepSeamRadiusPrecision, TangencyStillReportsAPointNotADegenerateCircle)
{
    const double R = 1.2;
    const auto si = intersectSurfaces(planeAt({0., 0., R}, {0., 0., 1.}),
                                      sphereAtCentre({0., 0., 0.}, R));
    EXPECT_EQ(si.kind, SurfaceIntersectionKind::Point);

    const auto miss = intersectSurfaces(planeAt({0., 0., R * 2.0}, {0., 0., 1.}),
                                        sphereAtCentre({0., 0., 0.}, R));
    EXPECT_EQ(miss.kind, SurfaceIntersectionKind::None);
}

}  // namespace nexus::geometry::brep::testing
