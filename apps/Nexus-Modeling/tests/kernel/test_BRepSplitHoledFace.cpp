// Splitting a face gave its holes to the wrong piece.
//
// `cutFaceBetween` divides a face's outer loop into two rings and hands one to a new face. It
// never looked at the face's INNER loops, so every hole stayed attached to whichever piece
// inherited the original face record — a choice about which segment of the ring kept the old
// id, which has nothing to do with where the hole is.
//
// MEASURED on box(2,2,2) bored by a cylinder of radius 0.5 and then slotted across its top: the
// top cap split into the strip y in [0.6, 1.0] and the remainder y in [-1, 0.6], and the BORE'S
// HOLE — a circle of radius 0.5 about the origin, entirely inside the remainder — was left on the
// STRIP. The strip lies inside the cut, so the Difference correctly dropped it, and it took the
// bore's rim with it. The rim's 24 segments were then offered by the cylinder faces alone: 24
// one-sided edges, an open sew, and `booleanToBody` returning a clean empty body by its
// watertight-or-empty contract.
//
// The visible symptom was that "drill a hole, then cut across it" failed — on any solid with a
// hole in a face that a later cut splits. It is a face-splitting bug, not a Boolean one, and not
// a bore one: what mattered was only that a split face had a hole.
//
// A hole lies inside exactly one of the two pieces UNLESS the cut runs through it, and that is
// not a re-assignment at all — it is a hole being divided into two boundary arcs, which
// `cutFaceBetween` does not do. That case is REFUSED rather than approximated, so the Boolean
// still returns cleanly empty for it, now by decision. It is the named remaining gap.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry::brep::testing {

namespace {

constexpr double kPi = 3.14159265358979323846;

Body boredBox()   // box(2,2,2) with a radius-0.5 bore along Z
{
    return booleanToBody(makeBox(2.f, 2.f, 2.f), makeCylinder(0.5f, 4.f, 24),
                         BooleanOp::Difference);
}

bool sound(const Body& b)
{
    return b.faceCount() > 0 && b.isClosed() && b.checkIntegrity().ok && b.checkGeometry().ok;
}

int facesWithHoles(const Body& b)
{
    int n = 0;
    for (uint32_t f = 0; f < static_cast<uint32_t>(b.faceCount()); ++f)
        if (b.face(f).alive && !b.face(f).innerLoops.empty()) ++n;
    return n;
}

}  // namespace

// THE case: a slot across the top of a bored box, clear of the bore. This returned an empty body.
TEST(BRepSplitHoledFace, ASlotAcrossABoredBoxTopSews)
{
    const Body bored = boredBox();
    ASSERT_TRUE(sound(bored));
    ASSERT_EQ(facesWithHoles(bored), 2) << "the fixture has no holed faces — the bore is missing";

    Body slot = makeBox(6.f, 0.8f, 0.8f);   // through in X, clear of the bore in Y
    slot.translate({0., 1.0, 0.8});         // reaches past the top face at z = 1

    const Body r = booleanToBody(bored, slot, BooleanOp::Difference);
    ASSERT_TRUE(sound(r)) << "the slot across a bored box did not sew";
    // the bore is untouched by this cut, so the result still has its two holed caps
    EXPECT_EQ(facesWithHoles(r), 2) << "the bore's holes were lost in the split";
}

// The hole must end up on the piece that CONTAINS it, which is the fix stated directly rather
// than through a Boolean's success.
TEST(BRepSplitHoledFace, AnInnerLoopFollowsTheGeometryNotTheFaceRecord)
{
    Body A = boredBox();
    Body B = makeBox(6.f, 0.8f, 0.8f);
    B.translate({0., 1.0, 0.8});
    ASSERT_TRUE(imprintMutually(A, B));

    // find the two planar pieces at z = +1 and check which carries the bore's hole
    int checked = 0;
    for (uint32_t f = 0; f < static_cast<uint32_t>(A.faceCount()); ++f) {
        const Face& fc = A.face(f);
        if (!fc.alive || fc.surface >= A.surfaceCount()) continue;
        if (A.surface(fc.surface).kind != SurfaceKind::Plane) continue;
        const std::vector<uint32_t> vs = A.faceVertices(f);
        if (vs.size() < 3) continue;
        bool atTop = true;
        double yLo = 1e30, yHi = -1e30;
        for (const uint32_t v : vs) {
            const Vec3& p = A.vertex(v).point;
            if (std::abs(p.z - 1.0) > 1e-9) { atTop = false; break; }
            yLo = std::min(yLo, p.y);
            yHi = std::max(yHi, p.y);
        }
        if (!atTop) continue;
        ++checked;
        // the bore is at |y| <= 0.5, so only the piece spanning that can hold its hole
        const bool couldContainBore = (yLo < -0.4 && yHi > 0.4);
        EXPECT_EQ(!fc.innerLoops.empty(), couldContainBore)
            << "top piece y in [" << yLo << ", " << yHi << "] has "
            << fc.innerLoops.size() << " inner loop(s) — a hole is on a piece that cannot "
                                       "contain it, or missing from the one that does";
    }
    EXPECT_EQ(checked, 2) << "the top cap did not split into exactly two pieces";
}

