// A cone could not be combined with anything.
//
// `intersectSurfaces` handled plane/plane, plane/sphere, plane/cylinder and
// sphere/sphere. Every pair involving a cone returned Unsupported, so no seam was ever
// offered and no face was ever cut: a box against a cone imprinted NOTHING — six faces
// stayed six, seventeen stayed seventeen — and all three operators returned empty. A cone
// is one of the primitives the editor places, so this was not an exotic corner.
//
// Two families of plane section are curves this kernel can already represent, and they are
// the two that matter for cutting a cone with a box:
//
//   * a plane PERPENDICULAR to the axis cuts a CIRCLE. Unlike a cylinder's sections, which
//     all share one radius, a cone's ring at axial distance v from the apex has radius
//     slope*v — so the radius is a function of where the plane falls, not a constant.
//   * a plane containing BOTH the apex and the axis cuts the two rulings lying in it.
//
// Everything else — an oblique plane — cuts an ellipse, a parabola or a hyperbola, none of
// which is a Line or a Circle. Those stay Unsupported rather than being approximated, and
// that is a real boundary worth stating: a box face PARALLEL to the axis but missing the
// apex cuts a hyperbola, so a cone wider than the box around it is still out of scope. The
// tests below assert that boundary as deliberately as they assert the capability.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/BRepSurfaceIntersect.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

namespace {

constexpr double kPi = 3.14159265358979323846;

// apex at the origin, axis +Z, slope = base radius / height
Surface coneSurface(double slope)
{
    Surface s;
    s.kind = SurfaceKind::Cone;
    s.origin = {0., 0., 0.};
    s.normal = {0., 0., 1.};
    s.uAxis = {1., 0., 0.};
    s.radius = slope;
    return s;
}

Surface planeAt(const Vec3& origin, const Vec3& normal)
{
    Surface s;
    s.kind = SurfaceKind::Plane;
    s.origin = origin;
    s.normal = normal;
    return s;
}

double meshVolume(const Body& b, uint32_t sub)
{
    return MeshMassProperties::compute(b.toMesh(sub)).volume;
}

bool solid(const Body& b)
{
    return b.faceCount() > 0 && b.isClosed() && b.checkIntegrity().ok && b.checkGeometry().ok;
}

}  // namespace

// A perpendicular plane cuts a circle whose radius DEPENDS on where it cuts. Asserted as a
// metric quantity at several heights, because a constant radius — the cylinder's answer —
// would be geometrically wrong while still being a perfectly valid Circle.
TEST(BRepConeSection, PerpendicularPlaneCutsARingWhoseRadiusGrowsWithDistanceFromTheApex)
{
    const double slope = 0.25;
    for (const double v : {0.5, 1.0, 2.0, 7.5}) {
        const auto si = intersectSurfaces(planeAt({0., 0., v}, {0., 0., 1.}), coneSurface(slope));
        ASSERT_EQ(si.kind, SurfaceIntersectionKind::Circle) << "v=" << v;
        EXPECT_NEAR(si.curve.radius, slope * v, 1e-12)
            << "v=" << v << ": a cone's ring radius is slope*v, not a constant";
        EXPECT_NEAR(si.curve.origin.z, v, 1e-12) << "v=" << v << ": ring is not at the plane";
        EXPECT_NEAR(std::abs(si.curve.dir.z), 1.0, 1e-12) << "ring axis should be the cone axis";
    }
}

// The apex and the empty half. A cone is single-napped here — v is a distance from the
// apex along the axis — so a plane behind it meets nothing and one through it meets a
// point, not a zero-radius circle.
TEST(BRepConeSection, TheApexIsAPointAndBehindItIsNothing)
{
    const Surface cone = coneSurface(0.25);
    const auto atApex = intersectSurfaces(planeAt({0., 0., 0.}, {0., 0., 1.}), cone);
    EXPECT_EQ(atApex.kind, SurfaceIntersectionKind::Point);
    EXPECT_NEAR(atApex.point.z, 0.0, 1e-12);

    for (const double v : {-0.5, -3.0}) {
        const auto behind = intersectSurfaces(planeAt({0., 0., v}, {0., 0., 1.}), cone);
        EXPECT_EQ(behind.kind, SurfaceIntersectionKind::None) << "v=" << v;
    }
}

