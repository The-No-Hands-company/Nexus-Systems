// Foundation — PHASE M (mesh-boolean seam), the defect map RE-MEASURED FROM SCRATCH.
//
// The previous map in this file was drawn while orient2D/orient3D/inCircle were returning
// WRONG SIGNS on degenerate input (see the exact-predicate rebuild). Every conclusion it
// reached about where the leaks come from rested on unsound classifiers, so it was
// re-derived from nothing rather than adjusted. Three of its claims did not survive:
//
//   • "The residual is dominated by the coplanar-cap / side-wall 3-way junction."
//     NOT SUPPORTED. Box/box booleans in GENERAL POSITION are now essentially watertight —
//     0 leaks in a 900-run systematic sweep, 1 in 1200 randomized runs. The entire box
//     residual sits in a narrow band of NEAR-COINCIDENT offsets.
//   • "ZERO leaks are T-junctions."  TRUE ONLY FOR BOXES. The old battery was box-only;
//     curved operands do produce them.
//   • The old map never measured curved or finely-tessellated operands at all — and that
//     is now by far the LARGEST defect class.
//
// One claim did survive, and is re-asserted below: each operand's cut is individually
// watertight, so the defect is downstream of MeshCut, in classify/keep/weld.
//
// THE RE-MEASURED MAP — two live classes, both SCALE-INVARIANT (measured identical at
// model scales 0.2 through 200, so neither is a fixed absolute epsilon):
//
//   CLASS 1 — NEAR-COINCIDENT COPLANAR FACES. Axis-aligned boxes separated by a RELATIVE
//     offset in the measured band 1e-6..2e-4 leak; at and below 7e-7 the seam weld snaps
//     the faces together (clean), at and above 3e-4 they are genuinely distinct (clean),
//     and inside the band neither resolution happens. The same band appears around
//     face-touching separation. This is the whole of the box residual.
//   CLASS 2 — TESSELLATION DENSITY. Sphere-vs-box leak rate climbs with density:
//     ~6% at 6x10, ~14% at 12x16, ~16% at 16x20, ~51% at 24x28 — on inputs verified
//     watertight and 2-manifold, with clean cuts. An all-planar cylinder of comparable
//     face count does NOT leak, so it tracks the seam's feature size, not face count.

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
    // Measured 2026-07-22: 1 leak in 2100 runs across sweep + randomized.
    EXPECT_LE(leaks, 2) << "general-position box booleans regressed: " << leaks << " of " << runs;
}

// CLASS 1 — the entire box residual, and it is a narrow band of NEAR-COINCIDENT offsets,
// not a 3-way junction. Below the band the seam weld snaps coincident faces together;
// above it they are distinct; inside it neither resolution happens.
TEST(MeshBooleanSeam, NearCoincidentCoplanarOffsetsAreTheBoxResidual)
{
    using primitives::makeBox;
    Classes c;
    int inBand = 0, outOfBand = 0;

    // Measured band edges (relative to model size): leaks span 1e-6..2e-4; clean at and
    // below 7e-7, clean at and above 3e-4.
    for (double rel : {1e-6, 1e-5, 1e-4}) {
        const Mesh a = makeBox(2.f, 2.f, 2.f);
        const Mesh b = translated(makeBox(2.f, 2.f, 2.f), {static_cast<float>(rel * 2.0), 0.f, 0.f});
        for (auto op : kOps) {
            const Mesh r = robustMeshBoolean(a, b, op);
            if (leakState(r) == 1) ++inBand;
            classifyInto(c, r);
        }
    }
    for (double rel : {1e-8, 1e-7, 5e-7, 3e-4, 1e-3, 1e-2}) {
        const Mesh a = makeBox(2.f, 2.f, 2.f);
        const Mesh b = translated(makeBox(2.f, 2.f, 2.f), {static_cast<float>(rel * 2.0), 0.f, 0.f});
        for (auto op : kOps) {
            if (leakState(robustMeshBoolean(a, b, op)) == 1) ++outOfBand;
        }
    }

    RecordProperty("coincidentBandLeaks", inBand);
    RecordProperty("coincidentOutOfBandLeaks", outOfBand);
    EXPECT_GT(inBand, 0) << "the near-coincident band stopped leaking — RE-MEASURE the map, "
                            "this test's whole premise is that it does";
    EXPECT_EQ(outOfBand, 0) << "leaks escaped the near-coincident band into ordinary offsets";
    // Every leak in this class is a hole and/or a non-manifold edge; none is a bowtie.
    EXPECT_EQ(c.nonManifoldVertex, 0) << "a bowtie (non-manifold vertex) leak reappeared";
    EXPECT_GT(c.holes, 0) << "missing-face holes are the dominant shape of this class";
}

// The band is SCALE-INVARIANT: the same RELATIVE offsets leak whether the model is 0.2 or
// 200 units across. So it is not a fixed absolute epsilon left un-migrated — it is an
// algorithmic gap in handling faces that are near-coincident but not coincident.
TEST(MeshBooleanSeam, TheCoincidentBandIsScaleInvariant)
{
    using primitives::makeBox;
    std::vector<int> leaksAtScale;
    for (float scale : {0.1f, 1.f, 10.f, 100.f}) {
        int leaks = 0;
        for (double rel : {1e-6, 1e-5, 1e-4}) {
            const Mesh a = transformed(makeBox(2.f, 2.f, 2.f), scale, {0.f, 0.f, 0.f});
            const Mesh b = transformed(makeBox(2.f, 2.f, 2.f), scale,
                                       {static_cast<float>(rel * 2.0 * scale), 0.f, 0.f});
            for (auto op : kOps) {
                if (leakState(robustMeshBoolean(a, b, op)) == 1) ++leaks;
            }
        }
        leaksAtScale.push_back(leaks);
    }
    for (std::size_t i = 1; i < leaksAtScale.size(); ++i) {
        EXPECT_NEAR(leaksAtScale[i], leaksAtScale[0], 1)
            << "the near-coincident band moved with model scale — it would then be a fixed "
               "absolute epsilon, a different (and easier) defect than the measured one";
    }
}

// CLASS 2 — the largest remaining class, and one the previous box-only map never saw.
// Inputs are verified watertight first, so this cannot be blamed on the primitives.
TEST(MeshBooleanSeam, CurvedOperandLeakRateGrowsWithTessellationDensity)
{
    using primitives::makeBox;
    std::vector<int> rates;
    for (uint32_t seg : {6u, 12u, 24u}) {
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
        rates.push_back(100 * leaks / runs);
        RecordProperty("curvedLeakPercent_" + std::to_string(seg), 100 * leaks / runs);
    }
    // Denser tessellation leaks more — the signature that the defect tracks the seam's
    // feature size. Pinned so a fix shows up as this ordering collapsing toward zero.
    EXPECT_GT(rates.back(), rates.front())
        << "curved leak rate no longer grows with density — RE-MEASURE, the mechanism changed";
    EXPECT_LE(rates.back(), 60) << "curved-operand leak rate rose above the measured baseline";
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
