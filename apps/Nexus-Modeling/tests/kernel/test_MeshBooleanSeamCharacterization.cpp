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
// extreme density. Class 1 (near-coincident coplanar faces) is closed except for a narrow
// band where the face separation is ABOUT EQUAL to the coincidence tolerance — the
// inherent ambiguity band of any tolerance-based coincidence test, pinned below.

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

// CLASS 1, after the fix. Near-coincident coplanar faces are now resolved correctly
// except in a narrow band where the separation is ABOUT EQUAL to the coincidence
// tolerance. That band is inherent to deciding coincidence by measurement: at exactly the
// tolerance the answer is genuinely ambiguous, and neighbouring faces can fall on opposite
// sides of it, so keep/drop becomes inconsistent and the seam opens. Real kernels avoid it
// by snapping geometry to tolerance BEFORE the boolean, which is a separate feature.
//
// Pinned so that the band cannot silently widen, and so that separations comfortably below
// and above the tolerance stay clean.
TEST(MeshBooleanSeam, NearCoincidentFacesLeakOnlyNearTheCoincidenceTolerance)
{
    using primitives::makeBox;
    int wellBelow = 0, nearTolerance = 0, wellAbove = 0;

    auto count = [&](double rel, int& into) {
        const Mesh a = makeBox(2.f, 2.f, 2.f);
        const Mesh b = translated(makeBox(2.f, 2.f, 2.f), {static_cast<float>(rel * 2.0), 0.f, 0.f});
        for (auto op : kOps) {
            if (leakState(robustMeshBoolean(a, b, op)) == 1) ++into;
        }
    };

    // The coincidence tolerance at this model size is 1e-5 (Tolerance::at, floor 1e-5).
    for (double rel : {1e-9, 1e-8, 1e-7, 5e-7}) count(rel, wellBelow);   // separation << tol
    for (double rel : {1e-6, 1e-5}) count(rel, nearTolerance);           // separation ~ tol
    for (double rel : {1e-4, 3e-4, 1e-3, 1e-2, 1e-1}) count(rel, wellAbove);  // separation >> tol

    RecordProperty("coincidentWellBelowLeaks", wellBelow);
    RecordProperty("coincidentNearToleranceLeaks", nearTolerance);
    RecordProperty("coincidentWellAboveLeaks", wellAbove);

    EXPECT_EQ(wellBelow, 0) << "faces far closer than the tolerance are no longer merged cleanly";
    EXPECT_EQ(wellAbove, 0) << "faces far beyond the tolerance are no longer treated as distinct";
    // Measured 2026-07-22: 5 of 6 ops leak in the ambiguity band (was the full
    // 1e-6..2e-4 range before the classification fix).
    EXPECT_LE(nearTolerance, 6) << "the coincidence ambiguity band widened";
}

