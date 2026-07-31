// A face can have more than one hole, and until now the tessellator kept only the first.
//
// `toMesh` collected inner rings with a loop that assigned one and broke. The source said
// so plainly — "multiple holes on one face are a rare follow-up" — and that turned out to
// be the opposite of true: a plate drilled twice has two holes in one face, which is most
// real parts.
//
// The consequence is not cosmetic, because classifyPoint is a parity ray cast against
// these triangles. A hole that never reaches the tessellation is not a hole as far as
// classification is concerned: the face is drawn solid across it, the ray crosses material
// that should not be there, and points behind it come back on the wrong side.
//
// MEASURED on the canonical chain — a 4x4x1 plate drilled four times in sequence. The
// first hole was fine. On the second, three of the drill's own fifty faces, sampled at
// mid-plate and plainly inside the plate, classified OUTSIDE it. They were dropped from
// the difference, eight boundary edges were left with one face instead of two, and the sew
// refused. The chain died at hole two, and it had nothing to do with the two bores
// interacting — they are 2.4 apart with radius 0.4.
//
// Holes are now bridged into the outer ring one at a time, right to left, each by a
// two-way cut from its rightmost vertex to the first polygon edge a +X ray meets. The ray
// is cast against the polygon built SO FAR rather than against the outer ring, so a hole
// bridging leftward may land on a hole already merged — which is correct, and is the reason
// the order matters.
//
// This is what a feature history is made of, so the assertions below are about the chain
// rather than about a single operation.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