// A plane holding the apex and the axis slices the cone along the two rulings in it.
TEST(BRepConeSection, PlaneThroughTheApexAndAxisCutsTwoRulings)
{
    const double slope = 0.5;
    // the y = 0 plane contains the origin (apex) and the Z axis
    const auto si = intersectSurfaces(planeAt({0., 0., 0.}, {0., 1., 0.}), coneSurface(slope));
    ASSERT_EQ(si.kind, SurfaceIntersectionKind::TwoLines);

    // both rulings start at the apex and rise at the cone's slope
    for (const Curve* c : {&si.curve, &si.curve2}) {
        EXPECT_NEAR(c->origin.x, 0.0, 1e-12);
        EXPECT_NEAR(c->origin.y, 0.0, 1e-12);
        EXPECT_NEAR(c->origin.z, 0.0, 1e-12);
        EXPECT_NEAR(std::abs(c->dir.y), 0.0, 1e-12) << "a ruling in y=0 cannot leave it";
        ASSERT_GT(std::abs(c->dir.z), 1e-9);
        EXPECT_NEAR(std::abs(c->dir.x / c->dir.z), slope, 1e-9)
            << "the ruling does not rise at the cone's slope";
    }
    // and they are the two DIFFERENT rulings, not one reported twice
    EXPECT_GT(std::abs(si.curve.dir.x - si.curve2.dir.x), 1e-9);
}

// THE SCOPE BOUNDARY, asserted on purpose. An oblique plane cuts a conic that is neither a
// Line nor a Circle; reporting one anyway would put geometry into a body that does not lie
// on its own surface.
TEST(BRepConeSection, ObliqueAndAxisParallelPlanesStayUnsupported)
{
    const Surface cone = coneSurface(0.25);
    // tilted: an ellipse
    EXPECT_EQ(intersectSurfaces(planeAt({0., 0., 2.}, {0.3, 0., 1.}), cone).kind,
              SurfaceIntersectionKind::Unsupported);
    // parallel to the axis but missing the apex: a hyperbola
    EXPECT_EQ(intersectSurfaces(planeAt({1., 0., 0.}, {1., 0., 0.}), cone).kind,
              SurfaceIntersectionKind::Unsupported);
    // still out of scope: cone against anything curved
    Surface sph;
    sph.kind = SurfaceKind::Sphere;
    sph.origin = {0., 0., 2.};
    sph.radius = 1.0;
    EXPECT_EQ(intersectSurfaces(cone, sph).kind, SurfaceIntersectionKind::Unsupported);
    EXPECT_EQ(intersectSurfaces(cone, cone).kind, SurfaceIntersectionKind::Unsupported);
}

// What the seam unblocks: a cone can now be combined with a box. Before this, the imprint
// left both bodies untouched and every operator returned empty.
TEST(BRepConeSection, BoxAgainstConeSewsAndConservesVolume)
{
    struct Case { double r, dx; };
    const Case cases[] = {{0.6, 0.0}, {0.6, 0.4}, {0.3, 0.0}, {0.3, 0.4}};
    for (const Case& c : cases) {
        const Body A = makeBox(2.f, 2.f, 2.f);
        Body B = makeCone(static_cast<float>(c.r), 3.f, 16);
        B.translate({c.dx, 0., 0.});

        const Body U = booleanToBody(A, B, BooleanOp::Union);
        const Body I = booleanToBody(A, B, BooleanOp::Intersection);
        const Body D = booleanToBody(A, B, BooleanOp::Difference);
        ASSERT_TRUE(solid(U)) << "r=" << c.r << " dx=" << c.dx << " union";
        ASSERT_TRUE(solid(I)) << "r=" << c.r << " dx=" << c.dx << " intersection";
        ASSERT_TRUE(solid(D)) << "r=" << c.r << " dx=" << c.dx << " difference";

        Body Ai = A, Bi = B;
        ASSERT_TRUE(imprintMutually(Ai, Bi));
        EXPECT_GT(Bi.faceCount(), B.faceCount())
            << "r=" << c.r << " dx=" << c.dx << ": the cone was never cut";
        for (const uint32_t sub : {0u, 2u}) {
            EXPECT_NEAR(meshVolume(U, sub) + meshVolume(I, sub),
                        meshVolume(Ai, sub) + meshVolume(Bi, sub),
                        1e-6 * (meshVolume(Ai, sub) + meshVolume(Bi, sub)))
                << "r=" << c.r << " dx=" << c.dx << " sub" << sub << ": U+I != A+B";
            EXPECT_NEAR(meshVolume(D, sub) + meshVolume(I, sub), meshVolume(Ai, sub),
                        1e-6 * meshVolume(Ai, sub))
                << "r=" << c.r << " dx=" << c.dx << " sub" << sub << ": D+I != A";
        }
    }
}

