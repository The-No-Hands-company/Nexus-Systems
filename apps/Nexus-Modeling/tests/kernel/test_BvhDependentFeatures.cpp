// The features that sit on MeshBVH, checked against independent references.
//
// MeshBVH was returning the wrong nearest triangle on any mesh large enough to split a
// node — for as long as it had existed — and seven features are built on that query:
// closest-point, signed distance fields, Hausdorff distance, Poisson-disk sampling,
// snapping, quad remeshing. Every one of them had tests. Every one of those tests passed
// while the structure underneath was wrong, and passed again once it was fixed, which
// means not one of them ever exercised the broken path.
//
// That is the gap this file closes. Each check below uses a mesh with enough triangles
// that the hierarchy must actually work, and compares against a reference derived
// independently — an exhaustive scan, or a property the answer must satisfy — rather than
// against a value someone wrote down once. A test that cannot fail when the layer beneath
// it is broken is not protecting anything.

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshClosestPoint.h>
#include <nexus/geometry/MeshHausdorffDistance.h>
#include <nexus/geometry/MeshPoissonDisk.h>
#include <nexus/geometry/MeshSignedDistanceField.h>
#include <nexus/geometry/SnappingEngine.h>

#include <SplitMix64.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

using namespace nexus::geometry;
using nexus::render::Vec3;

namespace {

// A mesh big enough that the hierarchy has real depth. The old tests used a quad.
Mesh referenceSphere() { return primitives::makeSphere(1.2f, 24, 28); }

struct TriSoup {
    std::vector<Vec3> a, b, c;
};

TriSoup soupOf(const Mesh& src)
{
    Mesh m = src;
    (void)m.topology().triangulate();
    const auto& p = m.attributes().positions();
    TriSoup s;
    for (std::size_t f = 0; f < m.topology().faceCount(); ++f) {
        const auto& fc = m.topology().face(f);
        if (fc.indices.size() != 3) continue;
        s.a.push_back(p[fc.indices[0]]);
        s.b.push_back(p[fc.indices[1]]);
        s.c.push_back(p[fc.indices[2]]);
    }
    return s;
}

float triDist2(const Vec3& p, const Vec3& a, const Vec3& b, const Vec3& c)
{
    const Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
    const Vec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
    const Vec3 ap{p.x - a.x, p.y - a.y, p.z - a.z};
    const float d1 = ab.x * ap.x + ab.y * ap.y + ab.z * ap.z;
    const float d2 = ac.x * ap.x + ac.y * ap.y + ac.z * ap.z;
    Vec3 q;
    if (d1 <= 0.f && d2 <= 0.f) {
        q = a;
    } else {
        const Vec3 bp{p.x - b.x, p.y - b.y, p.z - b.z};
        const float d3 = ab.x * bp.x + ab.y * bp.y + ab.z * bp.z;
        const float d4 = ac.x * bp.x + ac.y * bp.y + ac.z * bp.z;
        const Vec3 cp{p.x - c.x, p.y - c.y, p.z - c.z};
        const float d5 = ab.x * cp.x + ab.y * cp.y + ab.z * cp.z;
        const float d6 = ac.x * cp.x + ac.y * cp.y + ac.z * cp.z;
        const float vc = d1 * d4 - d3 * d2, vb = d5 * d2 - d1 * d6, va = d3 * d6 - d5 * d4;
        if (d3 >= 0.f && d4 <= d3) q = b;
        else if (d6 >= 0.f && d5 <= d6) q = c;
        else if (vc <= 0.f && d1 >= 0.f && d3 <= 0.f) {
            const float t = d1 / (d1 - d3);
            q = {a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t};
        } else if (vb <= 0.f && d2 >= 0.f && d6 <= 0.f) {
            const float t = d2 / (d2 - d6);
            q = {a.x + ac.x * t, a.y + ac.y * t, a.z + ac.z * t};
        } else if (va <= 0.f && (d4 - d3) >= 0.f && (d5 - d6) >= 0.f) {
            const float t = (d4 - d3) / ((d4 - d3) + (d5 - d6));
            q = {b.x + (c.x - b.x) * t, b.y + (c.y - b.y) * t, b.z + (c.z - b.z) * t};
        } else {
            const float den = 1.f / (va + vb + vc), v = vb * den, w = vc * den;
            q = {a.x + ab.x * v + ac.x * w, a.y + ab.y * v + ac.y * w, a.z + ab.z * v + ac.z * w};
        }
    }
    const float dx = p.x - q.x, dy = p.y - q.y, dz = p.z - q.z;
    return dx * dx + dy * dy + dz * dz;
}

float exhaustiveDist2(const Vec3& q, const TriSoup& s)
{
    float best = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < s.a.size(); ++i) {
        best = std::min(best, triDist2(q, s.a[i], s.b[i], s.c[i]));
    }
    return best;
}

}  // namespace

