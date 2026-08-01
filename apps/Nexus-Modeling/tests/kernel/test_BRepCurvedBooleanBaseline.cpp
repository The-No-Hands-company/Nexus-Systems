// Phase 0 of the true-analytic-curved-boolean arc — a safety net that LOCKS the
// current behavior so the later phases (curve-carrying assembly, curved-face
// imprint, driver wiring, curved-seam sew) cannot silently regress it.
//
// Today the analytic B-rep boolean is planar-BY-DESIGN: imprintOneWay imprints
// only Line seams (plane∩plane); the Circle seams a curved surface produces
// (plane∩sphere, plane∩cylinder-perp, sphere∩sphere) are skipped, so the curved
// faces stay straddling, the sew opens, and booleanToBody returns a clean EMPTY
// body under its watertight-or-empty contract. These tests pin four things:
//
//   1. WatertightOrEmpty invariant (PERMANENT) — a curved boolean is never a
//      leaky/corrupt body; it is either empty or a valid closed solid.
//   2. All three curved pairs — box/sphere, box/cylinder, sphere/sphere — SEW. This
//      entry began life pinning them as empty; every fixture has flipped, so it now
//      guards against regression instead.
//   3. Genuinely out-of-scope surface pairs stay Unsupported and bail to empty
//      (never leaky). The membership of that set has SHRUNK as the pairwise table
//      filled in — sphere∩cyl and then the three cone pairs each moved in for their
//      axially-symmetric case — so what is permanent is the CONTRACT, not the list.
//      The line every curved pair now falls on: axially symmetric ⇒ a real circle;
//      anything else ⇒ a quartic, still declined, and declined out loud.
//   4. A high-facet curved pair does not blow up / hang — the imprint face-budget
//      cap bounds it and the call returns, honoring watertight-or-empty.

#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/BRepSurfaceIntersect.h>

#include <gtest/gtest.h>

namespace nexus::geometry::brep::testing {


namespace {

bool watertightOrEmpty(const Body& b)
{
    return b.faceCount() == 0u || (b.isClosed() && b.checkIntegrity().ok);
}

Body sphereAt(float r, uint32_t lat, uint32_t lon, Vec3 t)
{
    Body b = makeSphere(r, lat, lon);
    b.translate(t);
    return b;
}

Body cylinderAt(float r, float h, uint32_t seg, Vec3 t)
{
    Body b = makeCylinder(r, h, seg);
    b.translate(t);
    return b;
}

Surface cylinderSurface(Vec3 axisPoint, Vec3 axis, float radius)
{
    Surface s;
    s.kind = SurfaceKind::Cylinder;
    s.origin = axisPoint;
    s.normal = axis;
    s.radius = radius;
    return s;
}

Surface sphereSurface(Vec3 centre, float radius)
{
    Surface s;
    s.kind = SurfaceKind::Sphere;
    s.origin = centre;
    s.radius = radius;
    return s;
}

Surface coneSurface(Vec3 apex, Vec3 axis, float slope)
{
    Surface s;
    s.kind = SurfaceKind::Cone;
    s.origin = apex;
    s.normal = axis;
    s.radius = slope;
    return s;
}

}  // namespace

// (1) PERMANENT: a curved boolean is watertight or empty, never leaky.
TEST(CurvedBooleanBaseline, WatertightOrEmptyInvariantHolds)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    const Body sph = sphereAt(1.2f, 8, 12, {0.7f, 0.f, 0.f});
    const Body cyl = cylinderAt(1.f, 4.f, 16, {0.5f, 0.f, 0.f});
    const Body sphA = makeSphere(1.2f, 8, 12);
    const Body sphB = sphereAt(1.2f, 8, 12, {1.0f, 0.f, 0.f});

    for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
        EXPECT_TRUE(watertightOrEmpty(booleanToBody(box, sph, op)));
        EXPECT_TRUE(watertightOrEmpty(booleanToBody(box, cyl, op)));
        EXPECT_TRUE(watertightOrEmpty(booleanToBody(sphA, sphB, op)));
    }
}