// A cut that SWALLOWS the bore: the result is just the box minus the cut, and it must sew. This
// is the plainest form of "the hole disappears into a later cut".
TEST(BRepSplitHoledFace, ACutThatSwallowsTheBoreSews)
{
    const Body bored = boredBox();
    const Body plain = makeBox(2.f, 2.f, 2.f);
    // side 1.2 > the bore's diameter of 1.0, so the bar contains the bore entirely
    const Body bar = makeBox(1.2f, 1.2f, 6.f);

    const Body viaBore = booleanToBody(bored, bar, BooleanOp::Difference);
    const Body direct = booleanToBody(plain, bar, BooleanOp::Difference);
    ASSERT_TRUE(sound(viaBore)) << "cutting away a bore did not sew";
    ASSERT_TRUE(sound(direct));
    // removing the bore first cannot change the answer: the bar takes all of it
    EXPECT_NEAR(static_cast<double>(viaBore.massProperties().volume),
                static_cast<double>(direct.massProperties().volume), 1e-6)
        << "bored-then-cut disagrees with cut-directly, though the bar swallows the bore";
    EXPECT_EQ(facesWithHoles(viaBore), 0) << "the bore is gone, so no face should carry its hole";
}

// A drilled plate, cut clear of its hole — the same defect on a different body, to show it was
// never about the bore.
TEST(BRepSplitHoledFace, ADrilledPlateCutClearOfItsHoleSews)
{
    const Body plate = booleanToBody(makeBox(4.f, 4.f, 1.f), makeCylinder(0.6f, 4.f, 16),
                                     BooleanOp::Difference);
    ASSERT_TRUE(sound(plate));
    ASSERT_EQ(facesWithHoles(plate), 2);

    Body notch = makeBox(6.f, 0.8f, 2.f);   // through in X, clear of the hole, through the top
    notch.translate({0., 1.5, 0.});
    const Body r = booleanToBody(plate, notch, BooleanOp::Difference);
    ASSERT_TRUE(sound(r)) << "a notch across a drilled plate did not sew";
    EXPECT_EQ(facesWithHoles(r), 2) << "the drill hole was lost in the split";

    // volume: the plate is 4x4x1 less a pi*0.6^2 bore, then a 0.8-wide notch across it
    const double plateVol = 4.0 * 4.0 * 1.0 - kPi * 0.36 * 1.0;
    const double notchVol = 4.0 * 0.8 * 1.0;          // spans x fully, y in [1.1,1.9], full depth
    EXPECT_NEAR(static_cast<double>(r.massProperties().volume), plateVol - notchVol, 1e-3);
}

// THE REMAINING GAP, pinned so it is visible: a cut running THROUGH a hole would divide that hole
// into two boundary arcs, and cutFaceBetween declines instead. The Boolean then returns cleanly
// empty, which is the watertight-or-empty contract, not a crash. If this starts sewing, the hole-
// splitting case has been implemented and this characterization should become a real assertion.
TEST(BRepSplitHoledFace, ACutThroughAHoleStillDeclinesCleanly)
{
    const Body bored = boredBox();
    Body slot = makeBox(6.f, 0.8f, 0.8f);   // y in [-0.4, 0.4] runs straight through the bore
    slot.translate({0., 0., 0.8});          // and out through the top

    const Body r = booleanToBody(bored, slot, BooleanOp::Difference);
    EXPECT_EQ(r.faceCount(), 0u)
        << "a cut through a hole now sews — implement/verify the hole-splitting case and promote "
           "this into a real assertion";
    // whatever happens, the contract holds: empty or watertight, never leaky
    if (r.faceCount() > 0) EXPECT_TRUE(r.isClosed());
}

}  // namespace nexus::geometry::brep::testing
