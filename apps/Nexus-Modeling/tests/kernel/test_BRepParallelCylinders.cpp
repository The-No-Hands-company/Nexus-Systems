// Two cylinders side by side — and a second defect that only a planar face bounded by
// ARCS could expose.
//
// PART ONE, the capability. `intersectSurfaces` refused every cylinder/cylinder pair, so
// two cylinders standing side by side imprinted nothing and all three operators returned
// empty. When the axes are PARALLEL the answer is straight: the whole problem collapses to
// two circles in the plane perpendicular to the shared direction, and each solution point
// extrudes along it into a ruling. Distance d between the axes decides everything —
// apart, externally tangent (one ruling), crossing (two), internally tangent, or one bore
// inside the other. Skew or crossing axes are a quartic space curve and stay Unsupported.
//
// PART TWO, which the capability uncovered. With the rulings arriving, the cylinders' SIDE
// walls cut correctly and their CAPS still would not: the caps stayed two faces throughout,
// every operator still returned empty, and 24 boundary edges were left one-sided.
//
// The cap is a planar face whose boundary is the cylinder's RIM — a circular arc, not a
// straight edge. So the crossing search took the arc path, which asks where a boundary arc
// crosses the imprint circle's PLANE. That is the right question when the arc is out of
// that plane, which is how it was written (for a sphere). Here the arc lies IN it, the
// formulation degenerates, and it reports nothing at all.
//
// Two coplanar circles do not meet by crossing each other's plane. They meet by ordinary
// circle-circle intersection, solved in the plane they share. That case is now handled, and
// it is not special to cylinders: it is the boundary of EVERY planar face bounded by arcs
// rather than straight edges — a cylinder's cap, a cone's base, a disk.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/BRepSurfaceIntersect.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

namespace {

Surface cylAt(double x, double r, const Vec3& axis = {0., 0., 1.})
{
    Surface s;
    s.kind = SurfaceKind::Cylinder;
    s.origin = {x, 0., 0.};
    s.normal = axis;
    s.uAxis = {1., 0., 0.};
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

// distance from a point to the line through `p` along `d`
double distToAxis(const Vec3& q, const Vec3& p, const Vec3& d)
{
    const Vec3 w{q.x - p.x, q.y - p.y, q.z - p.z};
    const double t = w.x * d.x + w.y * d.y + w.z * d.z;
    const Vec3 r{w.x - t * d.x, w.y - t * d.y, w.z - t * d.z};
    return std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
}

}  // namespace

// Every branch of the distance classification, and the rulings checked to actually lie on
// BOTH cylinders — a Line of the right kind in the wrong place would pass a type check.
TEST(BRepParallelCylinders, RulingsLieOnBothSurfacesAndTheDegenerateCasesAreNamed)
{
    using K = SurfaceIntersectionKind;
    const double rA = 1.0, rB = 0.7;

    // crossing: two rulings
    const auto two = intersectSurfaces(cylAt(0., rA), cylAt(1.0, rB));
    ASSERT_EQ(two.kind, K::TwoLines);
    for (const Curve* c : {&two.curve, &two.curve2}) {
        EXPECT_NEAR(distToAxis(c->origin, {0., 0., 0.}, {0., 0., 1.}), rA, 1e-9)
            << "ruling does not lie on the first cylinder";
        EXPECT_NEAR(distToAxis(c->origin, {1.0, 0., 0.}, {0., 0., 1.}), rB, 1e-9)
            << "ruling does not lie on the second cylinder";
        EXPECT_NEAR(std::abs(c->dir.z), 1.0, 1e-9) << "a ruling must run along the axis";
    }
    EXPECT_GT(std::abs(two.curve.origin.y - two.curve2.origin.y), 1e-6)
        << "the two rulings are the same line reported twice";

    EXPECT_EQ(intersectSurfaces(cylAt(0., rA), cylAt(rA + rB, rB)).kind, K::Line)
        << "externally tangent cylinders share exactly one ruling";
    EXPECT_EQ(intersectSurfaces(cylAt(0., rA), cylAt(rA - rB, rB)).kind, K::Line)
        << "internally tangent cylinders share exactly one ruling";
    EXPECT_EQ(intersectSurfaces(cylAt(0., rA), cylAt(3.0, rB)).kind, K::None) << "far apart";
    EXPECT_EQ(intersectSurfaces(cylAt(0., rA), cylAt(0.1, rB)).kind, K::None)
        << "one bore entirely inside the other never meets it";
    EXPECT_EQ(intersectSurfaces(cylAt(0., rA), cylAt(0., rA)).kind, K::Unsupported)
        << "the same surface twice is a whole-surface overlap, not a curve";
    EXPECT_EQ(intersectSurfaces(cylAt(0., rA), cylAt(0., rB)).kind, K::None) << "concentric";

    // axes that are not parallel remain a quartic and stay out of scope
    EXPECT_EQ(intersectSurfaces(cylAt(0., rA), cylAt(0., rB, {1., 0., 0.})).kind, K::Unsupported);
}

// PART TWO asserted on its own: the caps must be CUT. This is the property that stayed
// broken after the rulings arrived, and it is invisible in a boolean's face count.
TEST(BRepParallelCylinders, CapsBoundedByArcsAreCutByACoplanarSeamCircle)
{
    Body A = makeCylinder(1.f, 3.f, 16);
    Body B = makeCylinder(0.7f, 4.f, 16);
    B.translate({1.0, 0., 0.});

    auto planarFaces = [](const Body& b) {
        size_t n = 0;
        for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f)
            if (b.face(f).alive && b.surface(b.face(f).surface).kind == SurfaceKind::Plane) ++n;
        return n;
    };
    ASSERT_EQ(planarFaces(A), 2u) << "a cylinder starts with two caps";
    ASSERT_TRUE(imprintMutually(A, B));
    EXPECT_GT(planarFaces(A), 2u)
        << "the caps were never cut — a coplanar seam circle on an arc-bounded face was "
           "solved as a plane crossing and reported no intersection";
    EXPECT_TRUE(A.isClosed());
    EXPECT_TRUE(A.checkIntegrity().ok);
    EXPECT_TRUE(A.checkGeometry().ok);
}

