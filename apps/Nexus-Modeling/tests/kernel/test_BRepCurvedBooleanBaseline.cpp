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
//   3. Genuinely out-of-scope surface pairs (cyl∩cyl, sphere∩cyl, cone∩*) stay
//      Unsupported and bail to empty (PERMANENT — outside the Circle-seam v1).
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
    // Genuinely out of scope: each of these is a quartic space curve, not a Line or Circle.
    EXPECT_EQ(intersectSurfaces(cyl, cyl2).kind, SIK::Unsupported);   // crossing axes
    EXPECT_EQ(intersectSurfaces(cone, sph).kind, SIK::Unsupported);   // cone∩sphere
    EXPECT_EQ(intersectSurfaces(cone, cone).kind, SIK::Unsupported);  // cone∩cone

    // sphere∩cylinder has moved PARTLY in scope, and the boundary is where the symmetry
    // is: a sphere centred ON the axis meets the cylinder in two rings, and one centred
    // anywhere else meets it in a quartic that is still declined.
    EXPECT_EQ(intersectSurfaces(sph, cyl).kind, SIK::TwoCircles) << "centre on the axis";
    const Surface offAxis = sphereSurface({0.4f, 0.f, 0.f}, 1.2f);
    EXPECT_EQ(intersectSurfaces(offAxis, cyl).kind, SIK::Unsupported) << "centre off the axis";
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

}  // namespace nexus::geometry::brep::testing
