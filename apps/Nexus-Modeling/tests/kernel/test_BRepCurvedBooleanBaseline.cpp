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
//   2. Which curved pairs sew (CHARACTERIZATION). sphere/sphere does, and so does
//      CONCENTRIC box/sphere; the OFFSET box/sphere and box/cylinder fixtures here
//      still bail to empty and are expected to FLIP in turn; the messages say so.
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

// (2) CHARACTERIZATION: which curved pairs sew today. sphere/sphere does, and so does
// box/sphere when the two are CONCENTRIC — the fixture below is deliberately OFFSET
// (dx = 0.7), which does not sew yet. The distinction matters: reading this entry as
// "box/sphere is empty" is now wrong, and the centred case is asserted positively in
// test_BRepArcComplementSelection.cpp. box/cylinder remains empty at any offset.
TEST(CurvedBooleanBaseline, CurvedPairsCurrentlyBailToEmpty)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    const Body sph = sphereAt(1.2f, 8, 12, {0.7f, 0.f, 0.f});
    const Body cyl = cylinderAt(1.f, 4.f, 16, {0.5f, 0.f, 0.f});
    const Body sphA = makeSphere(1.2f, 8, 12);
    const Body sphB = sphereAt(1.2f, 8, 12, {1.0f, 0.f, 0.f});

    EXPECT_EQ(booleanToBody(box, sph, BooleanOp::Union).faceCount(), 0u)
        << "an OFFSET box/sphere union is now non-empty — verify it is watertight and "
           "conserves volume, then move it beside the centred case";
    EXPECT_EQ(booleanToBody(box, cyl, BooleanOp::Union).faceCount(), 0u)
        << "curved box/cylinder union is now non-empty — update this baseline";

    // sphere/sphere HAS flipped: once a Circle seam could be imprinted onto a spherical
    // face and the seam closed into a ring, this pair sews. Kept here as the positive
    // half of the same characterization so the entry stays honest about which curved
    // pairs work — the full assertions (watertight, inclusion-exclusion) live in
    // test_BRepSphereFaceImprint.cpp.
    const Body ssU = booleanToBody(sphA, sphB, BooleanOp::Union);
    EXPECT_GT(ssU.faceCount(), 0u) << "sphere/sphere union regressed to empty";
    EXPECT_TRUE(ssU.isClosed());
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
    EXPECT_EQ(intersectSurfaces(cyl, cyl2).kind, SIK::Unsupported);   // cylinder∩cylinder
    EXPECT_EQ(intersectSurfaces(sph, cyl).kind, SIK::Unsupported);    // sphere∩cylinder
    EXPECT_EQ(intersectSurfaces(cone, sph).kind, SIK::Unsupported);   // cone∩sphere
    EXPECT_EQ(intersectSurfaces(cone, cone).kind, SIK::Unsupported);  // cone∩cone
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
