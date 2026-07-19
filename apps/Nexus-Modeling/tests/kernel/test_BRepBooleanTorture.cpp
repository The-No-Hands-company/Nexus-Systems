// Foundation — near-degenerate boolean TORTURE battery. Now that every core
// boolean topological DECISION is exact-arithmetic (point-vs-plane, segment-tri,
// in-plane straddle, plane-plane parallel, plane-cyl perp, and the SoS point-in-
// solid ray parity), this proves the pipeline's zero-error contract end-to-end:
// across a wide sweep of grazing / near-coincident / rotated / face-touching
// configurations, booleanToBody ALWAYS returns a WATERTIGHT solid or a clean empty
// Body — never a corrupt or LEAKY (open) one — and is fully deterministic.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/render/Camera.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace nexus::geometry::brep::testing {

using nexus::render::Vec3;
using nexus::render::Mat4;

namespace {
Mat4 rotZ(float a)
{
    Mat4 m = Mat4::identity();
    const float c = std::cos(a), s = std::sin(a);
    m.m[0][0] = c;  m.m[0][1] = -s;
    m.m[1][0] = s;  m.m[1][1] = c;
    return m;
}

// The zero-error contract: valid watertight solid OR clean empty — never leaky.
void expectWatertightOrEmpty(const Body& r, const char* where)
{
    const auto ig = r.checkIntegrity();
    EXPECT_TRUE(ig.ok) << where << ": " << ig.reason;
    if (r.faceCount() > 0) {
        // The zero-error contract is watertight-or-empty: a non-empty result must
        // be a closed shell with NO boundary edges (never an open/leaky sew). The
        // euler characteristic is NOT constrained — a boolean can legitimately
        // yield disjoint components (euler 2·k) or edge-manifold bodies that touch
        // at a single vertex (e.g. two cubes sharing a corner → euler 3); those are
        // valid CSG results, not corruption.
        EXPECT_TRUE(r.isClosed()) << where << " (result is open/leaky)";
        EXPECT_EQ(ig.boundaryEdges, 0u) << where;
    }
}
}  // namespace

// Box ∩/∪/− box swept over exact-coincident, near-coincident, face-touching,
// edge-touching, and rotated configurations. Every result is watertight-or-empty
// and byte-for-byte deterministic.
TEST(BRepBooleanTorture, BoxBatteryWatertightOrEmptyAndDeterministic)
{
    const Body A = makeBox(2.f, 2.f, 2.f);  // [-1,1]^3
    std::vector<Vec3> offsets;
    for (float dx : {0.f, 1e-4f, 0.5f, 1.f, 1.9999f, 2.f, 2.0001f, 3.f})
        offsets.push_back({dx, 0.f, 0.f});
    offsets.push_back({1.f, 1.f, 1.f});         // corner overlap
    offsets.push_back({2.f, 2.f, 2.f});         // exact corner touch
    offsets.push_back({1e-4f, 1e-4f, 1e-4f});   // tiny diagonal offset
    offsets.push_back({2.f, 2.f, 0.f});         // exact edge touch
    const std::vector<float> angles = {0.f, 0.7853981634f, 0.5f, 1.0471975512f};  // 0/45/28.6/60°

    int checked = 0;
    for (const Vec3& off : offsets) {
        for (float ang : angles) {
            Body B = makeBox(2.f, 2.f, 2.f);
            if (ang != 0.f) ASSERT_TRUE(B.transform(rotZ(ang)));
            B.translate(off);
            for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
                const Body r = booleanToBody(A, B, op);
                expectWatertightOrEmpty(r, "box-battery");
                EXPECT_EQ(r.serialize(), booleanToBody(A, B, op).serialize());  // deterministic
                ++checked;
            }
        }
    }
    EXPECT_GT(checked, 100);
}

// Faceted-cylinder booleans over offsets including the historically-corrupt ones
// (near-coincident facets). Watertight-or-empty and deterministic throughout.
TEST(BRepBooleanTorture, FacetedCylinderBatteryWatertightOrEmpty)
{
    const Body A = makeFacetedCylinder(1.f, 2.f, 12);
    for (float dx : {0.f, 1e-4f, 0.5f, 0.6f, 1.f, 1.5f, 2.f}) {
        Body B = makeFacetedCylinder(1.f, 2.f, 12);
        B.translate({dx, 0.f, 0.f});
        for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
            const Body r = booleanToBody(A, B, op);
            expectWatertightOrEmpty(r, "faceted-cyl-battery");
            EXPECT_EQ(r.serialize(), booleanToBody(A, B, op).serialize());
        }
    }
}

