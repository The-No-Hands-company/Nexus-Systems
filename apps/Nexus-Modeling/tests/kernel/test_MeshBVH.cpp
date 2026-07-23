#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBVH.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshBVH, BuildSucceedsForValidQuad)
{
    Mesh quad = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(quad.isValid());

    MeshBVH bvh;
    bvh.build(quad);

    EXPECT_TRUE(bvh.isValid());
    EXPECT_FALSE(bvh.nodes().empty());
    EXPECT_FALSE(bvh.tris().empty());
}

TEST(MeshBVH, RayFromFrontHitsQuad)
{
    Mesh quad = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(quad.isValid());

    MeshBVH bvh;
    bvh.build(quad);
    ASSERT_TRUE(bvh.isValid());

    Ray ray;
    ray.origin = {0.f, 3.f, 0.f};
    ray.direction = {0.f, -1.f, 0.f};

    RayHit hit = bvh.raycast(ray);
    EXPECT_LT(hit.t, std::numeric_limits<float>::max());
    EXPECT_NEAR(hit.t, 3.f, 1e-4f);
}

TEST(MeshBVH, RayParallelToQuadMisses)
{
    Mesh quad = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(quad.isValid());

    MeshBVH bvh;
    bvh.build(quad);
    ASSERT_TRUE(bvh.isValid());

    Ray ray;
    ray.origin = {2.f, 2.f, 0.f};
    ray.direction = {1.f, 0.f, 0.f};

    RayHit hit = bvh.raycast(ray);
    EXPECT_EQ(hit.t, std::numeric_limits<float>::max());
    EXPECT_EQ(hit.triangleIndex, 0u);
}

TEST(MeshBVH, RayFromOtherSideHitsBackFace)
{
    Mesh quad = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(quad.isValid());

    MeshBVH bvh;
    bvh.build(quad);
    ASSERT_TRUE(bvh.isValid());

    Ray ray;
    ray.origin = {0.f, -3.f, 0.f};
    ray.direction = {0.f, 1.f, 0.f};

    RayHit hit = bvh.raycast(ray);
    EXPECT_LT(hit.t, std::numeric_limits<float>::max());
    EXPECT_NEAR(hit.t, 3.f, 1e-4f);
}

TEST(MeshBVH, EmptyMeshBuildFails)
{
    Mesh empty;
    EXPECT_FALSE(empty.isValid());

    MeshBVH bvh;
    bvh.build(empty);

    EXPECT_FALSE(bvh.isValid());
    EXPECT_TRUE(bvh.nodes().empty());
    EXPECT_TRUE(bvh.tris().empty());
}

TEST(MeshBVH, HitPropertiesValidForFrontRay)
{
    Mesh quad = makePlane(2.f, 2.f, 1, 1);
    ASSERT_TRUE(quad.isValid());

    MeshBVH bvh;
    bvh.build(quad);
    ASSERT_TRUE(bvh.isValid());

    Ray ray;
    ray.origin = {0.f, 3.f, 0.f};
    ray.direction = {0.f, -1.f, 0.f};

    RayHit hit = bvh.raycast(ray);
    EXPECT_LT(hit.t, std::numeric_limits<float>::max());
    EXPECT_EQ(hit.triangleIndex, 0u);
    EXPECT_GE(hit.u, 0.f);
    EXPECT_LE(hit.u, 1.f);
    EXPECT_GE(hit.v, 0.f);
    EXPECT_LE(hit.v, 1.f);
    EXPECT_LE(hit.u + hit.v, 1.f + 1e-5f);
}

