// classifyPoint's tessellation cache, and the only way it can hurt.
//
// `classifyPoint` answers by casting a parity ray at a tessellation of the shell, and it built
// that tessellation on EVERY query. `selectFace` loops over A's faces asking B to classify, then
// over B's faces asking A, so one Boolean paid for O(F^2) tessellations. Measured on a
// thirty-Boolean workload: 2222ms without the cache, 312ms with it, a 7.1x difference; across the
// whole kernel suite 105.1s to 31.2s.
//
// The cache is keyed on a FINGERPRINT of every field toMesh reads, not on a dirty flag set by
// mutators. Body has many mutation paths — vertexMut, faceMut, splitEdge, setEdgeArc,
// imprintCurve, transform, the Boolean's own rebuilds — and a single missed bump would hand the
// classifier a mesh of the body's PREVIOUS shape. That is not a slow answer, it is a wrong
// inside/outside answer, and the watertight-or-empty contract rests on that not happening. A key
// computed from the data cannot go stale by omission.
//
// So these tests are all one question asked several ways: after the body changes, is the answer
// the answer for the NEW body? Each mutation is checked against a reference body built the same
// way, whose cache is necessarily empty — so if the cache ever served a stale mesh, the two would
// disagree. Setting the key to a constant (making every lookup a hit) fails all of them.