TEST(BvhDependentFeatures, ClosestPointMatchesExhaustiveScan)
{
    const Mesh sphere = referenceSphere();
    const TriSoup soup = soupOf(sphere);
    ASSERT_GT(soup.a.size(), 1000u) << "reference mesh too small to exercise the hierarchy";

    std::mt19937 rng(31337u);
    std::uniform_real_distribution<float> u(-2.f, 2.f);
    std::vector<Vec3> queries;
    for (int i = 0; i < 600; ++i) queries.push_back({u(rng), u(rng), u(rng)});

    const MeshClosestBatch batch = MeshClosestPoint::query(sphere, queries);
    ASSERT_EQ(batch.hits.size(), queries.size());

    int wrong = 0;
    for (std::size_t i = 0; i < queries.size(); ++i) {
        const float ref = exhaustiveDist2(queries[i], soup);
        if (std::abs(batch.hits[i].distanceSquared - ref) > 1e-3f * std::max(ref, 1e-6f)) ++wrong;
    }
    EXPECT_EQ(wrong, 0) << wrong << " of " << queries.size()
                        << " closest-point queries disagreed with an exhaustive scan";
}

// Magnitude AND sign. The distance comes from the hierarchy; the sign comes from a winding
// number, and a field that is right about how far but wrong about which side is worse than
// useless to anything that marches it.
TEST(BvhDependentFeatures, SignedDistanceFieldMatchesExhaustiveScanIncludingSign)
{
    const Mesh sphere = referenceSphere();
    const TriSoup soup = soupOf(sphere);

    SDFOptions opts;
    opts.resolution = {16, 16, 16};
    opts.origin = {-2.f, -2.f, -2.f};
    opts.cellSize = {0.25f, 0.25f, 0.25f};
    const SDFResult sdf = MeshSignedDistanceField::compute(sphere, opts);
    ASSERT_EQ(sdf.distances.size(), 16u * 16u * 16u);

    int magnitudeWrong = 0, signWrong = 0, inside = 0;
    for (uint32_t z = 0; z < opts.resolution[2]; ++z) {
        for (uint32_t y = 0; y < opts.resolution[1]; ++y) {
            for (uint32_t x = 0; x < opts.resolution[0]; ++x) {
                // The field samples cell CENTRES.
                const Vec3 p{opts.origin.x + (static_cast<float>(x) + 0.5f) * opts.cellSize.x,
                             opts.origin.y + (static_cast<float>(y) + 0.5f) * opts.cellSize.y,
                             opts.origin.z + (static_cast<float>(z) + 0.5f) * opts.cellSize.z};
                const std::size_t idx = static_cast<std::size_t>(z) * 16u * 16u
                                      + static_cast<std::size_t>(y) * 16u + x;
                const float got = sdf.distances[idx];
                const float ref = std::sqrt(exhaustiveDist2(p, soup));
                if (std::abs(std::abs(got) - ref) > 1e-2f * std::max(ref, 1e-3f)) ++magnitudeWrong;

                // The sphere is centred at the origin, so the true side is known exactly.
                const float radial = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
                if (std::abs(radial - 1.2f) < 0.1f) continue;  // skip the boundary band
                const bool shouldBeInside = radial < 1.2f;
                if (shouldBeInside) ++inside;
                if (shouldBeInside != (got < 0.f)) ++signWrong;
            }
        }
    }
    EXPECT_EQ(magnitudeWrong, 0) << magnitudeWrong << " grid distances disagreed with an "
                                                     "exhaustive scan";
    ASSERT_GT(inside, 100) << "grid never sampled the interior — the sign check is vacuous";
    EXPECT_EQ(signWrong, 0) << signWrong << " grid points were on the wrong side of the surface";
}

TEST(BvhDependentFeatures, HausdorffOfAMeshAgainstItselfIsZero)
{
    const Mesh sphere = referenceSphere();
    HausdorffOptions opts;
    opts.sampleCount = 2000;
    const HausdorffResult h = MeshHausdorffDistance::compute(sphere, sphere, opts);
    EXPECT_LT(h.symmetricMax, 1e-4f) << "a mesh is not at zero distance from itself";
    EXPECT_LT(h.forwardMean, 1e-4f);
}

// A known displacement bounds the answer: shifting a closed surface by d can never move any
// point further than d from the original.
TEST(BvhDependentFeatures, HausdorffIsBoundedByAKnownTranslation)
{
    const Mesh sphere = referenceSphere();
    Mesh moved = sphere;
    auto p = moved.attributes().positions();
    for (auto& v : p) v.x += 0.05f;
    moved.attributes().setPositions(std::move(p));

    HausdorffOptions opts;
    opts.sampleCount = 2000;
    const HausdorffResult h = MeshHausdorffDistance::compute(sphere, moved, opts);
    EXPECT_GT(h.symmetricMax, 0.f) << "a displaced mesh reads as identical";
    EXPECT_LE(h.symmetricMax, 0.05f + 1e-2f)
        << "distance " << h.symmetricMax << " exceeds the 0.05 translation that produced it";
}