TEST(MeshBVH, NodeHierarchyIsConsistent)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    MeshBVH bvh;
    bvh.build(box);
    ASSERT_TRUE(bvh.isValid());

    const auto& nodes = bvh.nodes();
    for (size_t i = 0; i < nodes.size(); ++i) {
        const auto& n = nodes[i];
        if (n.isLeaf) {
            EXPECT_GE(n.triCount, 0);
            EXPECT_GE(n.firstTri, 0);
            continue;
        }
        // Internal node: children must exist and be within this node's bounds
        if (n.leftChild >= 0) {
            const auto& left = nodes[static_cast<size_t>(n.leftChild)];
            EXPECT_GE(left.min.x, n.min.x - 1e-5f);
            EXPECT_GE(left.min.y, n.min.y - 1e-5f);
            EXPECT_GE(left.min.z, n.min.z - 1e-5f);
            EXPECT_LE(left.max.x, n.max.x + 1e-5f);
            EXPECT_LE(left.max.y, n.max.y + 1e-5f);
            EXPECT_LE(left.max.z, n.max.z + 1e-5f);
        }
        if (n.leftChild >= 0 && n.leftChild + 1 < static_cast<int32_t>(nodes.size())) {
            const auto& right = nodes[static_cast<size_t>(n.leftChild + 1)];
            EXPECT_GE(right.min.x, n.min.x - 1e-5f);
            EXPECT_GE(right.min.y, n.min.y - 1e-5f);
            EXPECT_GE(right.min.z, n.min.z - 1e-5f);
            EXPECT_LE(right.max.x, n.max.x + 1e-5f);
            EXPECT_LE(right.max.y, n.max.y + 1e-5f);
            EXPECT_LE(right.max.z, n.max.z + 1e-5f);
        }
    }
}

TEST(MeshBVH, AllTrianglesReachable)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    MeshBVH bvh;
    bvh.build(box);
    ASSERT_TRUE(bvh.isValid());

    std::vector<bool> covered(bvh.tris().size(), false);
    for (const auto& n : bvh.nodes()) {
        if (!n.isLeaf) continue;
        for (int32_t j = 0; j < n.triCount; ++j) {
            int32_t idx = n.firstTri + j;
            ASSERT_GE(idx, 0);
            ASSERT_LT(static_cast<size_t>(idx), covered.size());
            covered[static_cast<size_t>(idx)] = true;
        }
    }
    for (size_t i = 0; i < covered.size(); ++i) {
        EXPECT_TRUE(covered[i]) << "triangle " << i << " not in any BVH leaf";
    }
}

TEST(MeshBVH, ClosestPointFindsQuery)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    MeshBVH bvh;
    bvh.build(box);
    ASSERT_TRUE(bvh.isValid());

    nexus::render::Vec3 query{2.0f, 2.0f, 2.0f};
    ClosestPointHit hit = bvh.closestPoint(query);
    EXPECT_TRUE(hit.valid);
    EXPECT_GT(hit.distanceSquared, 0.0f);
    EXPECT_LT(hit.distanceSquared, std::numeric_limits<float>::max());
}

TEST(MeshBVH, DeterministicBuild)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    MeshBVH bvhA, bvhB;
    bvhA.build(box);
    bvhB.build(box);

    ASSERT_EQ(bvhA.nodes().size(), bvhB.nodes().size());
    ASSERT_EQ(bvhA.tris().size(), bvhB.tris().size());

    for (size_t i = 0; i < bvhA.nodes().size(); ++i) {
        EXPECT_FLOAT_EQ(bvhA.nodes()[i].min.x, bvhB.nodes()[i].min.x);
        EXPECT_FLOAT_EQ(bvhA.nodes()[i].min.y, bvhB.nodes()[i].min.y);
        EXPECT_FLOAT_EQ(bvhA.nodes()[i].min.z, bvhB.nodes()[i].min.z);
        EXPECT_FLOAT_EQ(bvhA.nodes()[i].max.x, bvhB.nodes()[i].max.x);
        EXPECT_FLOAT_EQ(bvhA.nodes()[i].max.y, bvhB.nodes()[i].max.y);
        EXPECT_FLOAT_EQ(bvhA.nodes()[i].max.z, bvhB.nodes()[i].max.z);
    }
}

// ── Larger-mesh coverage ────────────────────────────────────────────────────────
//
// Every test above uses a quad. A quad never splits a node, so none of them could see
// either of two build defects that made this structure return wrong answers on any real
// mesh: leaves addressed a triangle array the build had permuted out from under them, and
// the depth cut-off compared the total node COUNT against a depth limit, capping the tree
// at ~71 nodes for a model of any size. The first produced wrong query results; the second
// hid it, because leaves then spanned half the mesh and happened to contain the answer.
//
// These exercise a mesh big enough to have a real hierarchy, and check the queries against
// exhaustive scans rather than against hand-picked expectations.