// (2) All three curved pairs this file was created to watch now SEW. The entry began as
// a characterization of what bailed to empty; every fixture in it has since flipped, so it
// is now an assertion that they stay working. The detailed correctness — volume
// conservation, seam ring closure, face segmentation — lives in the increments that landed
// each one: test_BRepSphereFaceImprint, test_BRepArcComplementSelection,
// test_BRepMultiCrossingBite and test_BRepVolumeConservation.
TEST(CurvedBooleanBaseline, CurvedPairsSewWatertight)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    const Body sph = sphereAt(1.2f, 8, 12, {0.7f, 0.f, 0.f});
    const Body cyl = cylinderAt(1.f, 4.f, 16, {0.5f, 0.f, 0.f});
    const Body sphA = makeSphere(1.2f, 8, 12);
    const Body sphB = sphereAt(1.2f, 8, 12, {1.0f, 0.f, 0.f});

    struct Pair { const char* name; const Body* a; const Body* b; };
    const Pair pairs[] = {{"offset box/sphere", &box, &sph},
                          {"offset box/cylinder", &box, &cyl},
                          {"sphere/sphere", &sphA, &sphB}};

    for (const Pair& p : pairs) {
        for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection}) {
            const Body r = booleanToBody(*p.a, *p.b, op);
            EXPECT_GT(r.faceCount(), 0u)
                << p.name << " op " << static_cast<int>(op) << " regressed to empty";
            EXPECT_TRUE(r.isClosed()) << p.name << " op " << static_cast<int>(op);
            EXPECT_TRUE(r.checkIntegrity().ok) << p.name << " op " << static_cast<int>(op);
        }
    }

    // Difference sews for two of the three. Named individually rather than looped, so the
    // one that does not is stated rather than averaged away.
    for (const Pair& p : {pairs[0], pairs[2]}) {
        const Body r = booleanToBody(*p.a, *p.b, BooleanOp::Difference);
        EXPECT_GT(r.faceCount(), 0u) << p.name << " difference regressed to empty";
        EXPECT_TRUE(r.isClosed()) << p.name << " difference";
    }

    // REMAINING GAP: the offset cylinder's difference is the last empty result among these
    // fixtures. Union and intersection of the same pair both sew, so the segmentation is
    // right and it is the difference's own selection or sew that fails.
    EXPECT_EQ(booleanToBody(box, cyl, BooleanOp::Difference).faceCount(), 0u)
        << "offset box/cylinder difference now sews — verify it is watertight and conserves "
           "volume (D + I == A), then move it up beside the other two";
}

