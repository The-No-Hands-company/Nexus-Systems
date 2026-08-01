// A patch integrated inside-out, and the assumption that hid it.
//
// `massProperties` integrates a CURVED analytic face over its parameter domain rather than
// over its triangles, which is why a sphere reports its exact volume instead of a faceted
// one. The patch normal it uses is du x dv — and it was handed to the integrator flipped
// only by `face.reversed`, i.e. on the assumption that du x dv points OUT of the solid
// whenever the face is not reversed.
//
// That holds for a primitive as built. It is not a property the kernel maintains. A
// Boolean's `fromFaces` rebuilds the result's surface records, and nothing there preserves
// which way the parametrisation runs.
//
// A whole primitive could never show it — every face of a sphere, cylinder or cone as built
// agrees with the assumption, so every primitive test passed, at any radius, translated
// anywhere. It takes a Boolean's fragments.
//
// MEASURED on box(2) u sphere(1.2) offset 0.5 in x: all 112 spherical patches carried
// face.reversed == false and 3D rings wound OUTWARD, and every one of them integrated with
// an INWARD normal. The body reported 1.0845 where its own tessellation says 9.8516 — the
// sphere's entire contribution subtracted instead of added, so a union containing a 2x2x2
// box claimed a volume of one. The CENTRED union was correct, which is the kind of
// near-miss that keeps a sign error alive for a long time.
//
// The direction is now read off the face's own boundary, which is the topological truth: a
// face's ring is wound counter-clockwise seen from outside, so its Newell normal points
// out. Comparing that against du x dv once, at the parameter centroid, subsumes `reversed`
// entirely and cannot be fooled by a rebuilt surface record.
//
// A first attempt made the parameter-triangle area SIGNED instead, reasoning that the (u,v)
// winding carries the orientation. It halved the failures too — and broke the arc-bite
// cases that were exact before, because there the assumption held and the winding was
// merely an artefact of ring traversal. Reading the orientation from the ring is right in
// both.
//
// ── FOLLOW-UP, and it revises two things said above ─────────────────────────────────────
//
// The residual this file used to bound is closed, and neither of the two causes was the one
// named here.
//
// FIRST, the metric was invalid. "Disagrees with its own fine tessellation" cannot reach
// zero, because `toMesh` under-refines the interior of a curved patch and PLATEAUS instead of
// converging — a cylinder's tessellated volume stops at 6.1757 against an exact 6.28319. That
// number mixed the integrator's error with the tessellator's. The oracles used below —
// an imprint must not change a volume, and U + I == A + B — touch no tessellation at all.
//
// SECOND, the dominant cause was not the integrator's arithmetic but a bail: any face with an
// INNER LOOP made `integratePlanarFace` give up, and the caller's response is to discard every
// exactly-integrated face and re-integrate the whole body from toMesh(3). One bored cap moved
// an imprinted cylinder 1.75%. Green's theorem needs no special case for a hole; the inner
// loops are simply appended, already wound opposite.
//
// THIRD, and this is the revision to the paragraph above: the parameter-triangle area IS
// signed now. That is not a repeat of the reverted attempt. That attempt used the sign to
// carry the face's FACING, which is wrong and is what broke the arc-bite cases; the facing
// still comes from the ring's Newell normal. The sign here does a different job — the fan
// from vertex 0 only tiles a CONVEX parameter polygon, and with |area| the triangles falling
// outside a non-convex one are added instead of cancelled. The two concerns are now
// separate, which is why this one does not break what that one broke.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>
#include <nexus/geometry/MeshMassProperties.h>

#include <gtest/gtest.h>

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <utility>

