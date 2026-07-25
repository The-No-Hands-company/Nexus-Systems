// Foundation — PHASE M (mesh-boolean seam). The defect map, and the fix it led to.
//
// The map was re-measured from scratch after the exact-predicate rebuild, because the
// previous one had been drawn while orient2D/orient3D/inCircle returned wrong signs. That
// re-measurement found two live classes and refuted the old "coplanar-cap 3-way junction"
// diagnosis. Both classes turned out to have ONE cause, in the classification stage.
//
// THE CAUSE. Classification offset each sub-triangle's centroid along its normal by eps
// and asked the exact point-in-solid test on both sides, using the two answers both to
// decide the region AND to detect coincidence. That made a COMBINATORIAL decision depend
// on choosing eps well, which is not possible in general: eps must clear float noise yet
// stay closer than the nearest other sheet of surface. eps was derived from the model's
// BOUNDING BOX, while the distance to the nearest sheet is a LOCAL quantity that shrinks
// as a model is tessellated more finely. Measured on sphere-vs-box, the smallest
// sub-triangle produced by the cut fell from 3.5e-4 at 6x10 to 9.4e-7 at 32x36 while the
// offset stayed at 2e-4 — up to 200x larger than the triangle it was probing off — and the
// leak rate rose with it, 12% to 67%.
//
// THE FIX. Split the two questions along the kernel's governing rule — combinatorial
// decisions exact, metric quantities tolerant. The region test is now the exact
// Simulation-of-Simplicity parity at the centroid itself, with no offset at all.
// Coincidence is a genuine measurement: distance from the centroid to the other surface
// within a scale-aware tolerance, AND the two surfaces locally parallel (distance alone
// misreads ordinary seam slivers, since every seam face is close to the other surface —
// that is what a seam is). Outward orientation comes from the nearest original triangle of
// the face's own solid rather than a third probe.
//
// RESULT, measured over 2593 boolean runs: 38 leaks -> 1.
//   pinned battery         9 -> 0        broad systematic sweep   0 -> 0
//   randomized box/box     1 -> 0        curved sphere/cylinder  28 -> 1
// Class 2 (tessellation density) is closed through moderate density and much reduced at
// extreme density. Class 1 (near-coincident coplanar faces): a later fix (2026-07-25)
// localised the residual leak to the SEAM WELD — the cut and classification were already
// correct — and closed it by welding at 3x the coincidence tolerance T, so operands
// separated by anything clearly below OR clearly above T are now watertight (the old
// failure at sep=2T, on genuinely distinct geometry, is gone). What remains is only the
// razor-thin band at sep ≈ exactly T, the inherent ambiguity of any threshold coincidence
// test; both are pinned below.

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBooleanRobust.h>
#include <nexus/geometry/MeshCut.h>
#include <nexus/geometry/MeshMassProperties.h>
#include <nexus/geometry/MeshTopologyValidation.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <random>
#include <utility>
#include <vector>