namespace {

nexus::geometry::Mesh tessellatedSphere()
{
    nexus::geometry::Mesh m = nexus::geometry::primitives::makeSphere(1.2f, 24, 28);
    (void)m.topology().triangulate();
    return m;
}

// Closest squared distance from p to triangle abc (Ericson region test).
float exhaustiveTriDist2(const nexus::render::Vec3& p, const nexus::render::Vec3& a,
                         const nexus::render::Vec3& b, const nexus::render::Vec3& c)
{
    const nexus::render::Vec3 ab{b.x - a.x, b.y - a.y, b.z - a.z};
    const nexus::render::Vec3 ac{c.x - a.x, c.y - a.y, c.z - a.z};
    const nexus::render::Vec3 ap{p.x - a.x, p.y - a.y, p.z - a.z};
    const float d1 = ab.x * ap.x + ab.y * ap.y + ab.z * ap.z;
    const float d2 = ac.x * ap.x + ac.y * ap.y + ac.z * ap.z;
    nexus::render::Vec3 q;
    if (d1 <= 0.f && d2 <= 0.f) {
        q = a;
    } else {
        const nexus::render::Vec3 bp{p.x - b.x, p.y - b.y, p.z - b.z};
        const float d3 = ab.x * bp.x + ab.y * bp.y + ab.z * bp.z;
        const float d4 = ac.x * bp.x + ac.y * bp.y + ac.z * bp.z;
        const nexus::render::Vec3 cp{p.x - c.x, p.y - c.y, p.z - c.z};
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

}  // namespace

TEST(MeshBVH, ClosestPointMatchesExhaustiveScanOnATessellatedMesh)
{
    using namespace nexus::geometry;
    const Mesh m = tessellatedSphere();
    MeshBVH bvh;
    bvh.build(m);
    ASSERT_TRUE(bvh.isValid());
    ASSERT_GT(bvh.tris().size(), 1000u) << "mesh too small to build a real hierarchy";

    std::mt19937 rng(7u);
    std::uniform_real_distribution<float> u(-2.f, 2.f);
    int disagreements = 0;
    for (int i = 0; i < 1500; ++i) {
        const nexus::render::Vec3 p{u(rng), u(rng), u(rng)};
        float best = std::numeric_limits<float>::max();
        for (const auto& t : bvh.tris()) {
            const nexus::render::Vec3 b{t.v0.x + t.e1.x, t.v0.y + t.e1.y, t.v0.z + t.e1.z};
            const nexus::render::Vec3 c{t.v0.x + t.e2.x, t.v0.y + t.e2.y, t.v0.z + t.e2.z};
            best = std::min(best, exhaustiveTriDist2(p, t.v0, b, c));
        }
        const ClosestPointHit hit = bvh.closestPoint(p);
        ASSERT_TRUE(hit.valid) << "no closest point at query " << i;
        if (std::abs(hit.distanceSquared - best) > 1e-3f * std::max(best, 1e-6f)) ++disagreements;
    }
    // Only float-rounding differences are tolerated; 2,764 of 3,000 disagreed before the
    // triangle-ordering fix, and always by reporting too LARGE a distance.
    EXPECT_LE(disagreements, 5) << disagreements
                                << " of 1500 closest-point queries disagreed with an "
                                   "exhaustive scan";
}

TEST(MeshBVH, RaycastMatchesExhaustiveScanOnATessellatedMesh)
{
    using namespace nexus::geometry;
    const Mesh m = tessellatedSphere();
    MeshBVH bvh;
    bvh.build(m);
    ASSERT_TRUE(bvh.isValid());

    std::mt19937 rng(99u);
    std::uniform_real_distribution<float> u(-1.f, 1.f);
    int disagreements = 0, hits = 0;
    for (int i = 0; i < 800; ++i) {
        const nexus::render::Vec3 o{u(rng) * 3.f, u(rng) * 3.f, u(rng) * 3.f};
        nexus::render::Vec3 d{-o.x + u(rng) * 0.3f, -o.y + u(rng) * 0.3f, -o.z + u(rng) * 0.3f};
        const float len = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        if (len < 1e-6f) continue;
        d = {d.x / len, d.y / len, d.z / len};

        // Exhaustive nearest hit (Möller–Trumbore over every triangle).
        float bestT = std::numeric_limits<float>::max();
        for (const auto& t : bvh.tris()) {
            const nexus::render::Vec3 pv{d.y * t.e2.z - d.z * t.e2.y, d.z * t.e2.x - d.x * t.e2.z,
                                         d.x * t.e2.y - d.y * t.e2.x};
            const float det = t.e1.x * pv.x + t.e1.y * pv.y + t.e1.z * pv.z;
            if (std::abs(det) < 1e-12f) continue;
            const float inv = 1.f / det;
            const nexus::render::Vec3 tv{o.x - t.v0.x, o.y - t.v0.y, o.z - t.v0.z};
            const float bu = (tv.x * pv.x + tv.y * pv.y + tv.z * pv.z) * inv;
            if (bu < 0.f || bu > 1.f) continue;
            const nexus::render::Vec3 qv{tv.y * t.e1.z - tv.z * t.e1.y, tv.z * t.e1.x - tv.x * t.e1.z,
                                         tv.x * t.e1.y - tv.y * t.e1.x};
            const float bv = (d.x * qv.x + d.y * qv.y + d.z * qv.z) * inv;
            if (bv < 0.f || bu + bv > 1.f) continue;
            const float tt = (t.e2.x * qv.x + t.e2.y * qv.y + t.e2.z * qv.z) * inv;
            if (tt > 1e-5f && tt < bestT) bestT = tt;
        }

        const RayHit hit = bvh.raycast(Ray{o, d});
        const bool exhaustiveHit = bestT < std::numeric_limits<float>::max();
        const bool bvhHit = hit.t < std::numeric_limits<float>::max();
        if (exhaustiveHit) ++hits;
        if (exhaustiveHit != bvhHit) { ++disagreements; continue; }
        if (exhaustiveHit && std::abs(hit.t - bestT) > 1e-3f * std::max(bestT, 1e-6f)) {
            ++disagreements;
        }
    }
    ASSERT_GT(hits, 100) << "battery degenerated — almost nothing was hit";
    EXPECT_LE(disagreements, 5) << disagreements << " raycasts disagreed with an exhaustive scan";
}

// The tree must keep subdividing as the mesh grows. The depth cut-off previously tested
// the node COUNT, so the tree stopped growing at ~71 nodes and a single leaf ended up
// holding half the model — the structure kept answering correctly by brute force while
// silently providing no acceleration at all.
TEST(MeshBVH, LeavesStayBoundedAsTheMeshGrows)
{
    using namespace nexus::geometry;
    for (const uint32_t seg : {16u, 24u, 32u}) {
        Mesh m = primitives::makeSphere(1.2f, seg, seg + 4);
        (void)m.topology().triangulate();
        MeshBVH bvh;
        bvh.build(m);
        ASSERT_TRUE(bvh.isValid());

        std::size_t maxLeaf = 0, leaves = 0;
        for (const auto& n : bvh.nodes()) {
            if (!n.isLeaf) continue;
            ++leaves;
            maxLeaf = std::max(maxLeaf, static_cast<std::size_t>(n.triCount));
        }
        const std::size_t tris = bvh.tris().size();
        EXPECT_LE(maxLeaf, 8u) << "a leaf holds " << maxLeaf << " of " << tris
                               << " triangles — the tree stopped subdividing";
        EXPECT_GT(leaves * 8u, tris / 2u) << "far too few leaves for " << tris << " triangles";
    }
}

// Segment candidates must be CONSERVATIVE: any triangle the segment actually passes
// through has to appear, or a parity count built on them silently inverts.
TEST(MeshBVH, SegmentCandidatesContainEveryTriangleTheSegmentCrosses)
{
    using namespace nexus::geometry;
    const Mesh m = tessellatedSphere();
    MeshBVH bvh;
    bvh.build(m);
    ASSERT_TRUE(bvh.isValid());

    std::mt19937 rng(4242u);
    std::uniform_real_distribution<float> u(-2.5f, 2.5f);
    std::vector<uint32_t> candidates;
    int missed = 0, crossings = 0;
    for (int i = 0; i < 400; ++i) {
        const nexus::render::Vec3 a{u(rng), u(rng), u(rng)};
        const nexus::render::Vec3 b{u(rng), u(rng), u(rng)};
        bvh.collectSegmentCandidates(a, b, candidates);
        std::vector<char> present(bvh.tris().size(), 0);
        for (const uint32_t bi : candidates) {
            if (bi < present.size()) present[bi] = 1;
        }
        // Any triangle the segment truly crosses must be a candidate.
        const nexus::render::Vec3 d{b.x - a.x, b.y - a.y, b.z - a.z};
        for (std::size_t ti = 0; ti < bvh.tris().size(); ++ti) {
            const auto& t = bvh.tris()[ti];
            const nexus::render::Vec3 pv{d.y * t.e2.z - d.z * t.e2.y, d.z * t.e2.x - d.x * t.e2.z,
                                         d.x * t.e2.y - d.y * t.e2.x};
            const float det = t.e1.x * pv.x + t.e1.y * pv.y + t.e1.z * pv.z;
            if (std::abs(det) < 1e-12f) continue;
            const float inv = 1.f / det;
            const nexus::render::Vec3 tv{a.x - t.v0.x, a.y - t.v0.y, a.z - t.v0.z};
            const float bu = (tv.x * pv.x + tv.y * pv.y + tv.z * pv.z) * inv;
            if (bu < 0.f || bu > 1.f) continue;
            const nexus::render::Vec3 qv{tv.y * t.e1.z - tv.z * t.e1.y, tv.z * t.e1.x - tv.x * t.e1.z,
                                         tv.x * t.e1.y - tv.y * t.e1.x};
            const float bv = (d.x * qv.x + d.y * qv.y + d.z * qv.z) * inv;
            if (bv < 0.f || bu + bv > 1.f) continue;
            const float tt = (t.e2.x * qv.x + t.e2.y * qv.y + t.e2.z * qv.z) * inv;
            if (tt < 0.f || tt > 1.f) continue;  // outside the segment
            ++crossings;
            if (!present[ti]) ++missed;
        }
    }
    ASSERT_GT(crossings, 200) << "battery degenerated — the segments cross almost nothing";
    EXPECT_EQ(missed, 0) << missed << " of " << crossings
                         << " genuine segment crossings were absent from the candidate set";
}

// The box query is the cut's broad phase: which triangles of one mesh can possibly meet a
// given triangle of the other. Pairing them by brute force is quadratic — at 1,288 against
// 1,288 triangles it tested 1,658,944 pairs where the hierarchy proposes 4,271.
// Conservative in the same way as the segment query, so a caller can re-apply its own
// exact box test and get precisely the brute-force set.
TEST(MeshBVH, BoxCandidatesContainEveryOverlappingTriangle)
{
    using namespace nexus::geometry;
    const Mesh m = tessellatedSphere();
    MeshBVH bvh;
    bvh.build(m);
    ASSERT_TRUE(bvh.isValid());

    std::mt19937 rng(2024u);
    std::uniform_real_distribution<float> c(-1.6f, 1.6f), e(0.02f, 0.4f);
    std::vector<uint32_t> candidates;
    int missed = 0, overlaps = 0;
    for (int i = 0; i < 300; ++i) {
        const nexus::render::Vec3 centre{c(rng), c(rng), c(rng)};
        const float half = e(rng);
        const nexus::render::Vec3 lo{centre.x - half, centre.y - half, centre.z - half};
        const nexus::render::Vec3 hi{centre.x + half, centre.y + half, centre.z + half};

        bvh.collectBoxCandidates(lo, hi, candidates);
        std::vector<char> present(bvh.tris().size(), 0);
        for (const uint32_t bi : candidates) {
            if (bi < present.size()) present[bi] = 1;
        }
        for (std::size_t ti = 0; ti < bvh.tris().size(); ++ti) {
            const auto& t = bvh.tris()[ti];
            const nexus::render::Vec3 b{t.v0.x + t.e1.x, t.v0.y + t.e1.y, t.v0.z + t.e1.z};
            const nexus::render::Vec3 d{t.v0.x + t.e2.x, t.v0.y + t.e2.y, t.v0.z + t.e2.z};
            const float tlo[3] = {std::min({t.v0.x, b.x, d.x}), std::min({t.v0.y, b.y, d.y}),
                                  std::min({t.v0.z, b.z, d.z})};
            const float thi[3] = {std::max({t.v0.x, b.x, d.x}), std::max({t.v0.y, b.y, d.y}),
                                  std::max({t.v0.z, b.z, d.z})};
            const float qlo[3] = {lo.x, lo.y, lo.z};
            const float qhi[3] = {hi.x, hi.y, hi.z};
            bool overlap = true;
            for (int k = 0; k < 3; ++k) {
                if (thi[k] < qlo[k] || tlo[k] > qhi[k]) overlap = false;
            }
            if (!overlap) continue;
            ++overlaps;
            if (!present[ti]) ++missed;
        }
    }
    ASSERT_GT(overlaps, 500) << "battery degenerated — the boxes overlap almost nothing";
    EXPECT_EQ(missed, 0) << missed << " of " << overlaps
                         << " overlapping triangles were absent from the candidate set";
}
