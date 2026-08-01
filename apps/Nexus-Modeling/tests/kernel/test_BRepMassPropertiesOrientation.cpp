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
// WHAT THIS DOES NOT FIX, measured and bounded below rather than implied: over 800
// configurations (2443 bodies) the count whose analytic volume disagrees with its own fine
// tessellation goes from 251 to 94, and the worst disagreement from 7.6e+02 to 1.3e+01.
// The analytic conservation identity U+I == A+B goes from 286 violations of 624 to 84, and
// its worst from 9.8e-01 to 2.8e-02. Much better, and not finished.

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

// The centred case, which was already right, so the fix is not paying for itself elsewhere.
TEST(BRepMassPropertiesOrientation, TheCentredUnionIsStillRight)
{
    const Body U = booleanToBody(makeBox(2.f, 2.f, 2.f), makeSphere(1.2f, 8, 12),
                                 BooleanOp::Union);
    ASSERT_GT(U.faceCount(), 0u);
    const double analytic = U.massProperties().volume;
    const double tess = tessVolume(U, 6);
    EXPECT_NEAR(analytic, tess, 0.01 * tess)
        << "analytic " << analytic << " vs tessellated " << tess;
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

// A deterministic sweep, bounded on BOTH sides. This is the measurement that says how far
// the repair goes and how far it does not: a body's analytic volume should agree with its
// own fine tessellation to within the chord error, and some still do not.
TEST(BRepMassPropertiesOrientation, TheRemainingDisagreementIsBounded)
{
    int bodies = 0, bad = 0;
    double worst = 0.0;
    auto check = [&](const Body& b) {
        if (b.faceCount() == 0u) return;
        const double t = tessVolume(b, 6);
        if (t <= 1e-9) return;
        ++bodies;
        const double rel = (static_cast<double>(b.massProperties().volume) - t) / t;
        if (rel < -1e-3 || rel > 0.10) {
            ++bad;
            worst = std::max(worst, std::abs(rel));
        }
    };

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
            Body Ai = A, Bi = B;
            if (!imprintMutually(Ai, Bi)) continue;
            check(Ai);
            check(Bi);
            for (const BooleanOp op :
                 {BooleanOp::Union, BooleanOp::Intersection, BooleanOp::Difference})
                check(booleanToBody(A, B, op));
        }
    }

    EXPECT_GT(bodies, 40) << "the sweep did not run";
    // Measured with this repair. Lower bound so a silent fix is noticed and the bound
    // retired; upper bound so it cannot grow.
    EXPECT_LE(bad, 12) << bad << " of " << bodies
                       << " bodies disagree with their own tessellation (worst " << worst
                       << ") — the parametric integrator has regressed";
    EXPECT_GT(bad, 0) << "no body disagrees any more — the rest of the integrator's "
                         "orientation handling has been fixed, so retire this bound";
}

}  // namespace nexus::geometry::brep::testing