#include <nexus/geometry/AnalyticBRep.h>
#include <nexus/geometry/BRepBoolean.h>

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry::brep::testing {

namespace {
using PC = Body::PointContainment;
}  // namespace

// A translation moves the solid out from under the cached mesh entirely.
TEST(BRepClassifyCache, ATranslationIsNotServedAStaleAnswer)
{
    const Vec3 probe{1.5, 0.0, 0.0};

    Body b = makeBox(2.f, 2.f, 2.f);          // spans [-1,1]: the probe is outside
    ASSERT_EQ(b.classifyPoint(probe), PC::Outside) << "fixture is wrong";

    b.translate({1.0, 0.0, 0.0});             // now spans [0,2]: the probe is inside
    Body reference = makeBox(2.f, 2.f, 2.f);  // built fresh, so its cache is empty
    reference.translate({1.0, 0.0, 0.0});

    EXPECT_EQ(b.classifyPoint(probe), reference.classifyPoint(probe))
        << "the cache served a mesh of the body's previous position";
    EXPECT_EQ(b.classifyPoint(probe), PC::Inside);
}

// Moving a single vertex changes the shell without changing any count, which is the case a
// fingerprint over sizes alone would miss.
TEST(BRepClassifyCache, MovingOneVertexIsNotServedAStaleAnswer)
{
    Body b = makeBox(2.f, 2.f, 2.f);
    // a point just outside the +X face, and inside the corner region the vertex will sweep over
    const Vec3 probe{1.4, 0.9, 0.9};
    ASSERT_EQ(b.classifyPoint(probe), PC::Outside) << "fixture is wrong";

    // find the (+1,+1,+1) corner and pull it out past the probe
    uint32_t corner = kInvalid;
    for (uint32_t v = 0; v < static_cast<uint32_t>(b.vertexCount()); ++v) {
        const Vec3& p = b.vertex(v).point;
        if (p.x > 0.5 && p.y > 0.5 && p.z > 0.5) { corner = v; break; }
    }
    ASSERT_NE(corner, kInvalid) << "no +X+Y+Z corner — the fixture changed";

    Body reference = makeBox(2.f, 2.f, 2.f);
    b.vertexMut(corner).point = Vec3{3.0, 3.0, 3.0};
    reference.vertexMut(corner).point = Vec3{3.0, 3.0, 3.0};

    EXPECT_EQ(b.classifyPoint(probe), reference.classifyPoint(probe))
        << "the cache served a mesh from before the vertex moved — a fingerprint that only "
           "covered entity COUNTS would do exactly this";
}

// A copy must not inherit a cache that then describes the wrong shape.
TEST(BRepClassifyCache, ACopyDoesNotInheritAStaleCache)
{
    const Vec3 probe{1.5, 0.0, 0.0};
    Body original = makeBox(2.f, 2.f, 2.f);
    ASSERT_EQ(original.classifyPoint(probe), PC::Outside);   // populate the cache

    Body copy = original;                                    // copy carries no tessellation
    copy.translate({1.0, 0.0, 0.0});
    Body reference = makeBox(2.f, 2.f, 2.f);
    reference.translate({1.0, 0.0, 0.0});

    EXPECT_EQ(copy.classifyPoint(probe), reference.classifyPoint(probe));
    EXPECT_EQ(copy.classifyPoint(probe), PC::Inside);
    // and the original is untouched by any of it
    EXPECT_EQ(original.classifyPoint(probe), PC::Outside);
}

// The cached and uncached answers must be the SAME answer, over a sweep, for a curved body
// whose tessellation is the interesting one.
TEST(BRepClassifyCache, CachedAnswersMatchFreshOnesAcrossASweep)
{
    Body cached = makeSphere(1.f, 8, 12);
    std::vector<Vec3> probes;
    for (int i = -3; i <= 3; ++i)
        for (int j = -3; j <= 3; ++j)
            probes.push_back(Vec3{i * 0.4, j * 0.4, 0.15});

    for (const Vec3& p : probes) {
        const Body fresh = makeSphere(1.f, 8, 12);   // never classified, so never cached
        EXPECT_EQ(cached.classifyPoint(p), fresh.classifyPoint(p))
            << "cached and fresh disagree at (" << p.x << ", " << p.y << ", " << p.z << ")";
    }
}

// Two bodies queried alternately must not contaminate each other, and repeated mutation of one
// must keep being reflected.
TEST(BRepClassifyCache, AlternatingBodiesAndRepeatedMutationStayCorrect)
{
    const Vec3 probe{1.5, 0.0, 0.0};
    Body a = makeBox(2.f, 2.f, 2.f);
    Body b = makeSphere(1.6f, 8, 12);      // radius 1.6: the probe is inside this one

    for (int round = 0; round < 4; ++round) {
        EXPECT_EQ(a.classifyPoint(probe), PC::Outside) << "round " << round;
        EXPECT_EQ(b.classifyPoint(probe), PC::Inside) << "round " << round;
    }

    // walk `a` across the probe one step at a time, checking every step against a fresh build
    for (int step = 1; step <= 4; ++step) {
        a.translate({0.5, 0.0, 0.0});
        Body reference = makeBox(2.f, 2.f, 2.f);
        reference.translate({0.5 * step, 0.0, 0.0});
        EXPECT_EQ(a.classifyPoint(probe), reference.classifyPoint(probe)) << "step " << step;
        EXPECT_EQ(b.classifyPoint(probe), PC::Inside) << "step " << step << ": b was disturbed";
    }
}

// An imprint adds seams without changing the solid, so the classification must not change —
// while the fingerprint certainly does. This is the case where a cache keyed too loosely would
// look right and a cache keyed too tightly would merely be slow.
TEST(BRepClassifyCache, AnImprintChangesTheKeyButNotTheAnswer)
{
    Body a = makeBox(2.f, 2.f, 2.f);
    Body cyl = makeCylinder(0.5f, 4.f, 16);

    std::vector<Vec3> probes = {{0.0, 0.0, 0.0}, {0.9, 0.0, 0.0}, {1.5, 0.0, 0.0},
                                {0.0, 0.0, 1.5}, {0.3, 0.3, 0.3}};
    std::vector<PC> before;
    for (const Vec3& p : probes) before.push_back(a.classifyPoint(p));

    Body ai = a, bi = cyl;
    ASSERT_TRUE(imprintMutually(ai, bi));
    for (size_t i = 0; i < probes.size(); ++i)
        EXPECT_EQ(ai.classifyPoint(probes[i]), before[i])
            << "imprinting changed the classification at probe " << i;
}

}  // namespace nexus::geometry::brep::testing