// The booleans the two together unblock, with both conservation identities — a watertight
// result can still be the wrong size.
TEST(BRepParallelCylinders, ParallelCylinderBooleansSewAndConserveVolume)
{
    struct Case { double rB, hB, dx, dz; const char* what; };
    const Case cases[] = {
        {0.7, 3.0, 0.6, 0.0, "coplanar caps"},
        {0.7, 4.0, 0.6, 0.0, "differing heights"},
        {0.7, 4.0, 1.0, 0.0, "half overlapping"},
        {0.7, 4.0, 1.4, 0.0, "barely overlapping"},
        {0.7, 2.0, 0.8, 0.4, "shifted along the axis too"},
        {0.4, 4.0, 0.3, 0.0, "a small bore off centre"},
    };
    for (const Case& c : cases) {
        const Body A = makeCylinder(1.f, 3.f, 16);
        Body B = makeCylinder(static_cast<float>(c.rB), static_cast<float>(c.hB), 16);
        B.translate({c.dx, 0., c.dz});

        const Body U = booleanToBody(A, B, BooleanOp::Union);
        const Body I = booleanToBody(A, B, BooleanOp::Intersection);
        const Body D = booleanToBody(A, B, BooleanOp::Difference);
        ASSERT_TRUE(solid(U)) << c.what << ": union";
        ASSERT_TRUE(solid(I)) << c.what << ": intersection";
        ASSERT_TRUE(solid(D)) << c.what << ": difference";

        Body Ai = A, Bi = B;
        ASSERT_TRUE(imprintMutually(Ai, Bi));
        for (const uint32_t sub : {0u, 2u}) {
            EXPECT_NEAR(meshVolume(U, sub) + meshVolume(I, sub),
                        meshVolume(Ai, sub) + meshVolume(Bi, sub),
                        1e-6 * (meshVolume(Ai, sub) + meshVolume(Bi, sub)))
                << c.what << " sub" << sub << ": U+I != A+B";
            EXPECT_NEAR(meshVolume(D, sub) + meshVolume(I, sub), meshVolume(Ai, sub),
                        1e-6 * meshVolume(Ai, sub))
                << c.what << " sub" << sub << ": D+I != A";
        }
    }
}

// The intersection of two parallel cylinders is a PRISM on the lens where their circles
// overlap, so its volume is that lens area times the shared height. Checked against the
// closed form rather than against another part of the kernel.
TEST(BRepParallelCylinders, TheIntersectionIsALensPrismOfTheRightSize)
{
    const double rA = 1.0, rB = 0.7, d = 1.0, h = 3.0;  // B is taller, so h is A's
    const Body A = makeCylinder(static_cast<float>(rA), static_cast<float>(h), 16);
    Body B = makeCylinder(static_cast<float>(rB), 4.f, 16);
    B.translate({d, 0., 0.});
    const Body I = booleanToBody(A, B, BooleanOp::Intersection);
    ASSERT_TRUE(solid(I));

    // circular lens area for radii rA, rB at centre distance d
    const double t1 = (d * d + rA * rA - rB * rB) / (2.0 * d * rA);
    const double t2 = (d * d + rB * rB - rA * rA) / (2.0 * d * rB);
    const double lens =
        rA * rA * std::acos(std::min(1.0, std::max(-1.0, t1))) +
        rB * rB * std::acos(std::min(1.0, std::max(-1.0, t2))) -
        0.5 * std::sqrt((-d + rA + rB) * (d + rA - rB) * (d - rA + rB) * (d + rA + rB));
    const double smooth = lens * h;

    const double got = meshVolume(I, 2);
    // the kernel stores 16-gon cylinders, so the faceted lens is a little smaller
    EXPECT_GT(got, smooth * 0.94) << "far below the lens prism: " << got << " vs " << smooth;
    EXPECT_LT(got, smooth * 1.001) << "above the true lens prism: " << got << " vs " << smooth;
}

// Cylinders that do not meet, and one entirely inside another, must not invent a seam.
TEST(BRepParallelCylinders, NonMeetingCylindersAreLeftAlone)
{
    {   // far apart: the union is two disjoint solids, which this sew does not build, but
        // nothing may be imprinted either way
        Body A = makeCylinder(1.f, 3.f, 16);
        Body B = makeCylinder(0.5f, 3.f, 16);
        B.translate({4.0, 0., 0.});
        const size_t fa = A.faceCount(), fb = B.faceCount();
        ASSERT_TRUE(imprintMutually(A, B));
        EXPECT_EQ(A.faceCount(), fa) << "a cylinder 4 units away cut this one";
        EXPECT_EQ(B.faceCount(), fb);
    }
    {   // strictly inside in cross-section, and shorter: the bore never reaches the wall
        Body A = makeCylinder(1.f, 3.f, 16);
        Body B = makeCylinder(0.3f, 1.f, 16);
        B.translate({0.2, 0., 0.});
        ASSERT_TRUE(imprintMutually(A, B));
        EXPECT_TRUE(A.isClosed());
        EXPECT_TRUE(A.checkIntegrity().ok);
        EXPECT_TRUE(B.checkIntegrity().ok);
    }
}

}  // namespace nexus::geometry::brep::testing