// CLASS 2, after the fix. This was the largest class — sphere-vs-box leak rates of 12%,
// 22%, 48%, 67% climbing with tessellation density. Through moderate density it is now
// zero. A residual remains only where the cut produces sub-triangles at the very limit of
// float resolution (below ~1e-6 on a 2-unit model), which is a cut-quality question rather
// than a classification one, and is pinned rather than claimed fixed.
TEST(MeshBooleanSeam, CurvedOperandBooleansAreWatertightThroughModerateTessellation)
{
    using primitives::makeBox;
    for (uint32_t seg : {6u, 12u, 16u}) {
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

// The extreme-density residual, tracked not asserted at zero. The cut emits sub-triangles
// under 1e-6 across on a 2-unit model — a few float ULPs wide — where no classification
// can be reliable because the geometry itself has run out of precision. Closing this means
// not producing such slivers, which is a cut-quality increment.
TEST(MeshBooleanSeam, ExtremeTessellationResidualIsTracked)
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
    // Measured 2026-07-22: 3 of 60, down from 29 of 60 before the classification fix.
    EXPECT_LE(leaks, 5) << "the extreme-tessellation residual rose above its baseline: "
                        << leaks << " of " << runs;
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

// WHAT THE EXTREME-DENSITY RESIDUAL ACTUALLY IS — and what it is not.
//
// It was previously recorded as T-junctions (a seam edge split on one operand and not the
// other) at "34% of boundary edges". That measurement was WRONG: it searched every vertex
// for one lying on a boundary edge's interior without excluding the edge's OWN opposite
// corner, so it counted a degenerate triangle's own apex as a foreign vertex.
//
// Measured correctly, every residual boundary edge (20 of 20) is owned by a CAP: a triangle whose
// third vertex lies on its opposite edge, ~3e-8 off a ~0.12-long edge, at a parameter well
// inside the span (0.77..0.96). The cut's retriangulation emits it because, with exact
// predicates, those three points genuinely are not collinear — they are only collinear to
// within float resolution — so a valid but degenerate triangle is the correct answer to
// the question as posed. Its long edge has no partner, because the region on the other
// side is tiled through the apex instead, and that unpartnered edge is the leak.
//
// This test pins the mechanism so the next attempt starts from the right place: the fix
// belongs in the retriangulation (snapping a near-collinear seam point onto the edge so it
// SPLITS the edge instead of forming a cap), not in a conforming pass over the cut — a
// conforming pass was written against the T-junction diagnosis and correctly found nothing
// to do.
TEST(MeshBooleanSeam, ExtremeDensityResidualIsCapTrianglesNotTJunctions)
{
    using primitives::makeBox;
    int boundaryEdges = 0, ownApexOnEdge = 0, foreignVertexOnEdge = 0;
    std::mt19937 rng(4242u);
    std::uniform_real_distribution<float> t(-1.6f, 1.6f);
    const Mesh sphere = primitives::makeSphere(1.2f, 32, 36);

    for (int i = 0; i < 20; ++i) {
        const Mesh a = makeBox(2.f, 2.f, 2.f);
        const Mesh b = translated(sphere, {t(rng), t(rng), t(rng)});
        const MeshCutResult cr = MeshCut::cut(a, b);
        for (const Mesh* m : {&cr.a, &cr.b}) {
            const auto& pos = m->attributes().positions();
            std::map<std::pair<uint32_t, uint32_t>, int> use;
            for (size_t f = 0; f < m->topology().faceCount(); ++f) {
                const auto& fc = m->topology().face(f);
                if (fc.indices.size() != 3) continue;
                for (int e = 0; e < 3; ++e) {
                    uint32_t x = fc.indices[e], y = fc.indices[(e + 1) % 3];
                    if (x > y) std::swap(x, y);
                    use[{x, y}]++;
                }
            }
            for (size_t f = 0; f < m->topology().faceCount(); ++f) {
                const auto& fc = m->topology().face(f);
                if (fc.indices.size() != 3) continue;
                for (int e = 0; e < 3; ++e) {
                    const uint32_t ia = fc.indices[e], ib = fc.indices[(e + 1) % 3];
                    const uint32_t ic = fc.indices[(e + 2) % 3];
                    const uint32_t lo = std::min(ia, ib), hi = std::max(ia, ib);
                    const auto it = use.find({lo, hi});
                    if (it == use.end() || it->second != 1) continue;
                    ++boundaryEdges;
                    // Is this face's own apex on the edge (a cap), or some other vertex
                    // (a genuine T-junction)?
                    if (onSegmentInterior(pos[ic], pos[ia], pos[ib])) ++ownApexOnEdge;
                    for (uint32_t v = 0; v < pos.size(); ++v) {
                        if (v == ia || v == ib || v == ic) continue;
                        if (onSegmentInterior(pos[v], pos[ia], pos[ib])) {
                            ++foreignVertexOnEdge;
                            break;
                        }
                    }
                }
            }
        }
    }

    RecordProperty("cutBoundaryEdges", boundaryEdges);
    RecordProperty("cutCapApexOnEdge", ownApexOnEdge);
    RecordProperty("cutForeignVertexOnEdge", foreignVertexOnEdge);
    ASSERT_GT(boundaryEdges, 0) << "the cut stopped leaking at 32x36 — RE-MEASURE, this "
                                   "test's whole premise is that it still does";
    EXPECT_EQ(ownApexOnEdge, boundaryEdges)
        << "not every residual boundary edge is a cap any more (" << ownApexOnEdge << " of "
        << boundaryEdges << ") — the mechanism changed";
    // A couple of these edges ALSO have some unrelated vertex near them (measured 2 of
    // 20) — near-degenerate neighbourhoods are crowded. That is a co-occurrence, not the
    // mechanism: every one of them is a cap first.
    EXPECT_LE(foreignVertexOnEdge, 4)
        << foreignVertexOnEdge << " of " << boundaryEdges << " residual boundary edges now "
        << "carry a foreign vertex — genuine T-junctions may have become the mechanism, "
        << "so re-derive before fixing";
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