namespace nexus::geometry::brep::testing {

namespace {

double meshVolume(const Body& b, uint32_t sub)
{
    return MeshMassProperties::compute(b.toMesh(sub)).volume;
}

bool solid(const Body& b)
{
    return b.faceCount() > 0 && b.isClosed() && b.checkIntegrity().ok && b.checkGeometry().ok;
}

Body drillAt(double x, double y)
{
    Body c = makeCylinder(0.4f, 4.f, 16);
    c.translate({x, y, 0.0});
    return c;
}

}  // namespace

// THE chain. Each hole must remove the SAME volume as the one before it — the bores are
// identical and none of them overlaps another, so anything else means a hole was lost or
// double-counted.
TEST(BRepMultiHoleFace, DrillingFourHolesInSequenceKeepsTheSolidValid)
{
    Body cur = makeBox(4.f, 4.f, 1.f);
    ASSERT_TRUE(solid(cur));
    const double start = meshVolume(cur, 2);
    EXPECT_NEAR(start, 16.0, 1e-9);

    const double at[4][2] = {{1.2, 1.2}, {-1.2, 1.2}, {-1.2, -1.2}, {1.2, -1.2}};
    double previous = start;
    double firstBite = 0.0;
    for (int i = 0; i < 4; ++i) {
        const Body next = booleanToBody(cur, drillAt(at[i][0], at[i][1]), BooleanOp::Difference);
        ASSERT_TRUE(solid(next)) << "the chain broke at hole " << (i + 1);
        const double now = meshVolume(next, 2);
        const double bite = previous - now;
        if (i == 0)
            firstBite = bite;
        else
            EXPECT_NEAR(bite, firstBite, 1e-6)
                << "hole " << (i + 1) << " removed " << bite << " where hole 1 removed "
                << firstBite << " — identical bores must remove identical volume";
        previous = now;
        cur = next;
    }

    // four 16-gon bores of radius 0.4 through a plate of height 1
    const double gon = 0.5 * 16.0 * 0.4 * 0.4 * std::sin(2.0 * 3.14159265358979 / 16.0);
    const double smooth = 3.14159265358979 * 0.16;
    const double removed = start - previous;
    EXPECT_GT(removed, 4.0 * gon * 0.98) << "less material removed than four faceted bores";
    EXPECT_LT(removed, 4.0 * smooth * 1.02) << "more material removed than four true bores";
}

// The face really does end up carrying several holes — asserted directly, so the test
// above cannot pass for some unrelated reason.
TEST(BRepMultiHoleFace, AFaceCarriesEveryHoleDrilledThroughIt)
{
    Body cur = makeBox(4.f, 4.f, 1.f);
    for (const auto& p : {std::pair{1.2, 1.2}, {-1.2, 1.2}, {-1.2, -1.2}}) {
        cur = booleanToBody(cur, drillAt(p.first, p.second), BooleanOp::Difference);
        ASSERT_TRUE(solid(cur));
    }

    size_t mostHoles = 0;
    size_t totalHoles = 0;
    for (uint32_t f = 0; f < static_cast<uint32_t>(cur.faceCount()); ++f) {
        if (!cur.face(f).alive) continue;
        const size_t h = cur.faceInnerLoopVertices(f).size();
        mostHoles = std::max(mostHoles, h);
        totalHoles += h;
    }
    EXPECT_EQ(mostHoles, 3u) << "the plate's face should carry all three bores as holes";
    EXPECT_EQ(totalHoles, 6u) << "three bores, entering one face and leaving another";
}

// The tessellation must actually OPEN the holes, which is what classification rides on. A
// point in the middle of a bore is outside the solid; if a hole were drawn solid it would
// read as inside.
TEST(BRepMultiHoleFace, EveryBoreIsOpenInTheTessellationNotJustTheFirst)
{
    Body cur = makeBox(4.f, 4.f, 1.f);
    const double at[3][2] = {{1.2, 1.2}, {-1.2, 1.2}, {-1.2, -1.2}};
    for (const auto& p : at) {
        cur = booleanToBody(cur, drillAt(p[0], p[1]), BooleanOp::Difference);
        ASSERT_TRUE(solid(cur));
    }

    for (const auto& p : at) {
        const Vec3 downTheBore{p[0], p[1], 0.0};
        EXPECT_EQ(cur.classifyPoint(downTheBore), Body::PointContainment::Outside)
            << "the axis of the bore at (" << p[0] << "," << p[1]
            << ") reads as material — that hole is not open in the tessellation";
    }
    // and the plate itself is still solid where it should be
    EXPECT_EQ(cur.classifyPoint({0.0, 0.0, 0.0}), Body::PointContainment::Inside);
    EXPECT_EQ(cur.classifyPoint({0.0, 0.0, 3.0}), Body::PointContainment::Outside);
}

// Mixed chains: differences and unions alternating, each consuming the previous result.
TEST(BRepMultiHoleFace, MixedOperationChainsStayValid)
{
    Body cur = makeBox(4.f, 4.f, 1.f);
    cur = booleanToBody(cur, drillAt(1.2, 1.2), BooleanOp::Difference);
    ASSERT_TRUE(solid(cur));
    cur = booleanToBody(cur, drillAt(-1.2, 1.2), BooleanOp::Difference);
    ASSERT_TRUE(solid(cur));

    const double holed = meshVolume(cur, 2);
    for (int i = 0; i < 2; ++i) {
        Body boss = makeBox(1.f, 1.f, 3.f);
        boss.translate({0.0, i ? 1.0 : -1.0, 0.0});
        const Body next = booleanToBody(cur, boss, BooleanOp::Union);
        ASSERT_TRUE(solid(next)) << "union " << (i + 1) << " of the chain failed";
        cur = next;
    }
    // each boss is 1x1x3 and one unit of it already lies inside the plate
    EXPECT_NEAR(meshVolume(cur, 2) - holed, 4.0, 1e-6)
        << "two bosses should add exactly two units each";
}

// A single hole must be unaffected — the one-hole path is what every previous test in this
// kernel exercised, and the bridging was rewritten underneath it.
TEST(BRepMultiHoleFace, SingleHoleFacesAreUnchanged)
{
    const Body plate = makeBox(4.f, 4.f, 1.f);
    const Body one = booleanToBody(plate, drillAt(0.0, 0.0), BooleanOp::Difference);
    ASSERT_TRUE(solid(one));
    EXPECT_EQ(one.faceCount(), 22u) << "a singly-drilled plate changed shape";

    const double gon = 0.5 * 16.0 * 0.4 * 0.4 * std::sin(2.0 * 3.14159265358979 / 16.0);
    const double removed = meshVolume(plate, 0) - meshVolume(one, 0);
    EXPECT_NEAR(removed, gon, 1e-6) << "at subdivision 0 a 16-gon bore removes the n-gon area";
    EXPECT_EQ(one.classifyPoint({0.0, 0.0, 0.0}), Body::PointContainment::Outside);
}

}  // namespace nexus::geometry::brep::testing
