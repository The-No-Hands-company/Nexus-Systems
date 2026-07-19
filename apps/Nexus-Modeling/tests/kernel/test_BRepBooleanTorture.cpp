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

}  // namespace nexus::geometry::brep::testing
