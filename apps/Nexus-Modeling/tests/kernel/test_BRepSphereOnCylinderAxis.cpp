// A ball on a rod — and a sample point that was never on its own face.
//
// PART ONE, the capability. A sphere whose centre lies ON a cylinder's axis makes the whole
// configuration a surface of revolution about that axis, so the meeting must be one too:
// RINGS, not a general space curve. Writing rc for the cylinder's radius, R for the
// sphere's and z for the axial offset from its centre, a common point satisfies
// rc² + z² = R² — two rings at z = ±sqrt(R² − rc²), each of radius rc, one either side of
// the centre. They merge into one where the sphere is exactly inscribed in the bore, and
// there is nothing at all when the sphere is narrower than it. Move the centre off the axis
// and the symmetry is gone: the meeting is a quartic and is still declined.
//
// That needed a new intersection kind. TwoLines already existed for a plane cutting a
// cylinder along two rulings; TwoCircles is its curved counterpart, and both branches are
// imprinted rather than the first that succeeds, since one face can be crossed by both
// rings of a sphere spanning the bore.
//
// PART TWO, which part one exposed and which is not about spheres or cylinders. With the
// rings arriving, some radii sewed and others returned empty, in bands — which looked like
// a property of the radius and was not.
//
// A CURVED face was sampled at its outline's centroid, and the centroid of a ring of points
// on a curved surface does not lie on that surface: it sags inside by the chord-versus-arc
// difference. The one point that decides how a whole face is classified was a point the
// face does not contain — the same mistake the holed-planar case was fixed for, in
// different clothes.
//
// MEASURED on a sphere of radius 1.10 against a cylinder of radius 1: all 120 of the
// sphere's face samples sat at |p| = 1.0735 instead of 1.10, and for the 24 narrow faces
// between the seam and the neighbouring grid latitude the sag moved the sample from radius
// 1.008 — outside the cylinder, where that face's material is — to 0.978, inside it. All 24
// were classified Inside, dropped from the union, and left 72 boundary edges one-sided. At
// radius 1.30 the same sag flips nothing, which is exactly why neighbouring configurations
// worked and disguised the cause.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/BRepSurfaceIntersect.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

