// Foundation — faceted curved-solid booleans. makeFacetedCylinder builds an
// ALL-PLANAR n-gon prism, so the existing planar boolean pipeline booleans it
// against boxes and other faceted solids — watertight, with the exact
// faceted-prism volume, converging to the smooth-cylinder value as facets grow.

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
// Volume of a regular n-gon prism (radius r, height h).
double prismVolume(float r, float h, uint32_t n)
{
    return static_cast<double>(n) * 0.5 * std::sin(2.0 * M_PI / n) * r * r * h;
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

TEST(BRepFacetedBoolean, FacetedCylinderIsAllPlanarSolid)
{
    const Body c = makeFacetedCylinder(1.f, 2.f, 12);
    expectWatertight(c);
    EXPECT_EQ(c.checkIntegrity().euler, 2);
    EXPECT_NEAR(c.massProperties().volume, static_cast<float>(prismVolume(1.f, 2.f, 12)), 1e-3f);
}

TEST(BRepFacetedBoolean, HalfByBoxIsExactHalfPrism)
{
    // Cylinder r=1 h=2 (z ∈ [-1,1]) sliced by the slab z ≥ 0 (which does not touch
    // the sides in xy) → exactly half the prism.
    const Body c = makeFacetedCylinder(1.f, 2.f, 12);
    const Body slab = boxMinMax({-2, -2, 0}, {2, 2, 2});
    const double half = prismVolume(1.f, 2.f, 12) / 2.0;

    const Body inter = booleanToBody(c, slab, BooleanOp::Intersection);
    expectWatertight(inter);
    EXPECT_NEAR(inter.massProperties().volume, static_cast<float>(half), 1e-3f);

    const Body diff = booleanToBody(c, slab, BooleanOp::Difference);
    expectWatertight(diff);
    EXPECT_NEAR(diff.massProperties().volume, static_cast<float>(half), 1e-3f);
}

TEST(BRepFacetedBoolean, ConvergesToSmoothCylinder)
{
    // The half-cylinder intersection volume approaches ½·π·r²·h = π as facets grow.
    const Body slab = boxMinMax({-2, -2, 0}, {2, 2, 2});
    auto halfVol = [&](uint32_t n) {
        return static_cast<double>(
            booleanToBody(makeFacetedCylinder(1.f, 2.f, n), slab, BooleanOp::Intersection)
                .massProperties()
                .volume);
    };
    const double e6 = std::abs(halfVol(6) - M_PI);
    const double e12 = std::abs(halfVol(12) - M_PI);
    const double e24 = std::abs(halfVol(24) - M_PI);
    EXPECT_GT(e6, e12);   // strictly closer to the smooth value with more facets
    EXPECT_GT(e12, e24);
    EXPECT_LT(e24, 0.05);  // and quite close by n=24
}

TEST(BRepFacetedBoolean, HighFacetCountIsPracticalAndWatertight)
{
    // The full-pass imprint fixpoint makes a 48-facet cylinder boolean practical
    // (it was impractically slow with the restart-per-imprint scan). The result
    // is watertight and its half-volume is close to the smooth cylinder's π.
    const Body c = makeFacetedCylinder(1.f, 2.f, 48);
    const Body slab = boxMinMax({-2, -2, 0}, {2, 2, 2});
    const Body inter = booleanToBody(c, slab, BooleanOp::Intersection);
    expectWatertight(inter);
    EXPECT_NEAR(inter.massProperties().volume, static_cast<float>(M_PI), 0.02);
    EXPECT_NEAR(inter.massProperties().volume,
                static_cast<float>(prismVolume(1.f, 2.f, 48) / 2.0), 1e-3f);  // exact prism half
}

TEST(BRepFacetedBoolean, UnionAndDifferenceWithBox)
{
    const Body c = makeFacetedCylinder(1.f, 2.f, 12);
    const Body box = boxMinMax({0.4f, -0.3f, -0.3f}, {2.5f, 0.3f, 0.3f});  // pokes out one side
    expectWatertight(booleanToBody(c, box, BooleanOp::Union));
    expectWatertight(booleanToBody(c, box, BooleanOp::Difference));
}

TEST(BRepFacetedBoolean, IntersectTwoFacetedCylinders)
{
    // Two all-planar faceted cylinders intersect into a watertight solid.
    const Body a = makeFacetedCylinder(1.f, 2.f, 12);
    Body b = makeFacetedCylinder(1.f, 2.f, 12);
    b.translate({0.6f, 0.f, 0.f});
    expectWatertight(booleanToBody(a, b, BooleanOp::Intersection));
}

TEST(BRepFacetedBoolean, Deterministic)
{
    const Body c = makeFacetedCylinder(1.f, 2.f, 12);
    const Body slab = boxMinMax({-2, -2, 0}, {2, 2, 2});
    const float v1 = booleanToBody(c, slab, BooleanOp::Intersection).massProperties().volume;
    const float v2 = booleanToBody(c, slab, BooleanOp::Intersection).massProperties().volume;
    EXPECT_EQ(v1, v2);
}

}  // namespace nexus::geometry::brep::testing
