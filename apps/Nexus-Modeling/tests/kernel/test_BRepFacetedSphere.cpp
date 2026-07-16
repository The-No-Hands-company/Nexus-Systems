// Foundation — faceted sphere booleans. makeFacetedSphere builds an ALL-PLANAR
// UV-sphere polyhedron (semicircle revolved), so the existing planar boolean
// booleans it against boxes watertight, with volume converging to the smooth
// sphere as facets grow. Companion to makeFacetedCylinder. (Facet counts kept
// small: the boolean is still ~O(n³), so a faceted sphere's O(lat·lon) faces get
// expensive fast — 6×8 keeps a boolean ~0.3s.)

#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
Body boxMinMax(Vec3 lo, Vec3 hi)
{
    Body b = makeBox(hi.x - lo.x, hi.y - lo.y, hi.z - lo.z);
    b.translate({(lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, (lo.z + hi.z) * 0.5f});
    return b;
}
void expectWatertight(const Body& b)
{
    const auto ig = b.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.boundaryEdges, 0u);
    EXPECT_TRUE(b.isClosed());
    EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;
}
}  // namespace

TEST(BRepFacetedSphere, IsAllPlanarValidSolid)
{
    const Body s = makeFacetedSphere(1.f, 6, 8);
    expectWatertight(s);
    EXPECT_EQ(s.checkIntegrity().euler, 2);
    // Every face is a Plane (so the planar boolean can sew it).
    for (uint32_t f = 0; f < s.faceCount(); ++f) {
        if (!s.face(f).alive) continue;
        EXPECT_EQ(s.surface(s.face(f).surface).kind, SurfaceKind::Plane);
    }
    // A faceted sphere under-approximates 4/3·π·r³.
    const float v = s.massProperties().volume;
    EXPECT_GT(v, 3.f);
    EXPECT_LT(v, static_cast<float>(4.0 / 3.0 * M_PI));
}

TEST(BRepFacetedSphere, HemisphereBySlabIsExactHalf)
{
    // 6 latitude bands → the equator (z=0) is a band boundary, so the slab z ≥ 0
    // cuts the faceted sphere exactly in half.
    const Body s = makeFacetedSphere(1.f, 6, 8);
    const double sphereVol = s.massProperties().volume;
    const Body slab = boxMinMax({-2, -2, 0}, {2, 2, 2});

    const Body inter = booleanToBody(s, slab, BooleanOp::Intersection);
    expectWatertight(inter);
    EXPECT_NEAR(inter.massProperties().volume, static_cast<float>(sphereVol / 2.0), 1e-3f);

    const Body diff = booleanToBody(s, slab, BooleanOp::Difference);
    expectWatertight(diff);
    EXPECT_NEAR(diff.massProperties().volume, static_cast<float>(sphereVol / 2.0), 1e-3f);
}

TEST(BRepFacetedSphere, ConvergesToSmoothHemisphere)
{
    const Body slab = boxMinMax({-2, -2, 0}, {2, 2, 2});
    auto hemi = [&](uint32_t lat, uint32_t lon) {
        return static_cast<double>(
            booleanToBody(makeFacetedSphere(1.f, lat, lon), slab, BooleanOp::Intersection)
                .massProperties()
                .volume);
    };
    const double target = 2.0 / 3.0 * M_PI;  // smooth hemisphere
    const double eCoarse = std::abs(hemi(4, 6) - target);   // ≈ 0.62
    const double eFine = std::abs(hemi(6, 8) - target);     // ≈ 0.34 (coarse facets)
    EXPECT_GT(eCoarse, eFine);  // finer faceting → strictly closer to the smooth value
    EXPECT_LT(eFine, eCoarse);
    EXPECT_LT(eFine, 0.4);
}

TEST(BRepFacetedSphere, HighFacetCountIsPracticalAfterBroadPhase)
{
    // The AABB broad-phase in the imprint prunes the O(tool-faces²) slab-face
    // explosion, so a 16×24 sphere boolean (was seconds, now ~0.15s) is practical
    // and its hemisphere volume is close to the smooth 2/3·π·r³.
    const Body s = makeFacetedSphere(1.f, 16, 24);
    const Body slab = boxMinMax({-2, -2, 0}, {2, 2, 2});
    const Body inter = booleanToBody(s, slab, BooleanOp::Intersection);
    expectWatertight(inter);
    EXPECT_NEAR(inter.massProperties().volume, static_cast<float>(2.0 / 3.0 * M_PI), 0.06f);
}

TEST(BRepFacetedSphere, DifferenceAndUnionWithBox)
{
    const Body s = makeFacetedSphere(1.f, 6, 8);
    const Body bar = boxMinMax({0.f, -0.3f, -0.3f}, {2.f, 0.3f, 0.3f});  // pokes out +x
    expectWatertight(booleanToBody(s, bar, BooleanOp::Difference));
    expectWatertight(booleanToBody(s, bar, BooleanOp::Union));
}

TEST(BRepFacetedSphere, Deterministic)
{
    const Body s = makeFacetedSphere(1.f, 6, 8);
    const Body slab = boxMinMax({-2, -2, 0}, {2, 2, 2});
    const float v1 = booleanToBody(s, slab, BooleanOp::Intersection).massProperties().volume;
    const float v2 = booleanToBody(s, slab, BooleanOp::Intersection).massProperties().volume;
    EXPECT_EQ(v1, v2);
}

}  // namespace nexus::geometry::brep::testing
