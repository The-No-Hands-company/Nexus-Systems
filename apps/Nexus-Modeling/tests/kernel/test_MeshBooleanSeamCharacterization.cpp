// Foundation — PHASE M (mesh-boolean shared-seam rebuild) begins with an honest map.
// The mesh boolean's exact classification and coplanar imprint are done, but ~29
// near-degenerate configs still leak. Earlier increments GUESSED the mechanism was
// T-junctions (a vertex landing on the interior of another triangle's edge). A
// boundary-edge characterization DISPROVES that: across the whole torture battery,
// ZERO leaks are T-junctions. The residual is instead:
//   • HOLES — genuine missing faces at the seam, where MeshCut retriangulates the two
//     operands INDEPENDENTLY so their seam neighbourhoods don't tile consistently
//     (the dominant class); a flat cap would be geometrically WRONG, so the real fix
//     is generating the missing seam faces correctly (a shared-seam construction).
//   • NON-MANIFOLD — an edge shared by ≥3 triangles, from doubled coincident faces
//     (the coplanar operand-order-dependent cases).
// This test PINS that corrected diagnosis (0 T-junctions) and the residual leak count
// as a baseline, so the seam-rebuild increments can measure progress and no change can
// silently reintroduce a T-junction. It does NOT assert full watertightness (the
// residual is the open arc) — only what is true and measured today.

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBooleanRobust.h>
#include <nexus/geometry/MeshTopologyValidation.h>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <utility>
#include <vector>

namespace nexus::geometry::testing {

using nexus::render::Vec3;

namespace {
Mesh translated(Mesh m, Vec3 o)
{
    auto p = m.attributes().positions();
    for (auto& v : p) { v.x += o.x; v.y += o.y; v.z += o.z; }
    m.attributes().setPositions(std::move(p));
    return m;
}
Mesh rotZ(Mesh m, float a)
{
    const float c = std::cos(a), s = std::sin(a);
    auto p = m.attributes().positions();
    for (auto& v : p) { const float x = v.x, y = v.y; v.x = c * x - s * y; v.y = s * x + c * y; }
    m.attributes().setPositions(std::move(p));
    return m;
}
float dist2(const Vec3& a, const Vec3& b)
{
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}
// Is V strictly on the interior of segment A-B (the T-junction signature)?
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

struct LeakKind { bool leaks = false; bool nonManifold = false; bool tJunction = false; };

LeakKind classify(const Mesh& r)
{
    LeakKind k;
    if (r.topology().faceCount() == 0) return k;
    const auto v = MeshTopologyValidation::validate(r);
    if (v.boundaryLoops == 0) return k;  // closed (incl. valid vertex-touching) → not a leak
    k.leaks = true;

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
    for (const auto& [edge, count] : edgeUse) {
        if (count >= 3) { k.nonManifold = true; }
        if (count == 1) {
            for (uint32_t vi = 0; vi < pos.size(); ++vi) {
                if (vi == edge.first || vi == edge.second) continue;
                if (onSegmentInterior(pos[vi], pos[edge.first], pos[edge.second])) { k.tJunction = true; break; }
            }
        }
    }
    return k;
}
}  // namespace

// PINNED baseline + corrected diagnosis for the Phase-M seam rebuild.
TEST(MeshBooleanSeam, ResidualLeaksAreHolesOrNonManifoldNeverTJunctions)
{
    using primitives::makeBox;
    int leaks = 0, tJunctions = 0, nonManifold = 0;
    auto run = [&](const Mesh& a, const Mesh& b, BooleanOperationType op) {
        const LeakKind k = classify(robustMeshBoolean(a, b, op));
        if (k.leaks) ++leaks;
        if (k.tJunction) ++tJunctions;
        if (k.nonManifold) ++nonManifold;
    };
    const std::vector<float> dxs = {0.f, 1e-4f, 0.3f, 0.5f, 1.f, 1.5f, 1.9999f, 2.f, 2.0001f, 3.f};
    const std::vector<float> angs = {0.f, 0.7853981634f, 0.5f, 1.0471975512f};
    for (float dx : dxs) {
        for (float ang : angs) {
            const Mesh a = makeBox(2.f, 2.f, 2.f);
            const Mesh b = translated(rotZ(makeBox(2.f, 2.f, 2.f), ang), {dx, 0.f, 0.f});
            for (auto op : {BooleanOperationType::Union, BooleanOperationType::Intersection,
                            BooleanOperationType::Difference})
                run(a, b, op);
        }
    }
    for (Vec3 o : {Vec3{1e-4f, 1e-4f, 1e-4f}, Vec3{1.f, 1.f, 1.f}, Vec3{2.f, 2.f, 2.f}, Vec3{0.5f, 0.5f, 0.5f}}) {
        const Mesh a = makeBox(2.f, 2.f, 2.f);
        const Mesh b = translated(makeBox(2.f, 2.f, 2.f), o);
        for (auto op : {BooleanOperationType::Union, BooleanOperationType::Intersection,
                        BooleanOperationType::Difference})
            run(a, b, op);
    }

    // The corrected finding: NO leak is a T-junction. This guards the mechanism — a
    // future MeshCut change that introduced a T-junction would fail here.
    EXPECT_EQ(tJunctions, 0) << "a leak is now a T-junction — the seam mechanism changed";
    EXPECT_GT(nonManifold, 0) << "expected some doubled-face (non-manifold) leaks in the residual";
    // Pinned residual baseline (box-box + diagonal subset; box/curved excluded for speed).
    // The seam rebuild must only ever REDUCE this — a rise means a regression. When
    // Phase M drops it, TIGHTEN this number (the RecordProperty makes the count visible).
    RecordProperty("meshBooleanSeamLeaks", leaks);
    RecordProperty("meshBooleanSeamNonManifold", nonManifold);
    EXPECT_LE(leaks, 24) << "mesh-boolean seam leaks ROSE above the pinned baseline (regression)";
}

}  // namespace nexus::geometry::testing
