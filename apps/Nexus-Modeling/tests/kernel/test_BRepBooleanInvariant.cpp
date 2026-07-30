// Foundation — booleanToBody's never-corrupt invariant. A boolean must return a
// valid solid or a clean empty Body, never one that fails its own checkIntegrity
// — even on the degenerate near-coincident-facet sews that used to produce a
// corrupt "partner not reciprocal" Body. fromFaces now rejects non-manifold sews,
// and booleanToBody drops any integrity failure to an empty Body.

#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

namespace nexus::geometry::brep::testing {


namespace {
Body cylAt(float dx)
{
    Body b = makeFacetedCylinder(1.f, 2.f, 12);
    b.translate({dx, 0.f, 0.f});
    return b;
}
}  // namespace

TEST(BRepBooleanInvariant, ResultAlwaysPassesCheckIntegrity)
{
    // A grid of faceted-cylinder booleans, including the offsets whose union sew
    // used to corrupt (dx = 0.6, 1.5). Every result must be integrity-clean —
    // either a valid solid (closed) or a clean empty Body (no faces).
    const Body a = makeFacetedCylinder(1.f, 2.f, 12);
    for (float dx : {0.3f, 0.5f, 0.6f, 0.7f, 1.0f, 1.5f, 2.5f}) {
        const Body b = cylAt(dx);
        for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
            const Body r = booleanToBody(a, b, op);
            const auto ig = r.checkIntegrity();
            EXPECT_TRUE(ig.ok) << "dx=" << dx << " op=" << static_cast<int>(op) << ": "
                               << ig.reason;
            // Non-empty results are watertight closed solids.
            if (r.faceCount() > 0) {
                EXPECT_TRUE(r.isClosed()) << "dx=" << dx << " op=" << static_cast<int>(op);
                EXPECT_EQ(ig.boundaryEdges, 0u);
            }
        }
    }
}

TEST(BRepBooleanInvariant, FormerlyDegenerateUnionNowResolvesWatertight)
{
    // Two identical faceted cylinders offset by 0.6. This once produced a "partner not
    // reciprocal" corpse, then (after fromFaces learned to reject non-manifold sews) a clean
    // empty Body. The coplanar-duplicate-face dedup in booleanToBody now resolves the
    // coincident sliver pair the classifier left doubled, so it produces the CORRECT
    // watertight union — verified by the exact inclusion-exclusion identity vol(U)+vol(I) =
    // vol(A)+vol(B) (measured U=8.3035, I=3.6965, A=B=6.0 → 12.0).
    const Body a = makeFacetedCylinder(1.f, 2.f, 12);
    const Body b = cylAt(0.6f);
    const Body u = booleanToBody(a, b, BooleanOp::Union);
    const Body i = booleanToBody(a, b, BooleanOp::Intersection);
    EXPECT_TRUE(u.checkIntegrity().ok);
    EXPECT_TRUE(u.isClosed());
    EXPECT_GT(u.faceCount(), 0u);
    const float sum = u.massProperties(1.f).volume + i.massProperties(1.f).volume;
    const float ab = a.massProperties(1.f).volume + b.massProperties(1.f).volume;
    EXPECT_NEAR(sum, ab, 1e-2f) << "union+intersection must equal A+B";
}

TEST(BRepBooleanInvariant, CleanCasesUnchanged)
{
    // Non-degenerate faceted booleans still produce valid watertight solids.
    const Body inter =
        booleanToBody(makeFacetedCylinder(1.f, 2.f, 12), cylAt(0.7f), BooleanOp::Intersection);
    EXPECT_TRUE(inter.checkIntegrity().ok);
    EXPECT_TRUE(inter.isClosed());
    EXPECT_GT(inter.checkIntegrity().faces, 0u);

    // A box-box union is unaffected (exact volume 15 as before).
    Body A = makeBox(2.f, 2.f, 2.f);
    Body B = makeBox(2.f, 2.f, 2.f);
    B.translate({1.f, 1.f, 1.f});
    const Body u = booleanToBody(A, B, BooleanOp::Union);
    EXPECT_TRUE(u.checkIntegrity().ok);
    EXPECT_TRUE(u.isClosed());
}

}  // namespace nexus::geometry::brep::testing