// The intersection is a FRUSTUM and must have a frustum's volume — bounded below by the
// faceted solid the kernel actually stores and above by the smooth cone it approximates.
// A topological check would accept any closed shape here.
TEST(BRepConeSection, TheIntersectionIsAFrustumOfTheRightSize)
{
    const double coneR = 0.6, coneH = 3.0;
    const Body A = makeBox(2.f, 2.f, 2.f);
    const Body B = makeCone(static_cast<float>(coneR), static_cast<float>(coneH), 16);
    const Body I = booleanToBody(A, B, BooleanOp::Intersection);
    ASSERT_TRUE(solid(I));

    // makeCone puts the apex at +h/2 and the base at -h/2; the box keeps |z| <= 1
    const double slope = coneR / coneH;
    const double rTop = slope * (coneH * 0.5 - 1.0);   // ring at z = +1
    const double rBot = slope * (coneH * 0.5 + 1.0);   // ring at z = -1
    const double smooth = kPi * 2.0 / 3.0 * (rTop * rTop + rTop * rBot + rBot * rBot);
    // the same frustum on 16-gon cross-sections
    const double k = 0.5 * 16.0 * std::sin(2.0 * kPi / 16.0);  // n-gon area / r^2
    const double faceted =
        2.0 / 3.0 * (k * rTop * rTop + std::sqrt(k * rTop * rTop * k * rBot * rBot) +
                     k * rBot * rBot);

    const double got = meshVolume(I, 2);
    EXPECT_GT(got, faceted * 0.999) << "less than the faceted frustum: " << got;
    EXPECT_LT(got, smooth * 1.001) << "more than the true frustum: " << got;
}

// The imprint must accept the cone's OWN rings and refuse circles that merely look like
// them — the radius at one height is wrong at any other, which is the whole difference
// between a cone and a cylinder.
TEST(BRepConeSection, OnlyRingsActuallyOnTheConeAreImprinted)
{
    const Body base = makeCone(0.6f, 3.f, 16);
    const double slope = 0.6 / 3.0;
    const double apexZ = 1.5;

    auto ringAt = [&](double z, double radius) {
        Curve c;
        c.kind = CurveKind::Circle;
        c.origin = {0., 0., z};
        c.dir = {0., 0., 1.};
        c.ref = {1., 0., 0.};
        c.radius = radius;
        return c;
    };

    // the true ring at z = 0 has radius slope * (apexZ - 0)
    const double trueR = slope * apexZ;
    bool acceptedTrue = false;
    for (uint32_t f = 0; f < static_cast<uint32_t>(base.faceCount()); ++f) {
        Body b = base;
        if (b.imprintCurve(f, ringAt(0.0, trueR)) != kInvalid) acceptedTrue = true;
    }
    EXPECT_TRUE(acceptedTrue) << "the cone's own ring at z=0 was refused everywhere";

    // the radius that would be right for a CYLINDER of the cone's base radius is wrong here
    for (const double bogus : {0.6, trueR * 0.5, trueR * 1.5}) {
        if (std::abs(bogus - trueR) < 1e-9) continue;
        bool accepted = false;
        for (uint32_t f = 0; f < static_cast<uint32_t>(base.faceCount()); ++f) {
            Body b = base;
            if (b.imprintCurve(f, ringAt(0.0, bogus)) != kInvalid) accepted = true;
        }
        EXPECT_FALSE(accepted) << "a ring of radius " << bogus << " at z=0 was imprinted, but the "
                               << "cone's radius there is " << trueR;
    }
}

}  // namespace nexus::geometry::brep::testing