// Snapping to a point already on the surface must return that point.
TEST(BvhDependentFeatures, SnappingLandsOnTheSurfaceItWasBuiltFrom)
{
    const Mesh sphere = referenceSphere();
    SnappingEngine engine;
    engine.build(sphere);

    const TriSoup soup = soupOf(sphere);
    const auto& verts = sphere.attributes().positions();
    ASSERT_GT(verts.size(), 100u);

    std::mt19937 rng(5150u);
    int checked = 0, offSurface = 0;
    for (int i = 0; i < 200; ++i) {
        const Vec3& v = verts[rng() % verts.size()];
        const SnapResult r = engine.snap(v);
        if (r.target == SnapTarget::None) continue;
        ++checked;
        // Whatever it snapped to must itself lie on the mesh.
        if (exhaustiveDist2(r.position, soup) > 1e-4f) ++offSurface;
    }
    ASSERT_GT(checked, 50) << "snapping returned nothing for almost every query";
    EXPECT_EQ(offSurface, 0) << offSurface << " snap results were not on the source surface";
}

// Poisson-disk sampling promises a minimum separation. Verify the promise rather than a
// point count, and verify the samples lie on the surface they were drawn from.
TEST(BvhDependentFeatures, PoissonDiskSamplesRespectMinimumDistanceAndLieOnTheSurface)
{
    const Mesh sphere = referenceSphere();
    const TriSoup soup = soupOf(sphere);

    PoissonDiskOptions opts;
    opts.minDist = 0.25f;
    opts.seed = 99u;
    const PoissonDiskResult res = MeshPoissonDisk::sample(sphere, opts);
    ASSERT_GT(res.points.size(), 20u) << "sampler produced almost nothing";

    int tooClose = 0, offSurface = 0;
    for (std::size_t i = 0; i < res.points.size(); ++i) {
        if (exhaustiveDist2(res.points[i], soup) > 1e-4f) ++offSurface;
        for (std::size_t j = i + 1; j < res.points.size(); ++j) {
            const float dx = res.points[i].x - res.points[j].x;
            const float dy = res.points[i].y - res.points[j].y;
            const float dz = res.points[i].z - res.points[j].z;
            // Allow a small slack: the constraint is enforced in float.
            if (dx * dx + dy * dy + dz * dz < (opts.minDist * 0.9f) * (opts.minDist * 0.9f)) {
                ++tooClose;
            }
        }
    }
    EXPECT_EQ(offSurface, 0) << offSurface << " samples were not on the surface";
    EXPECT_EQ(tooClose, 0) << tooClose << " sample pairs violated the minimum separation";
}

// The kernel's random source, checked as a distribution rather than by eyeball.
//
// SplitMix64::uniform01 shifted its 64-bit output down to 53 bits and then divided by
// 2^64, so it returned values in [0, 0.000488] instead of [0, 1) — a constant for every
// practical purpose. Four features draw from it: Poisson-disk sampling, surface point
// sampling, ambient occlusion and the instancer. Poisson-disk sampling was reduced to a
// single point because every dart went the same direction at the same distance, and the
// instancer piled every instance on one spot. Neither failed a test: the sampler's tests
// asserted only that the result was non-empty, and the instancer's asserted a WIDTH that
// the collapse happened to satisfy.
//
// A generator is a distribution, so that is what gets asserted.
TEST(BvhDependentFeatures, RandomSourceIsUniformOverTheUnitInterval)
{
    nexus::geometry::internal::SplitMix64 rng(42u);
    constexpr int kDraws = 100000;
    constexpr int kBuckets = 10;
    int bucket[kBuckets] = {};
    double sum = 0.0, lo = 1.0, hi = 0.0;

    for (int i = 0; i < kDraws; ++i) {
        const double x = rng.uniform01();
        ASSERT_GE(x, 0.0);
        ASSERT_LT(x, 1.0) << "uniform01 returned " << x << ", outside [0,1)";
        sum += x;
        lo = std::min(lo, x);
        hi = std::max(hi, x);
        bucket[std::min(kBuckets - 1, static_cast<int>(x * kBuckets))]++;
    }

    EXPECT_NEAR(sum / kDraws, 0.5, 0.01) << "mean is not central";
    EXPECT_LT(lo, 0.01) << "never drew a small value";
    EXPECT_GT(hi, 0.99) << "never drew a large value — the range is collapsed";
    for (int b = 0; b < kBuckets; ++b) {
        // Each decile should hold about a tenth; generous bounds so this fails on a
        // collapsed range, not on sampling noise.
        EXPECT_GT(bucket[b], kDraws / kBuckets / 2)
            << "decile " << b << " holds only " << bucket[b] << " of " << kDraws;
        EXPECT_LT(bucket[b], kDraws / kBuckets * 2) << "decile " << b << " is overfull";
    }
}