// (3a) PERMANENT: the Circle-seam v1 scope is plane∩sphere, plane∩cylinder-perp,
// sphere∩sphere. Every other surface pair must stay Unsupported.
TEST(CurvedBooleanBaseline, OutOfScopeSurfacePairsStayUnsupported)
{
    const Surface cyl = cylinderSurface({0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, 1.f);
    const Surface cyl2 = cylinderSurface({0.f, 0.f, 0.f}, {1.f, 0.f, 0.f}, 1.f);
    const Surface sph = sphereSurface({0.f, 0.f, 0.f}, 1.2f);
    const Surface cone = coneSurface({0.f, 0.f, 0.f}, {0.f, 0.f, 1.f}, 0.5f);

    using SIK = SurfaceIntersectionKind;
    // Genuinely out of scope: a quartic space curve, not a Line or Circle.
    EXPECT_EQ(intersectSurfaces(cyl, cyl2).kind, SIK::Unsupported);  // crossing axes
    // Two coincident cones are the SAME surface, which has no intersection CURVE.
    EXPECT_EQ(intersectSurfaces(cone, cone).kind, SIK::Unsupported);

    // Every curved pair here follows one boundary: the section is a genuine circle
    // exactly when the configuration is axially symmetric, and a quartic otherwise.
    // The cone pairs used to be declined on BOTH sides of that line, which is why a
    // cone unioned with a coaxial rod came back empty.
    EXPECT_EQ(intersectSurfaces(sph, cyl).kind, SIK::TwoCircles) << "centre on the axis";
    const Surface offAxis = sphereSurface({0.4f, 0.f, 0.f}, 1.2f);
    EXPECT_EQ(intersectSurfaces(offAxis, cyl).kind, SIK::Unsupported) << "centre off the axis";

    // cone∩sphere: `sph` is centred at the origin, which is this cone's APEX and so on
    // its axis — the sphere cuts the nappe in one ring (the second root is behind the
    // apex and is not on the surface). Moved off the axis it is a quartic again.
    EXPECT_EQ(intersectSurfaces(cone, sph).kind, SIK::Circle) << "centre on the axis";
    EXPECT_EQ(intersectSurfaces(cone, sphereSurface({0.4f, 0.f, 0.5f}, 1.2f)).kind,
              SIK::Unsupported)
        << "centre off the axis";

    // cone∩cylinder: coaxial is the ring where the nappe reaches the bore's radius.
    EXPECT_EQ(intersectSurfaces(cone, cyl).kind, SIK::Circle) << "coaxial";
    EXPECT_EQ(intersectSurfaces(cone, cyl2).kind, SIK::Unsupported) << "crossing axes";
}

// (3b) PERMANENT: solids whose overlap would need an unsupported seam bail to
// empty (never leaky) — the watertight-or-empty contract absorbs the gap.
TEST(CurvedBooleanBaseline, OutOfScopeSolidPairsBailToEmpty)
{
    const Body cone = [] {
        Body c = makeCone(1.f, 2.f, 16);
        c.translate({0.3f, 0.f, 0.f});
        return c;
    }();
    const Body box = makeBox(2.f, 2.f, 2.f);
    const Body cylA = makeCylinder(1.f, 4.f, 16);
    const Body cylB = cylinderAt(1.f, 4.f, 16, {0.6f, 0.f, 0.f});

    for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
        EXPECT_TRUE(watertightOrEmpty(booleanToBody(box, cone, op)));
        EXPECT_TRUE(watertightOrEmpty(booleanToBody(cylA, cylB, op)));
    }
}

// (4) PERMANENT: a high-facet curved pair must not explode the imprint or hang —
// the imprintOneWay face-budget cap bounds the work; the call returns and the
// result honors watertight-or-empty.
TEST(CurvedBooleanBaseline, NoBlowUpOnHighFacetCurvedInputs)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    const Body sph = sphereAt(1.6f, 16, 24, {0.5f, 0.f, 0.f});  // ~360 curved faces
    const Body r = booleanToBody(box, sph, BooleanOp::Union);
    EXPECT_TRUE(watertightOrEmpty(r));
}


// A closed seam is not the same as a CORRECT seam: booleanToBody's watertight-or-empty
// contract says nothing about where the ring landed. These check the resulting solid's
// volume against an integral worked out by hand, which shares no code with the kernel.
//
// cone(baseR=1, h=2) has its apex at z=+1 and its base at z=−1, so its radius at height
// z is (1−z)/2 and its volume is πr²h/3. A coaxial rod of radius 0.3 meets the nappe
// where (1−z)/2 = 0.3, i.e. z = 0.4 — the ring the cone∩cylinder pair returns.
TEST(CurvedBooleanBaseline, ConeMinusCoaxialRodHasTheHandIntegratedVolume)
{
    const Body r = booleanToBody(makeCone(1.f, 2.f, 16), makeCylinder(0.3f, 6.f, 16),
                                 BooleanOp::Difference);
    ASSERT_GT(r.faceCount(), 0u) << "cone − coaxial rod returned EMPTY";
    EXPECT_TRUE(r.isClosed());
    EXPECT_TRUE(r.checkIntegrity().ok);
    EXPECT_TRUE(r.checkGeometry().ok);

    // π∫₋₁^0.4 ((1−z)²/4 − 0.09) dz, substituting u = 1−z:
    // π[u³/12 − 0.09u] from 0.6 to 2 = π(0.486667 + 0.036) = π·0.5226667.
    const double expected = 3.14159265358979324 * 0.5226666666666667;
    EXPECT_NEAR(r.massProperties().volume, expected, expected * 1e-5);
}