namespace nexus::geometry::brep::testing {

namespace {

double tessVolume(const Body& b, uint32_t sub)
{
    return b.faceCount() == 0u ? 0.0 : MeshMassProperties::compute(b.toMesh(sub)).volume;
}

}  // namespace

// THE case. A union whose spherical patches wound the other way reported a volume smaller
// than the box it contains.
TEST(BRepMassPropertiesOrientation, AUnionsCurvedPatchesAreIntegratedTheRightWayRound)
{
    const Body box = makeBox(2.f, 2.f, 2.f);
    Body sph = makeSphere(1.2f, 8, 12);
    sph.translate({0.5, 0., 0.});
    const Body U = booleanToBody(box, sph, BooleanOp::Union);
    ASSERT_GT(U.faceCount(), 0u);
    ASSERT_TRUE(U.isClosed());

    const double analytic = U.massProperties().volume;
    const double tess = tessVolume(U, 6);

    // it contains a 2x2x2 box, so nothing below 8 can be right
    EXPECT_GT(analytic, 8.0) << "the union of a 2x2x2 box with a sphere reports " << analytic;

    // and it must AGREE with its own chordal tessellation to within the chord error. Note
    // the direction is deliberately not asserted: which way the two differ depends on
    // whether the kept patches bulge out of the body or into it, and on this fixture the
    // centred case comes out 0.03% the other way.
    EXPECT_NEAR(analytic, tess, 0.05 * tess)
        << "analytic " << analytic << " vs tessellated " << tess;
}

// The centred case, against an answer that is known outright.
//
// ORACLE CHANGED, and this is why: this used to compare the analytic volume against the
// body's own tessellation at 1%. That comparison was only ever meaningful while BOTH sides
// were wrong in the same direction. `toMesh` under-refines the interior of a curved patch
// and does not converge out of it — a cylinder's tessellated volume plateaus at 6.1757
// against an exact 2*pi = 6.28319 — so it is not a valid reference for a curved body at any
// subdivision. Once the integrator was repaired the two parted company and the test failed,
// reporting the accurate number as the error.
//
// The centred union has a closed form. The sphere (r = 1.2) pokes through each face of the
// box (half-extent 1.0) as a spherical cap of height 0.2, and those caps have footprint
// radius sqrt(1.44 - 1) = 0.663 < 1, so they do not reach the box's edges and do not meet
// each other. Volume = 8 + 6 * pi*a^2*(3R - a)/3 = 8.854513.
//
// MEASURED against it: analytic 8.851276 (0.037% — the residual is the faceted seam, not the
// integration), tessellated 8.697198 (1.78%). The oracle was 48x less accurate than the
// subject it was judging.
TEST(BRepMassPropertiesOrientation, TheCentredUnionMatchesItsClosedForm)
{
    const double kPi = 3.14159265358979323846;
    const double R = 1.2, h = 1.0, a = R - h;
    const double exact = 8.0 + 6.0 * (kPi * a * a * (3.0 * R - a) / 3.0);

    const Body U = booleanToBody(makeBox(2.f, 2.f, 2.f), makeSphere(1.2f, 8, 12),
                                 BooleanOp::Union);
    ASSERT_GT(U.faceCount(), 0u);
    const double analytic = U.massProperties().volume;
    EXPECT_NEAR(analytic, exact, exact * 2e-3)
        << "analytic " << analytic << " vs the closed form " << exact;

    // and it must be nearer the truth than the tessellation is, which is the whole reason
    // the analytic path exists
    const double tess = tessVolume(U, 6);
    EXPECT_LT(std::abs(analytic - exact), std::abs(tess - exact))
        << "the analytic path is no better than tessellating: analytic " << analytic
        << ", tessellated " << tess << ", exact " << exact;
}

// Primitives must be untouched: their analytic volumes are exact and independent of the
// segment count, which is the property the parametric integrator exists for.
TEST(BRepMassPropertiesOrientation, PrimitivesAreStillExactAtEverySegmentCount)
{
    const double kPi = 3.14159265358979323846;
    for (const uint32_t n : {8u, 16u, 64u}) {
        EXPECT_NEAR(makeCylinder(1.f, 2.f, n).massProperties().volume,
                    static_cast<float>(kPi * 2.0), static_cast<float>(kPi * 2.0) * 1e-5)
            << "cylinder n=" << n;
        EXPECT_NEAR(makeCone(1.f, 2.f, n).massProperties().volume,
                    static_cast<float>(kPi * 2.0 / 3.0),
                    static_cast<float>(kPi * 2.0 / 3.0) * 1e-5)
            << "cone n=" << n;
    }
    const double sphere = 4.0 / 3.0 * kPi * 1.2 * 1.2 * 1.2;
    for (const uint32_t lat : {4u, 8u, 16u})
        EXPECT_NEAR(makeSphere(1.2f, lat, 12).massProperties().volume,
                    static_cast<float>(sphere), static_cast<float>(sphere) * 1e-5)
            << "sphere lat=" << lat;

    // and translation must not matter — the divergence theorem is origin-independent
    Body s = makeSphere(1.2f, 8, 12);
    s.translate({5., -3., 2.});
    EXPECT_NEAR(s.massProperties().volume, static_cast<float>(sphere),
                static_cast<float>(sphere) * 1e-5);
}

// THE BOUND THAT ASKED TO BE RETIRED, RETIRED — and replaced with oracles that hold at zero.
//
// The sweep here used to compare each body's analytic volume against its own tessellation and
// assert that between 1 and 12 of them disagreed, the lower bound present so that "a silent
// fix is noticed and the bound retired". This is that moment, but not for the reason the
// bound anticipated: the METRIC was invalid. `toMesh` under-refines the interior of a curved
// patch and PLATEAUS rather than converging — a cylinder's tessellated volume stops at 6.1757
// against an exact 2*pi = 6.28319 — so "disagrees with its own tessellation" conflated two
// independent defects, the integrator's and the tessellator's, and could never reach zero
// however correct the integrator became.
//
// The two oracles below use no tessellation at all, so they see only the integrator, and both
// are now exactly satisfied:
//
//   * an IMPRINT only adds seams, so the solid — and its volume — must be unchanged;
//   * U + I == A + B, with all four volumes taken analytically.
//
// What they caught: a single face with an inner loop made the WHOLE body fall back to the
// tessellated path, and one bored cap was enough to move an imprinted cylinder by 1.75%.
TEST(BRepMassPropertiesOrientation, ImprintingDoesNotChangeVolume)
{
    int checked = 0;
    for (const double d : {0.0, 0.3, 0.6, 0.9, 1.2}) {
        const std::pair<Body, Body> pairs[] = {
            {makeBox(2.f, 2.f, 2.f), makeSphere(1.2f, 8, 12)},
            {makeBox(2.f, 2.f, 2.f), makeCylinder(0.7f, 4.f, 16)},
            {makeBox(2.f, 2.f, 2.f), makeCone(1.f, 2.f, 16)},
            {makeCylinder(1.f, 3.f, 16), makeCylinder(0.7f, 4.f, 16)},
        };
        for (const auto& [A0, B0] : pairs) {
            Body A = A0, B = B0;
            B.translate({d, 0.1 * d, 0.});
            const double va = A.massProperties().volume, vb = B.massProperties().volume;
            Body Ai = A, Bi = B;
            if (!imprintMutually(Ai, Bi)) continue;
            ++checked;
            EXPECT_NEAR(Ai.massProperties().volume, va, std::abs(va) * 1e-4)
                << "imprinting changed A's volume at d=" << d;
            EXPECT_NEAR(Bi.massProperties().volume, vb, std::abs(vb) * 1e-4)
                << "imprinting changed B's volume at d=" << d;
        }
    }
    EXPECT_GT(checked, 10) << "the sweep did not run";
}

TEST(BRepMassPropertiesOrientation, AnalyticVolumesSatisfyUnionPlusIntersection)
{
    int pairs = 0;
    for (const double d : {0.0, 0.3, 0.6, 0.9, 1.2}) {
        const std::pair<Body, Body> cases[] = {
            {makeBox(2.f, 2.f, 2.f), makeSphere(1.2f, 8, 12)},
            {makeBox(2.f, 2.f, 2.f), makeSphere(1.2f, 7, 9)},   // odd segment counts
            {makeBox(2.f, 2.f, 2.f), makeCylinder(0.7f, 4.f, 16)},
            {makeBox(2.f, 2.f, 2.f), makeCone(1.f, 2.f, 16)},
            {makeCylinder(1.f, 3.f, 16), makeCylinder(0.7f, 4.f, 16)},
            {makeSphere(1.3f, 8, 12), makeSphere(1.0f, 8, 12)},
        };
        for (const auto& [A0, B0] : cases) {
            Body A = A0, B = B0;
            B.translate({d, 0.1 * d, 0.05 * d});
            const Body U = booleanToBody(A, B, BooleanOp::Union);
            const Body I = booleanToBody(A, B, BooleanOp::Intersection);
            if (U.faceCount() == 0u || I.faceCount() == 0u) continue;  // empty is its own contract
            ++pairs;
            const double lhs = static_cast<double>(U.massProperties().volume)
                             + static_cast<double>(I.massProperties().volume);
            const double rhs = static_cast<double>(A.massProperties().volume)
                             + static_cast<double>(B.massProperties().volume);
            EXPECT_NEAR(lhs, rhs, std::max(1.0, std::abs(rhs)) * 1e-3)
                << "U+I != A+B at d=" << d << " (" << lhs << " vs " << rhs << ")";
        }
    }
    EXPECT_GT(pairs, 15) << "the sweep did not run";
}

// The tessellator's bias, pinned SEPARATELY so it is never again mistaken for the
// integrator's. A curved primitive's analytic volume is exact; its tessellation is not, and
// refining does not close the gap. If this ever starts converging, toMesh has been repaired
// and this characterization should become a convergence assertion.
TEST(BRepMassPropertiesOrientation, TessellationDoesNotConvergeToTheExactCurvedVolume)
{
    const double kPi = 3.14159265358979323846;
    const Body cyl = makeCylinder(1.f, 2.f, 16);
    const double exact = kPi * 2.0;
    EXPECT_NEAR(cyl.massProperties().volume, exact, exact * 1e-5) << "the analytic value";

    const double t2 = tessVolume(cyl, 2), t8 = tessVolume(cyl, 8);
    EXPECT_LT(t2, exact);
    EXPECT_LT(t8, exact);
    // refining moves it, but nowhere near the truth: still more than 1% short at sub 8
    EXPECT_GT(exact - t8, exact * 0.01)
        << "toMesh now converges on curved patches — retire this characterization";
}

}  // namespace nexus::geometry::brep::testing