// The specific regression this increment fixed: a near-face-touch (dx=1.9999) that
// used to sew an OPEN (leaky) shell now returns watertight-or-clean-empty.
TEST(BRepBooleanTorture, NearFaceTouchIsNeverLeaky)
{
    const Body A = makeBox(2.f, 2.f, 2.f);
    Body B = makeBox(2.f, 2.f, 2.f);
    B.translate({1.9999f, 0.f, 0.f});
    for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
        const Body r = booleanToBody(A, B, op);
        // Must NOT be an open shell: either watertight, or cleanly empty.
        EXPECT_TRUE(r.faceCount() == 0 || (r.isClosed() && r.checkIntegrity().boundaryEdges == 0u))
            << "op=" << static_cast<int>(op);
    }
}

namespace {
Body tr(Body b, Vec3 o)
{
    b.translate(o);
    return b;
}
}  // namespace

// CHAINED 3-way booleans ((A∪B)−C, (A−B)∩C) over near-degenerate offsets stay
// watertight-or-empty and deterministic — the whole exact stack composes.
TEST(BRepBooleanTorture, ChainedThreeWayWatertightOrEmpty)
{
    for (float d1 : {0.5f, 1.f, 1.9999f, 2.f}) {
        for (float d2 : {0.5f, 1.f, 1.9999f}) {
            const Body A = makeBox(2.f, 2.f, 2.f);
            const Body B = tr(makeBox(2.f, 2.f, 2.f), {d1, 0.f, 0.f});
            const Body C = tr(makeBox(2.f, 2.f, 2.f), {0.f, d2, 0.f});
            const Body r1 = booleanToBody(booleanToBody(A, B, BooleanOp::Union), C,
                                          BooleanOp::Difference);
            expectWatertightOrEmpty(r1, "(AuB)-C");
            EXPECT_EQ(r1.serialize(), booleanToBody(booleanToBody(A, B, BooleanOp::Union), C,
                                                    BooleanOp::Difference)
                                          .serialize());
            const Body r2 = booleanToBody(booleanToBody(A, B, BooleanOp::Difference), C,
                                          BooleanOp::Intersection);
            expectWatertightOrEmpty(r2, "(A-B)nC");
        }
    }
}

// MIXED primitive types (box × faceted-cylinder × faceted-sphere), both operand
// orders, over a sweep of offsets — watertight-or-empty and deterministic.
TEST(BRepBooleanTorture, MixedPrimitivesWatertightOrEmpty)
{
    for (float dx : {0.f, 0.5f, 1.f, 1.5f}) {
        const Body box = makeBox(2.f, 2.f, 2.f);
        const Body fcyl = tr(makeFacetedCylinder(1.f, 3.f, 12), {dx, 0.f, 0.f});
        const Body fsph = tr(makeFacetedSphere(1.2f, 8, 12), {dx, 0.f, 0.f});
        for (BooleanOp op : {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference}) {
            expectWatertightOrEmpty(booleanToBody(box, fcyl, op), "box?fcyl");
            expectWatertightOrEmpty(booleanToBody(fcyl, box, op), "fcyl?box");
            expectWatertightOrEmpty(booleanToBody(box, fsph, op), "box?fsph");
            EXPECT_EQ(booleanToBody(box, fcyl, op).serialize(),
                      booleanToBody(box, fcyl, op).serialize());
        }
    }
}

// A DIFFERENCE that splits the target into TWO disconnected pieces is a valid
// multi-component watertight result: a 6×1×1 bar minus a w×3×3 slab through the
// middle → two bars, each watertight, total volume 6−w, euler 4 (two genus-0
// shells). Proves the pipeline handles component-splitting cleanly.
TEST(BRepBooleanTorture, DifferenceSplitProducesTwoWatertightPieces)
{
    for (float w : {0.3f, 0.5f, 1.f}) {
        const Body bar = makeBox(6.f, 1.f, 1.f);
        const Body slab = makeBox(w, 3.f, 3.f);  // spans the bar's full cross-section
        const Body r = booleanToBody(bar, slab, BooleanOp::Difference);
        const auto ig = r.checkIntegrity();
        ASSERT_TRUE(ig.ok) << "w=" << w << ": " << ig.reason;
        EXPECT_TRUE(r.isClosed()) << "w=" << w;
        EXPECT_EQ(ig.boundaryEdges, 0u) << "w=" << w;
        EXPECT_EQ(ig.euler, 4) << "w=" << w;  // two disconnected genus-0 pieces
        EXPECT_NEAR(r.massProperties().volume, 6.f - w, 1e-3f) << "w=" << w;
    }
}

}  // namespace nexus::geometry::brep::testing