TEST(CurvedBooleanBaseline, ConePlusCoaxialRodHasTheHandIntegratedVolume)
{
    const Body r = booleanToBody(makeCone(1.f, 2.f, 16), makeCylinder(0.3f, 6.f, 16),
                                 BooleanOp::Union);
    ASSERT_GT(r.faceCount(), 0u) << "cone ∪ coaxial rod returned EMPTY";
    EXPECT_TRUE(r.isClosed());
    EXPECT_TRUE(r.checkGeometry().ok);

    // cone + rod − (rod ∩ cone). Below z = 0.4 the nappe is wider than the rod, so the
    // rod's disc is wholly inside it; above z = 0.4 the cone is the narrower of the two.
    const double PI = 3.14159265358979324;
    const double expected = PI * 2.0 / 3.0        // cone: πr²h/3
                            + PI * 0.09 * 6.0     // rod
                            - PI * 0.144;         // overlap: 0.09·1.4 + ∫₀.₄¹((1−z)/2)²dz
    EXPECT_NEAR(r.massProperties().volume, expected, expected * 1e-5);
}

TEST(CurvedBooleanBaseline, TwoCoaxialConesOfDifferentSlopeUnionAcrossTheirCrossingRing)
{
    // The slopes have to DIFFER for this to exercise cone∩cone at all: two nappes of the
    // same slope on one axis are parallel and never meet, so an earlier version of this
    // test — identical cones, one lifted — passed just as well with the cone pairs removed
    // from the dispatch. It was measuring plane∩cone and nothing else.
    //
    // a: apex z=+1, base z=−1, radius (1−z)/2  (slope 0.5)
    // b: apex z=+0.5, base z=−1.5, radius 0.75·(0.5−z)  (slope 0.75)
    // They agree at z = −0.5, both 0.75 — the ring cone∩cone returns.
    Body b = makeCone(1.5f, 2.f, 16);
    b.translate({0.0, 0.0, -0.5});
    const Body r = booleanToBody(makeCone(1.f, 2.f, 16), b, BooleanOp::Union);
    ASSERT_GT(r.faceCount(), 0u) << "cone ∪ steeper coaxial cone returned EMPTY";
    EXPECT_TRUE(r.isClosed());
    EXPECT_TRUE(r.checkGeometry().ok);

    // π∫max(rA,rB)² dz, split at the crossing: a is wider above z=−0.5 and b below it.
    // 0.0104167 + 0.2708333 + 0.4453125 + 0.8671875 = 1.59375, and the same number comes
    // out of A + B − A∩B = 0.666667 + 1.5 − 0.5729167.
    const double PI = 3.14159265358979324;
    const double expected = PI * 1.59375;
    EXPECT_NEAR(r.massProperties().volume, expected, expected * 1e-5);
}

TEST(CurvedBooleanBaseline, ConeMinusOnAxisSphereHasTheHandIntegratedVolume)
{
    Body s = makeSphere(0.5f, 8, 12);
    s.translate({0.0, 0.0, 0.6});
    const Body r = booleanToBody(makeCone(1.f, 2.f, 16), s, BooleanOp::Difference);
    ASSERT_GT(r.faceCount(), 0u) << "cone − on-axis sphere returned EMPTY";
    EXPECT_TRUE(r.isClosed());
    EXPECT_TRUE(r.checkGeometry().ok);

    // The removed volume is π∫ min(coneR, sphereR)² dz over z ∈ [0.1, 1]. The two radii
    // cross where 1.25z² − 1.7z + 0.36 = 0, i.e. z = 0.26237; the sphere is narrower
    // below it and the cone above. Evaluating both halves gives 0.0452011·π.
    const double PI = 3.14159265358979324;
    const double expected = PI * 2.0 / 3.0 - PI * 0.0452011;
    EXPECT_NEAR(r.massProperties().volume, expected, expected * 1e-4);
}

}  // namespace nexus::geometry::brep::testing