namespace nexus::geometry::testing {

using nexus::render::Vec3;

namespace {

Mesh transformed(Mesh m, float scale, Vec3 offset)
{
    auto p = m.attributes().positions();
    for (auto& v : p) {
        v.x = v.x * scale + offset.x;
        v.y = v.y * scale + offset.y;
        v.z = v.z * scale + offset.z;
    }
    m.attributes().setPositions(std::move(p));
    return m;
}
Mesh translated(Mesh m, Vec3 o) { return transformed(std::move(m), 1.f, o); }
Mesh rotZ(Mesh m, float a)
{
    const float c = std::cos(a), s = std::sin(a);
    auto p = m.attributes().positions();
    for (auto& v : p) { const float x = v.x, y = v.y; v.x = c * x - s * y; v.y = s * x + c * y; }
    m.attributes().setPositions(std::move(p));
    return m;
}
Mesh rotY(Mesh m, float a)
{
    const float c = std::cos(a), s = std::sin(a);
    auto p = m.attributes().positions();
    for (auto& v : p) { const float x = v.x, z = v.z; v.x = c * x + s * z; v.z = -s * x + c * z; }
    m.attributes().setPositions(std::move(p));
    return m;
}

// -1 empty (no result to judge), 0 watertight, 1 leaks. boundaryLoops is the authoritative
// signal and is n-gon aware; a triangle-only edge count is not.
int leakState(const Mesh& r)
{
    if (r.topology().faceCount() == 0) return -1;
    return MeshTopologyValidation::validate(r).boundaryLoops != 0 ? 1 : 0;
}

float dist2(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}
bool onSegmentInterior(const Vec3& V, const Vec3& A, const Vec3& B)
{
    const Vec3 ab{B.x - A.x, B.y - A.y, B.z - A.z}, av{V.x - A.x, V.y - A.y, V.z - A.z};
    const float L2 = ab.x * ab.x + ab.y * ab.y + ab.z * ab.z;
    if (L2 < 1e-12f) return false;
    const float t = (av.x * ab.x + av.y * ab.y + av.z * ab.z) / L2;
    if (t <= 1e-4f || t >= 1.f - 1e-4f) return false;
    const Vec3 proj{A.x + ab.x * t, A.y + ab.y * t, A.z + ab.z * t};
    return dist2(V, proj) < 1e-8f * (1.f + L2);
}

struct Classes {
    int leaks = 0, holes = 0, nonManifoldEdge = 0, nonManifoldVertex = 0, tJunction = 0;
};

void classifyInto(Classes& c, const Mesh& r)
{
    if (leakState(r) != 1) return;
    ++c.leaks;
    const auto& pos = r.attributes().positions();
    std::map<std::pair<uint32_t, uint32_t>, int> edgeUse;
    for (size_t f = 0; f < r.topology().faceCount(); ++f) {
        const auto& fc = r.topology().face(f);
        if (fc.indices.size() != 3) continue;
        for (int e = 0; e < 3; ++e) {
            uint32_t a = fc.indices[e], b = fc.indices[(e + 1) % 3];
            if (a > b) std::swap(a, b);
            edgeUse[{a, b}]++;
        }
    }
    bool hole = false, nmE = false, tj = false;
    for (const auto& [edge, count] : edgeUse) {
        if (count >= 3) nmE = true;
        if (count == 1) {
            hole = true;
            for (uint32_t vi = 0; vi < pos.size(); ++vi) {
                if (vi == edge.first || vi == edge.second) continue;
                if (onSegmentInterior(pos[vi], pos[edge.first], pos[edge.second])) { tj = true; break; }
            }
        }
    }
    if (hole) ++c.holes;
    if (nmE) ++c.nonManifoldEdge;
    if (tj) ++c.tJunction;
    if (!hole && !nmE) ++c.nonManifoldVertex;
}

constexpr std::array<BooleanOperationType, 3> kOps = {BooleanOperationType::Union,
                                                      BooleanOperationType::Intersection,
                                                      BooleanOperationType::Difference};
}  // namespace

// Box/box booleans away from the near-coincident band are watertight. This is the headline
// of the re-measurement: the general case is SOLVED, which the previous map did not show
// because its battery was concentrated on degenerate offsets.
TEST(MeshBooleanSeam, GeneralPositionBoxBooleansAreWatertight)
{
    using primitives::makeBox;
    int runs = 0, leaks = 0;

    for (float dx = 0.125f; dx <= 3.01f; dx += 0.125f) {
        for (float ang : {0.f, 0.2f, 0.5f, 0.7853981634f, 1.0471975512f, 1.2f}) {
            for (float dz : {0.f, 0.37f}) {
                const Mesh a = makeBox(2.f, 2.f, 2.f);
                const Mesh b = translated(rotZ(makeBox(2.f, 2.f, 2.f), ang), {dx, 0.f, dz});
                for (auto op : kOps) {
                    ++runs;
                    if (leakState(robustMeshBoolean(a, b, op)) == 1) ++leaks;
                }
            }
        }
    }

    std::mt19937 rng(20260722u);
    std::uniform_real_distribution<float> sz(0.7f, 2.5f), tr(-2.2f, 2.2f), an(-3.14159f, 3.14159f);
    for (int i = 0; i < 150; ++i) {
        const Mesh a = makeBox(sz(rng), sz(rng), sz(rng));
        Mesh b = rotY(rotZ(makeBox(sz(rng), sz(rng), sz(rng)), an(rng)), an(rng));
        b = translated(std::move(b), {tr(rng), tr(rng), tr(rng)});
        for (auto op : kOps) {
            ++runs;
            if (leakState(robustMeshBoolean(a, b, op)) == 1) ++leaks;
        }
    }

    RecordProperty("generalPositionRuns", runs);
    RecordProperty("generalPositionLeaks", leaks);
    ASSERT_GT(runs, 1000) << "battery degenerated";
    // Measured 2026-07-22 after the classification fix: 0 leaks in 1314 runs.
    EXPECT_EQ(leaks, 0) << "general-position box booleans regressed: " << leaks << " of " << runs;
}

// CLASS 1, reduced to the inherent knife-edge. Near-coincident coplanar faces used to open
// the seam across a band up to 2x the coincidence tolerance — i.e. on genuinely DISTINCT
// geometry above the tolerance, which is a bug: a box offset by 2T should make a clean
// stepped union.
//
// Root cause (localised 2026-07-25): the cut and classification are correct there — cutR.a
// and cutR.b are each individually watertight, and the classifier keeps/drops the right
// faces. The hole was in the final SEAM WELD. When operands are separated by up to the
// coincidence tolerance T (which the classifier treats as coincident), the cut emits, at
// each shared corner, distinct vertices offset by up to T on each side, so a corner cluster
// spans up to 2·√2·T ≈ 2.83·T (the diagonal of a T-by-T offset). The weld ran at 1·T and
// could not merge that cluster, leaving a boundary loop. Welding at 3·T covers the cluster.
//
// What remains is the genuine ambiguity of any threshold test: a razor-thin band at
// separation ≈ exactly T (measured ~[0.99·T, T]), where the metric coincidence test
// d2 <= T^2 is decided by float rounding and neighbouring faces can fall on opposite sides.
// Every separation clearly below OR clearly above T is now watertight; only landing a face
// pair essentially exactly on the tolerance can still open the seam. That is inherent to a
// tolerance-based coincidence decision and is what a snap-to-tolerance modelling pre-pass
// would remove, not the boolean.
TEST(MeshBooleanSeam, NearCoincidentFacesResolveCleanlyExceptAtTheToleranceKnifeEdge)
{
    using primitives::makeBox;
    int clearLeaks = 0, knifeEdgeLeaks = 0;

    // Model size 2 -> coincidence tolerance T = 1e-5 (Tolerance::at, floor 1e-5). The
    // face separation is rel*2, so the knife edge sep ≈ T sits at rel ≈ 5e-6.
    auto count = [&](double rel, int& into) {
        const Mesh a = makeBox(2.f, 2.f, 2.f);
        const Mesh b = translated(makeBox(2.f, 2.f, 2.f), {static_cast<float>(rel * 2.0), 0.f, 0.f});
        for (auto op : kOps) {
            if (leakState(robustMeshBoolean(a, b, op)) == 1) ++into;
        }
    };

    // Clearly BELOW the tolerance (sep from 2e-9 up to ~9.6e-6): the pair is coincident and
    // must weld into one clean seam.
    for (double rel : {1e-9, 1e-8, 1e-7, 5e-7, 1e-6, 2e-6, 3e-6, 4e-6, 4.8e-6})
        count(rel, clearLeaks);
    // Clearly ABOVE the tolerance (sep from ~1.02e-5 up to 2e-1): the pair is distinct and
    // must make a clean stepped result. This is the range the old 1·T weld failed on.
    for (double rel : {5.1e-6, 6e-6, 8e-6, 1e-5, 1.4e-5, 2e-5, 5e-5, 1e-4, 3e-4, 1e-3, 1e-2, 1e-1})
        count(rel, clearLeaks);
    // The knife edge, sep ≈ exactly T: inherently ambiguous, may leak.
    for (double rel : {4.95e-6, 5e-6, 5.05e-6})
        count(rel, knifeEdgeLeaks);

    RecordProperty("clearOfToleranceLeaks", clearLeaks);
    RecordProperty("knifeEdgeLeaks", knifeEdgeLeaks);

    EXPECT_EQ(clearLeaks, 0)
        << "separations clearly below or above the coincidence tolerance must be watertight; "
           "the old failure at sep=2T (distinct geometry) is fixed by the 3T seam weld";
    // The knife edge is inherent; pin it so it cannot widen into the clear ranges above.
    EXPECT_LE(knifeEdgeLeaks, 9)
        << "the tolerance knife-edge widened beyond sep ≈ T";
}

// CLASS 2, CLOSED. This was the largest class — sphere-vs-box leak rates of 12%, 22%, 48%
// and 67% climbing with tessellation density. It is now zero at every density here,
// including the two that used to be the worst, after two fixes: classifying without a
// probe offset, and dropping the CAP sub-triangles the retriangulation emitted.
TEST(MeshBooleanSeam, CurvedOperandBooleansAreWatertightThroughModerateTessellation)
{
    using primitives::makeBox;
    for (uint32_t seg : {6u, 12u, 16u, 24u, 32u}) {
        const Mesh sphere = primitives::makeSphere(1.2f, seg, seg + 4);
        const auto sv = MeshTopologyValidation::validate(sphere);
        ASSERT_EQ(sv.boundaryLoops, 0u) << "the sphere INPUT is not closed at " << seg;
        ASSERT_EQ(sv.euler, 2) << "the sphere INPUT is not genus-0 at " << seg;

        int runs = 0, leaks = 0;
        std::mt19937 rng(4242u);
        std::uniform_real_distribution<float> t(-1.6f, 1.6f);
        for (int i = 0; i < 20; ++i) {
            const Mesh a = makeBox(2.f, 2.f, 2.f);
            const Mesh b = translated(sphere, {t(rng), t(rng), t(rng)});
            for (auto op : kOps) {
                ++runs;
                if (leakState(robustMeshBoolean(a, b, op)) == 1) ++leaks;
            }
        }
        RecordProperty("curvedLeaks_" + std::to_string(seg), leaks);
        EXPECT_EQ(leaks, 0) << "sphere " << seg << "x" << (seg + 4) << " leaked " << leaks
                            << " of " << runs << " — class 2 regressed";
    }
}

// The former extreme-density residual, now closed and asserted at zero. It was caused by
// cap sub-triangles, not by geometry running out of precision as first supposed: the
// retriangulation emitted a zero-area triangle spanning an edge that its own apex sat on,
// duplicating area the real sub-triangles already covered and leaving that edge without a
// partner. Dropping caps removed it.
TEST(MeshBooleanSeam, ExtremeTessellationIsWatertight)
{
    using primitives::makeBox;
    int runs = 0, leaks = 0;
    std::mt19937 rng(4242u);
    std::uniform_real_distribution<float> t(-1.6f, 1.6f);
    const Mesh sphere = primitives::makeSphere(1.2f, 24, 28);
    for (int i = 0; i < 20; ++i) {
        const Mesh a = makeBox(2.f, 2.f, 2.f);
        const Mesh b = translated(sphere, {t(rng), t(rng), t(rng)});
        for (auto op : kOps) {
            ++runs;
            if (leakState(robustMeshBoolean(a, b, op)) == 1) ++leaks;
        }
    }
    RecordProperty("extremeTessellationRuns", runs);
    RecordProperty("extremeTessellationLeaks", leaks);
    // 29 of 60 before the classification fix, 3 of 60 after it, 0 once caps are dropped.
    EXPECT_EQ(leaks, 0) << "extreme-tessellation booleans leaked again: " << leaks << " of "
                        << runs;
}

// SEVERITY, not just incidence. A leak that costs a handful of boundary edges is a small
// hole; one that costs thousands means the result disintegrated. The weld used to be
// all-or-nothing — a single collapsed sliver anywhere abandoned the ENTIRE weld and left
// every triangle isolated — so at high density a leak was often total. Measured at sphere
// 32x36: 10,926 boundary edges across the battery with a worst single result of 5,958,
// against 80 and 16 once the weld drops just the collapsed face.
//
// This bound is the guard on that. It fails loudly if the all-or-nothing behaviour ever
// returns, which the leak COUNT alone would barely register.
TEST(MeshBooleanSeam, ALeakIsALocalHoleNotADisintegratedResult)
{
    using primitives::makeBox;
    int worst = 0, total = 0;
    std::mt19937 rng(4242u);
    std::uniform_real_distribution<float> t(-1.6f, 1.6f);
    const Mesh sphere = primitives::makeSphere(1.2f, 32, 36);
    for (int i = 0; i < 20; ++i) {
        const Mesh a = makeBox(2.f, 2.f, 2.f);
        const Mesh b = translated(sphere, {t(rng), t(rng), t(rng)});
        for (auto op : kOps) {
            const Mesh r = robustMeshBoolean(a, b, op);
            if (leakState(r) != 1) continue;
            std::map<std::pair<uint32_t, uint32_t>, int> edgeUse;
            for (size_t f = 0; f < r.topology().faceCount(); ++f) {
                const auto& fc = r.topology().face(f);
                if (fc.indices.size() != 3) continue;
                for (int e = 0; e < 3; ++e) {
                    uint32_t x = fc.indices[e], y = fc.indices[(e + 1) % 3];
                    if (x > y) std::swap(x, y);
                    edgeUse[{x, y}]++;
                }
            }
            int bnd = 0;
            for (const auto& [k, c] : edgeUse) {
                if (c == 1) ++bnd;
            }
            total += bnd;
            worst = std::max(worst, bnd);
        }
    }
    RecordProperty("extremeBoundaryEdgesTotal", total);
    RecordProperty("extremeBoundaryEdgesWorst", worst);
    // Measured 2026-07-22: total 80, worst 16 (was 10,926 and 5,958).
    EXPECT_LE(worst, 40) << "a single result lost " << worst
                         << " boundary edges — the weld went all-or-nothing again";
    EXPECT_LE(total, 200) << "total boundary-edge damage rose to " << total;
}

// The cut emits no CAP sub-triangles, and its output is watertight at high density.
//
// A cap is a triangle whose third corner lies on its own opposite edge, enclosing no area.
// The retriangulation used to emit one wherever a seam point landed a hair off an edge:
// the correct sub-triangles that split through the point, AND a cap spanning the whole
// unsplit edge. The cap duplicated covered area — its two shared edges were each used
// THREE times while its long edge was used once and had no partner, which is exactly the
// leak. TriangleRetriangulate now drops them.
//
// Guarded by thinness, 2*area over the sum of squared edge lengths: dimensionless, ~0.29
// for an equilateral triangle and →0 as it degenerates, so the check means the same thing
// at any model scale.
TEST(MeshBooleanSeam, CutEmitsNoCapTrianglesAndStaysWatertightAtHighDensity)
{
    using primitives::makeBox;
    int cuts = 0, brokenCuts = 0, caps = 0, faces = 0;
    double thinnest = 1.0;
    std::mt19937 rng(4242u);
    std::uniform_real_distribution<float> t(-1.6f, 1.6f);
    const Mesh sphere = primitives::makeSphere(1.2f, 32, 36);

    for (int i = 0; i < 20; ++i) {
        const Mesh a = makeBox(2.f, 2.f, 2.f);
        const Mesh b = translated(sphere, {t(rng), t(rng), t(rng)});
        const MeshCutResult cr = MeshCut::cut(a, b);
        for (const Mesh* m : {&cr.a, &cr.b}) {
            ++cuts;
            if (MeshTopologyValidation::validate(*m).boundaryLoops != 0) ++brokenCuts;
            const auto& pos = m->attributes().positions();
            for (size_t f = 0; f < m->topology().faceCount(); ++f) {
                const auto& fc = m->topology().face(f);
                if (fc.indices.size() != 3) continue;
                ++faces;
                const Vec3& p0 = pos[fc.indices[0]];
                const Vec3& p1 = pos[fc.indices[1]];
                const Vec3& p2 = pos[fc.indices[2]];
                const double ux = p1.x - p0.x, uy = p1.y - p0.y, uz = p1.z - p0.z;
                const double vx = p2.x - p0.x, vy = p2.y - p0.y, vz = p2.z - p0.z;
                const double wx = p2.x - p1.x, wy = p2.y - p1.y, wz = p2.z - p1.z;
                const double cx = uy * vz - uz * vy, cy = uz * vx - ux * vz, cz = ux * vy - uy * vx;
                const double twiceArea = std::sqrt(cx * cx + cy * cy + cz * cz);
                const double sumSq = ux * ux + uy * uy + uz * uz + vx * vx + vy * vy + vz * vz
                                   + wx * wx + wy * wy + wz * wz;
                if (sumSq <= 0.0) continue;
                const double thinness = twiceArea / sumSq;
                thinnest = std::min(thinnest, thinness);
                if (thinness < 1e-6) ++caps;
            }
        }
    }

    RecordProperty("cutsChecked", cuts);
    RecordProperty("cutFaces", faces);
    RecordProperty("capsEmitted", caps);
    ASSERT_GT(faces, 10000) << "battery degenerated";
    EXPECT_EQ(caps, 0) << caps << " cap sub-triangles survived the retriangulation filter";
    EXPECT_EQ(brokenCuts, 0) << brokenCuts << " of " << cuts
                             << " operand cuts were not watertight at 32x36";
}

// Dropping caps must not delete real area. A cap encloses none, so union and intersection
// volumes still have to sum to the sum of the operand volumes.
TEST(MeshBooleanSeam, DroppingCapsPreservesVolume)
{
    using primitives::makeBox;
    double worst = 0.0;
    int checked = 0;
    std::mt19937 rng(4242u);
    std::uniform_real_distribution<float> t(-1.6f, 1.6f);
    const Mesh sphere = primitives::makeSphere(1.2f, 32, 36);
    for (int i = 0; i < 10; ++i) {
        const Mesh a = makeBox(2.f, 2.f, 2.f);
        const Mesh b = translated(sphere, {t(rng), t(rng), t(rng)});
        const Mesh u = robustMeshBoolean(a, b, BooleanOperationType::Union);
        const Mesh n = robustMeshBoolean(a, b, BooleanOperationType::Intersection);
        if (u.topology().faceCount() == 0 || n.topology().faceCount() == 0) continue;
        const double va = MeshMassProperties::compute(a).volume;
        const double vb = MeshMassProperties::compute(b).volume;
        const double vu = MeshMassProperties::compute(u).volume;
        const double vn = MeshMassProperties::compute(n).volume;
        worst = std::max(worst, std::abs((vu + vn) - (va + vb)) / std::abs(va + vb));
        ++checked;
    }
    RecordProperty("volumeWorstRelError", static_cast<int>(worst * 1e9));
    ASSERT_GT(checked, 5) << "battery degenerated";
    // |A| + |B| == |A U B| + |A n B|. Measured worst 3.7e-6 — float precision, not lost area.
    EXPECT_LT(worst, 1e-4) << "union+intersection volume drifted by " << worst
                           << " — the cap filter may be deleting real geometry";
}

// An all-planar operand of comparable face count does NOT leak, so class 2 is not simply
// "more triangles" — it tracks the size of the features the seam has to resolve.
TEST(MeshBooleanSeam, AllPlanarOperandsOfSimilarFaceCountDoNotLeak)
{
    using primitives::makeBox;
    int runs = 0, leaks = 0;
    std::mt19937 rng(77u);
    std::uniform_real_distribution<float> t(-1.4f, 1.4f);
    for (uint32_t sides : {4u, 8u, 12u}) {
        for (int i = 0; i < 15; ++i) {
            const Mesh a = makeBox(2.f, 2.f, 2.f);
            const Mesh b = translated(primitives::makeCylinder(0.9f, 2.4f, sides),
                                      {t(rng), t(rng), t(rng)});
            for (auto op : kOps) {
                ++runs;
                if (leakState(robustMeshBoolean(a, b, op)) == 1) ++leaks;
            }
        }
    }
    RecordProperty("planarCylinderRuns", runs);
    RecordProperty("planarCylinderLeaks", leaks);
    EXPECT_EQ(leaks, 0) << "all-planar cylinder/box booleans leaked (" << leaks << " of " << runs
                        << ") — class 2 would then be about face count, not feature size";
}

// The surviving claim from the old map, re-asserted rather than inherited: MeshCut only
// RETESSELLATES. Each operand's cut stays closed and keeps its volume, so every leak above
// originates downstream of the cut — in classify/keep/weld.
TEST(MeshBooleanSeam, EachOperandCutIsIndividuallyWatertight)
{
    using primitives::makeBox;
    int checked = 0, broken = 0;
    auto checkOne = [&](const Mesh& before, const Mesh& after) {
        ++checked;
        const float v0 = MeshMassProperties::compute(before).volume;
        const float v1 = MeshMassProperties::compute(after).volume;
        const bool closed = MeshTopologyValidation::validate(after).boundaryLoops == 0;
        const bool volumeKept = std::abs(v0) > 1e-6f
                                    ? std::abs(v1 - v0) / std::abs(v0) < 1e-3f
                                    : std::abs(v1 - v0) < 1e-6f;
        if (!closed || !volumeKept) ++broken;
    };
    auto check = [&](const Mesh& a, const Mesh& b) {
        const MeshCutResult cr = MeshCut::cut(a, b);
        checkOne(a, cr.a);
        checkOne(b, cr.b);
    };

    for (double rel : {1e-5, 1e-4}) {
        check(makeBox(2.f, 2.f, 2.f),
              translated(makeBox(2.f, 2.f, 2.f), {static_cast<float>(rel * 2.0), 0.f, 0.f}));
    }
    std::mt19937 rng(31337u);
    std::uniform_real_distribution<float> tr(-2.f, 2.f), an(-3.14159f, 3.14159f);
    for (int i = 0; i < 40; ++i) {
        Mesh b = rotZ(makeBox(2.f, 2.f, 2.f), an(rng));
        check(makeBox(2.f, 2.f, 2.f), translated(std::move(b), {tr(rng), tr(rng), tr(rng)}));
    }

    RecordProperty("cutsChecked", checked);
    RecordProperty("cutsBroken", broken);
    EXPECT_EQ(broken, 0) << broken << " of " << checked
                         << " operand cuts were not watertight — the defect moved INTO MeshCut, "
                            "which changes where the fix belongs";
}

}  // namespace nexus::geometry::testing
