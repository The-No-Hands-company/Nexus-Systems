// Foundation — rounded fillet of a box edge (the round to chamfer's bevel), via
// a boolean difference with a faceted corner-minus-quarter-cylinder tool. The
// removed wedge converges to the smooth fillet r²·(1−π/4)·edgeLength as the arc
// facet count grows.

#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;

namespace {
void expectWatertight(const Body& b)
{
    const auto ig = b.checkIntegrity();
    EXPECT_TRUE(ig.ok) << ig.reason;
    EXPECT_EQ(ig.boundaryEdges, 0u);
    EXPECT_TRUE(b.isClosed());
    EXPECT_TRUE(b.checkGeometry().ok) << b.checkGeometry().reason;
}
// Smooth single-edge fillet volume for a 2×2×2 box (edge length 2).
double smoothFillet(float r) { return 8.0 - static_cast<double>(r) * r * (1.0 - M_PI / 4.0) * 2.0; }
}  // namespace

TEST(BRepFillet, WatertightAndConvergesToSmooth)
{
    const float r = 0.4f;
    const Body box = makeBox(2.f, 2.f, 2.f);
    auto vol = [&](uint32_t seg) {
        return static_cast<double>(filletBoxEdge(box, 0, +1, +1, r, seg).massProperties().volume);
    };
    const Body coarse = filletBoxEdge(box, 0, +1, +1, r, 4);
    expectWatertight(coarse);

    const double e4 = std::abs(vol(4) - smoothFillet(r));
    const double e8 = std::abs(vol(8) - smoothFillet(r));
    const double e16 = std::abs(vol(16) - smoothFillet(r));
    EXPECT_GT(e4, e8);   // finer arc → strictly closer to the smooth fillet
    EXPECT_GT(e8, e16);
    EXPECT_LT(e16, 1e-3);
}

TEST(BRepFillet, WatertightEachAxis)
{
    const float r = 0.4f;
    for (int axis = 0; axis < 3; ++axis) {
        const Body f = filletBoxEdge(makeBox(2.f, 2.f, 2.f), axis, +1, +1, r, 8);
        expectWatertight(f);
        EXPECT_LT(f.massProperties().volume, 8.f);        // material removed
        EXPECT_GT(f.massProperties().volume, 8.f - 0.1f);  // only the corner wedge
    }
}

TEST(BRepFillet, MultipleEdges)
{
    const float r = 0.4f;
    const Body box = makeBox(2.f, 2.f, 2.f);
    const Body two = filletBoxEdge(filletBoxEdge(box, 0, +1, +1, r, 8), 0, -1, -1, r, 8);
    expectWatertight(two);
    // Roughly two single-edge wedges removed.
    EXPECT_NEAR(two.massProperties().volume, 8.0 - 2.0 * (8.0 - smoothFillet(r)), 0.01);
}

TEST(BRepFillet, DegenerateHandled)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    EXPECT_NEAR(filletBoxEdge(box, 0, +1, +1, 0.f, 8).massProperties().volume, 8.f, 1e-4f);
    EXPECT_NEAR(filletBoxEdge(box, 0, +1, +1, -2.f, 8).massProperties().volume, 8.f, 1e-4f);
    // A radius larger than the box is clamped to a valid watertight fillet.
    const Body big = filletBoxEdge(box, 0, +1, +1, 100.f, 8);
    expectWatertight(big);
    EXPECT_LT(big.massProperties().volume, 8.f);
    EXPECT_GT(big.massProperties().volume, 0.f);
}

TEST(BRepFillet, Deterministic)
{
    const float v1 = filletBoxEdge(makeBox(2.f, 2.f, 2.f), 2, -1, +1, 0.3f, 8).massProperties().volume;
    const float v2 = filletBoxEdge(makeBox(2.f, 2.f, 2.f), 2, -1, +1, 0.3f, 8).massProperties().volume;
    EXPECT_EQ(v1, v2);
}

}  // namespace nexus::geometry::brep::testing
