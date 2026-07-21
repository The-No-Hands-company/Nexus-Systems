// Foundation — PHASE M (mesh-boolean shared-seam rebuild), pinning honest progress.
// Earlier increments GUESSED the residual leak mechanism was T-junctions (a vertex
// landing on the interior of another triangle's edge). A boundary-edge
// characterization DISPROVED that: ZERO leaks are T-junctions. The residual is:
//   • HOLES — missing faces at the seam, where MeshCut retriangulates the two operands
//     INDEPENDENTLY so their seam neighbourhoods don't tile consistently (dominant).
//   • NON-MANIFOLD EDGE — an edge shared by ≥3 triangles (doubled coincident faces).
//   • (formerly) a bowtie NON-MANIFOLD VERTEX in one reversed-order coplanar case —
//     now ELIMINATED, see below.
// Progress: making the per-triangle CDT (ConstrainedDelaunay2D) robustly ENFORCE its
// constraint edges — convexity-checked edge recovery plus splitting a constraint at any
// vertex lying exactly on it — removed the STRADDLING sub-triangles that most
// transverse-seam leaks came from. That HALVED the leak count (30→15 on the full
// torture battery, 26→9 on this battery) and eliminated the bowtie class entirely, all
// while keeping determinism and introducing zero inverted/overlapping triangles.
// This test PINS the corrected diagnosis (0 T-junctions, 0 bowties) and the tightened
// residual leak count, so the seam-rebuild increments keep measuring progress and no
// change can silently regress. It does NOT assert full watertightness (the residual —
// dominated by the coplanar-cap / side-wall 3-way junction — is the open arc).

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

struct LeakKind {
    bool leaks = false;
    bool hole = false;              // a boundary edge (used by exactly 1 triangle) — a genuine gap
    bool nonManifoldEdge = false;   // an edge used by ≥3 triangles
    bool nonManifoldVertex = false; // edge-manifold, but a non-manifold VERTEX (bowtie / pinch)
    bool tJunction = false;         // a vertex lying on the interior of a boundary edge (the old, wrong guess)
};

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
        if (count >= 3) { k.nonManifoldEdge = true; }
        if (count == 1) {
            k.hole = true;
            for (uint32_t vi = 0; vi < pos.size(); ++vi) {
                if (vi == edge.first || vi == edge.second) continue;
                if (onSegmentInterior(pos[vi], pos[edge.first], pos[edge.second])) { k.tJunction = true; break; }
            }
        }
    }
    // Edge-manifold (no boundary edge, no ≥3 edge) yet the validator reports a
    // boundary loop ⇒ a non-manifold VERTEX (two surface sheets pinched at a point;
    // euler stays 2 while boundaryLoops is nonzero — an internally inconsistent, i.e.
    // non-2-manifold, result). This is a distinct defect from a hole.
    if (!k.hole && !k.nonManifoldEdge) k.nonManifoldVertex = true;
    return k;
}
}  // namespace

// PINNED baseline + corrected diagnosis for the Phase-M seam rebuild.
TEST(MeshBooleanSeam, ResidualLeaksAreHolesOrNonManifoldNeverTJunctions)
{
    using primitives::makeBox;
    int leaks = 0, tJunctions = 0, holes = 0, nmEdge = 0, nmVertex = 0;
    auto run = [&](const Mesh& a, const Mesh& b, BooleanOperationType op) {
        const LeakKind k = classify(robustMeshBoolean(a, b, op));
        if (k.leaks) ++leaks;
        if (k.tJunction) ++tJunctions;
        if (k.hole) ++holes;
        if (k.nonManifoldEdge) ++nmEdge;
        if (k.nonManifoldVertex) ++nmVertex;
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
    // A separate small check for the non-manifold-VERTEX (bowtie) class, which appears
    // only in the REVERSED operand order of an axis-aligned coplanar pair (B∘A), not in
    // the canonical order the battery above uses.
    {
        const Mesh a = makeBox(2.f, 2.f, 2.f);
        const Mesh b = translated(makeBox(2.f, 2.f, 2.f), {0.5f, 0.f, 0.f});
        run(b, a, BooleanOperationType::Union);
    }

    // The corrected map: every leak is a HOLE or a non-manifold EDGE. NONE is a
    // T-junction — the guess earlier increments recorded. This EXPECT guards the
    // mechanism: a future change that introduced a T-junction would fail here.
    EXPECT_EQ(tJunctions, 0) << "a leak is now a T-junction — the seam mechanism changed";
    // Every leak is classified (classify() sets nonManifoldVertex whenever a leak is
    // neither a hole nor a non-manifold edge), so the classes cover all leaks.
    EXPECT_GE(holes + nmEdge + nmVertex, leaks) << "an unclassified leak slipped through";
    EXPECT_GT(holes, 0) << "missing-face holes are the dominant class";
    EXPECT_GT(nmEdge, 0) << "some leaks are non-manifold edges (≥3 faces on an edge)";
    // The bowtie (non-manifold VERTEX) class was ELIMINATED by the CDT constraint-
    // enforcement fix: the reversed-order coplanar case that used to pinch two sheets
    // at a point is now clean. Guard that it stays eliminated.
    EXPECT_EQ(nmVertex, 0) << "a bowtie (non-manifold vertex) leak reappeared";
    // Pinned residual baseline. The residual leaks share ONE root — MeshCut triangulates
    // the two operands INDEPENDENTLY along the coplanar-cap seam (proven: each operand's
    // cut is individually watertight; the leak is the cap/side-wall 3-way junction) — so
    // none is fixable by a post-process (dedup/re-weld were both proven ineffective); the
    // fix is a shared-seam construction. The count was HALVED (30→15 on the full torture
    // battery, 26→9 here) by making the per-triangle CDT robustly ENFORCE its constraint
    // edges (convexity-checked recovery + on-constraint-vertex splitting), which removed
    // the straddling sub-triangles that most transverse-seam leaks came from. The seam
    // rebuild must only ever REDUCE these; TIGHTEN when it does.
    RecordProperty("seamLeaks", leaks);
    RecordProperty("seamHoles", holes);
    RecordProperty("seamNonManifoldEdge", nmEdge);
    RecordProperty("seamNonManifoldVertex", nmVertex);
    EXPECT_LE(leaks, 9) << "mesh-boolean seam leaks ROSE above the pinned baseline (regression)";
}

}  // namespace nexus::geometry::testing