namespace {

Surface cylSurface(double r)
{
    Surface s;
    s.kind = SurfaceKind::Cylinder;
    s.origin = {0., 0., 0.};
    s.normal = {0., 0., 1.};
    s.uAxis = {1., 0., 0.};
    s.radius = r;
    return s;
}

Surface sphSurface(const Vec3& c, double r)
{
    Surface s;
    s.kind = SurfaceKind::Sphere;
    s.origin = c;
    s.radius = r;
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

// The rings, checked to lie on BOTH surfaces — a Circle of the right kind in the wrong
// place passes a type check and nothing else.
TEST(BRepSphereOnCylinderAxis, TwoRingsLieOnBothSurfacesAtTheRightHeights)
{
    const double rc = 1.0, R = 1.5;
    const auto si = intersectSurfaces(sphSurface({0., 0., 0.5}, R), cylSurface(rc));
    ASSERT_EQ(si.kind, SurfaceIntersectionKind::TwoCircles);

    const double z = std::sqrt(R * R - rc * rc);
    for (const Curve* c : {&si.curve, &si.curve2}) {
        EXPECT_NEAR(c->radius, rc, 1e-12) << "a ring on the cylinder has the cylinder's radius";
        EXPECT_NEAR(std::abs(c->dir.z), 1.0, 1e-12) << "rings are perpendicular to the axis";
        EXPECT_NEAR(c->origin.x, 0.0, 1e-12);
        EXPECT_NEAR(c->origin.y, 0.0, 1e-12);
        // and every point of the ring is exactly R from the sphere's centre
        const double dz = c->origin.z - 0.5;
        EXPECT_NEAR(std::sqrt(rc * rc + dz * dz), R, 1e-12)
            << "the ring does not lie on the sphere";
    }
    EXPECT_NEAR(si.curve.origin.z + si.curve2.origin.z, 1.0, 1e-12)
        << "the two rings straddle the sphere's centre";
    EXPECT_NEAR(std::abs(si.curve.origin.z - si.curve2.origin.z), 2.0 * z, 1e-12);
}

// The degenerate ends of the family, each named rather than lumped together.
TEST(BRepSphereOnCylinderAxis, InscribedIsOneRingNarrowerIsNothingOffAxisIsDeclined)
{
    using K = SurfaceIntersectionKind;
    EXPECT_EQ(intersectSurfaces(sphSurface({0., 0., 0.}, 1.0), cylSurface(1.0)).kind, K::Circle)
        << "a sphere inscribed in the bore touches it along ONE ring, reported once";
    EXPECT_EQ(intersectSurfaces(sphSurface({0., 0., 0.}, 0.6), cylSurface(1.0)).kind, K::None)
        << "a sphere narrower than the bore never reaches it";
    EXPECT_EQ(intersectSurfaces(sphSurface({0.4, 0., 0.}, 1.5), cylSurface(1.0)).kind,
              K::Unsupported)
        << "off the axis the meeting is a quartic and must be declined, not approximated";
    // the argument order must not matter
    EXPECT_EQ(intersectSurfaces(cylSurface(1.0), sphSurface({0., 0., 0.}, 1.5)).kind,
              K::TwoCircles);
}

// PART TWO asserted directly: a curved face's sample point must lie ON that face's surface.
// This is the property that was silently false, and it is invisible in any boolean result.
TEST(BRepSphereOnCylinderAxis, CurvedFaceSamplePointsLieOnTheirSurface)
{
    Body cyl = makeCylinder(1.f, 4.f, 16);
    Body sph = makeSphere(1.1f, 8, 12);
    ASSERT_TRUE(imprintMutually(cyl, sph));

    int checked = 0;
    for (uint32_t f = 0; f < static_cast<uint32_t>(sph.faceCount()); ++f) {
        if (!sph.face(f).alive) continue;
        const Vec3 s = sph.faceSamplePoint(f);
        // against the surface's OWN stored radius, not a literal: makeSphere takes a float,
        // so the body's radius is 1.10000002... and a double 1.1 differs from it by 2.4e-08
        const double R = sph.surface(sph.face(f).surface).radius;
        EXPECT_NEAR(std::sqrt(s.x * s.x + s.y * s.y + s.z * s.z), R, 1e-9)
            << "sphere face " << f << ": the sample point is not on the sphere";
        ++checked;
    }
    EXPECT_GT(checked, 0);

    // and the consequence: a face whose material is entirely outside the cylinder must not
    // classify Inside it
    int misread = 0;
    for (uint32_t f = 0; f < static_cast<uint32_t>(sph.faceCount()); ++f) {
        if (!sph.face(f).alive) continue;
        double lo = 1e9;
        for (const uint32_t v : sph.faceVertices(f)) {
            const Vec3 p = sph.vertex(v).point;
            lo = std::min(lo, std::sqrt(p.x * p.x + p.y * p.y));
        }
        if (lo < 1.0 - 1e-9) continue;  // not wholly outside the bore
        if (sph.classifyFace(f, cyl) == Body::PointContainment::Inside) ++misread;
    }
    EXPECT_EQ(misread, 0)
        << misread << " faces lying entirely outside the cylinder were classified Inside it";
}

// The booleans the two together unblock, swept across radius so the result cannot depend
// on one lucky fixture, with both conservation identities.
TEST(BRepSphereOnCylinderAxis, BallOnRodBooleansSewAndConserveVolume)
{
    int sewed = 0;
    for (const double R : {1.1, 1.2, 1.3, 1.5, 1.7, 1.9}) {
        const Body A = makeCylinder(1.f, 4.f, 16);
        const Body B = makeSphere(static_cast<float>(R), 8, 12);
        const Body U = booleanToBody(A, B, BooleanOp::Union);
        const Body I = booleanToBody(A, B, BooleanOp::Intersection);
        const Body D = booleanToBody(A, B, BooleanOp::Difference);
        ASSERT_TRUE(solid(U)) << "R=" << R << " union";
        ASSERT_TRUE(solid(I)) << "R=" << R << " intersection";
        ASSERT_TRUE(solid(D)) << "R=" << R << " difference";
        ++sewed;

        Body Ai = A, Bi = B;
        ASSERT_TRUE(imprintMutually(Ai, Bi));
        for (const uint32_t sub : {0u, 2u}) {
            EXPECT_NEAR(meshVolume(U, sub) + meshVolume(I, sub),
                        meshVolume(Ai, sub) + meshVolume(Bi, sub),
                        1e-6 * (meshVolume(Ai, sub) + meshVolume(Bi, sub)))
                << "R=" << R << " sub" << sub << ": U+I != A+B";
            EXPECT_NEAR(meshVolume(D, sub) + meshVolume(I, sub), meshVolume(Ai, sub),
                        1e-6 * meshVolume(Ai, sub))
                << "R=" << R << " sub" << sub << ": D+I != A";
        }
    }
    EXPECT_EQ(sewed, 6);
}

// The intersection of a rod with a ball centred on its axis is a cylindrical core capped at
// each end by a spherical dome — the cylinder binds where it is the narrower of the two,
// the sphere binds beyond that. Its volume has a closed form, so it is checked against that
// rather than against another part of the kernel.
//
// (Written the other way round first — the sphere with two caps sliced off — which is the
// region inside the sphere and OUTSIDE the cylinder's radius near the equator, a different
// shape entirely. The measurement said 8.13 against a predicted 12.88 and the formula was
// what was wrong.)
TEST(BRepSphereOnCylinderAxis, TheIntersectionIsACylindricalCoreWithSphericalCaps)
{
    const double rc = 1.0, R = 1.5;
    const Body A = makeCylinder(static_cast<float>(rc), 6.f, 16);  // tall enough not to clip
    const Body B = makeSphere(static_cast<float>(R), 16, 24);
    const Body I = booleanToBody(A, B, BooleanOp::Intersection);
    ASSERT_TRUE(solid(I));

    // a cylinder of radius rc between z = ±sqrt(R²−rc²), plus a spherical cap at each end
    const double z = std::sqrt(R * R - rc * rc);
    const double capH = R - z;
    const double kPi = 3.14159265358979323846;
    const double smooth = kPi * rc * rc * (2.0 * z) +
                          2.0 * (kPi * capH * capH * (3.0 * R - capH) / 3.0);

    const double got = meshVolume(I, 2);
    // the kernel stores a 16-gon cylinder and a faceted sphere, so the solid it holds is a
    // little smaller than the smooth one it approximates
    EXPECT_GT(got, smooth * 0.95) << "far below the capped core: " << got << " vs " << smooth;
    EXPECT_LT(got, smooth * 1.005) << "above the true capped core: " << got << " vs " << smooth;
}

// CHARACTERIZATION. Two near-degenerate bands still return cleanly empty, and they are
// pinned so the boundary is known rather than discovered again: a sphere only just wider
// than the bore (approaching the inscribed tangency), and one whose ring lands within a
// degree or so of the sphere's own grid latitude. Both are the near-coincidence family
// already characterized elsewhere, not a new kind of failure.
TEST(BRepSphereOnCylinderAxis, NearDegenerateRadiiStillReturnCleanEmptyResults)
{
    for (const double R : {1.05, 1.45}) {
        const Body A = makeCylinder(1.f, 4.f, 16);
        const Body B = makeSphere(static_cast<float>(R), 8, 12);
        for (const BooleanOp op :
             {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
            const Body r = booleanToBody(A, B, op);
            EXPECT_TRUE(r.faceCount() == 0u || (r.isClosed() && r.checkIntegrity().ok))
                << "R=" << R << ": neither watertight nor empty";
        }
    }
}

}  // namespace nexus::geometry::brep::testing
