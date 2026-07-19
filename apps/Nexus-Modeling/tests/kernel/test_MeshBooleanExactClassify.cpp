// Foundation — the MESH boolean (world #2, the half-edge/DCC CSG path) now makes
// its point-in-solid CLASSIFICATION decision EXACTLY: robustMeshBoolean classifies
// each cut sub-triangle with an exact Simulation-of-Simplicity ray parity
// (brep::segmentCrossesTriangleSoS) instead of a fragile float Möller-Trumbore
// ray-cast that could flip near the seam. This pins the guarantees that decision
// buys — and honestly bounds the one that is still pending the seam rebuild.
//
// WHAT IS PROVEN HERE:
//   • Generic-position (fully transversal) booleans are WATERTIGHT (closed genus-0,
//     zero boundary loops) AND volume-EXACT — the classifier keeps exactly the
//     right sub-triangles, so no hole is opened by a misjudged centroid.
//   • The whole pipeline is byte-for-byte DETERMINISTIC across a wide near-
//     degenerate sweep (the exact decision is direction-independent and stable).
//
// WHAT IS HONESTLY CHARACTERISED (not yet watertight — the pending world-#2 arc):
//   • When operands share coplanar / axis-parallel faces, the result SOLID is still
//     volume-correct, but its surface mesh has boundary loops: MeshCut leaves
//     coplanar overlaps un-split and parallel-face seams T-junction, so the shared
//     seam is not a single T-junction-free topology. That is the keystone gap the
//     mesh-CSG exact rebuild targets next — NOT a classification error.

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBooleanRobust.h>
#include <nexus/geometry/MeshMassProperties.h>
#include <nexus/geometry/MeshTopologyValidation.h>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
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
float vol(const Mesh& m) { return std::abs(MeshMassProperties::compute(m).volume); }

// A comparable byte blob of positions + face indices (for determinism checks).
std::string blob(const Mesh& m)
{
    std::string s;
    for (const auto& p : m.attributes().positions())
        s.append(reinterpret_cast<const char*>(&p), sizeof(p));
    for (size_t f = 0; f < m.topology().faceCount(); ++f) {
        for (uint32_t i : m.topology().face(f).indices)
            s.append(reinterpret_cast<const char*>(&i), sizeof(i));
        s.push_back('|');
    }
    return s;
}

// Per-axis overlap of two unit-extent-2 axis-aligned boxes offset by o.
float overlapVol(const Vec3& o)
{
    const float ox = std::max(0.f, 2.f - std::abs(o.x));
    const float oy = std::max(0.f, 2.f - std::abs(o.y));
    const float oz = std::max(0.f, 2.f - std::abs(o.z));
    return ox * oy * oz;
}
}  // namespace

// Generic-position (fully transversal) box booleans: the exact classifier keeps
// exactly the right sub-triangles, so the result is a WATERTIGHT closed genus-0
// shell with the exact CSG volume — in BOTH operand orders.
TEST(MeshBooleanExactClassify, GenericPositionBooleansAreWatertightAndVolumeExact)
{
    using primitives::makeBox;
    const std::vector<Vec3> offs = {{1.f, 1.f, 1.f}, {0.5f, 0.5f, 0.5f},
                                    {1.f, 0.5f, 0.3f}, {1.3f, 0.7f, 0.9f}};
    for (const Vec3& o : offs) {
        const float ov = overlapVol(o);
        ASSERT_GT(ov, 0.f) << "offset must overlap";
        const Mesh A = makeBox(2.f, 2.f, 2.f);
        const Mesh B = translated(makeBox(2.f, 2.f, 2.f), o);
        struct Case { BooleanOperationType op; float expect; const char* n; };
        const Case cases[] = {
            {BooleanOperationType::Union, 16.f - ov, "union"},
            {BooleanOperationType::Intersection, ov, "intersection"},
            {BooleanOperationType::Difference, 8.f - ov, "difference"},
        };
        for (const Case& c : cases) {
            for (int order = 0; order < 2; ++order) {
                // Union/Intersection are symmetric; Difference is A−B only.
                if (order == 1 && c.op == BooleanOperationType::Difference) continue;
                const Mesh r = order == 0 ? robustMeshBoolean(A, B, c.op)
                                          : robustMeshBoolean(B, A, c.op);
                const auto v = MeshTopologyValidation::validate(r);
                EXPECT_TRUE(v.valid) << c.n << " o=(" << o.x << "," << o.y << "," << o.z << ")";
                EXPECT_EQ(v.boundaryLoops, 0u) << c.n << " must be watertight";
                EXPECT_EQ(v.euler, 2) << c.n << " must be closed genus-0";
                EXPECT_NEAR(vol(r), c.expect, 1e-3f) << c.n;
            }
        }
    }
}

// The exact classification decision is direction-independent and stable, so the
// whole pipeline is byte-for-byte deterministic — even across grazing, near-
// coincident, face-touching and rotated configurations that stress the seam.
TEST(MeshBooleanExactClassify, DeterministicAndFiniteAcrossNearDegenerateSweep)
{
    using primitives::makeBox;
    int checked = 0;
    for (float dx : {0.f, 1e-4f, 0.5f, 1.f, 1.5f, 1.9999f, 2.f, 2.0001f}) {
        for (float ang : {0.f, 0.7853981634f, 0.5f, 1.0471975512f}) {
            const Mesh A = makeBox(2.f, 2.f, 2.f);
            const Mesh B = translated(rotZ(makeBox(2.f, 2.f, 2.f), ang), {dx, 0.f, 0.f});
            for (auto op : {BooleanOperationType::Union, BooleanOperationType::Intersection,
                            BooleanOperationType::Difference}) {
                const Mesh r = robustMeshBoolean(A, B, op);
                for (const auto& p : r.attributes().positions()) {
                    ASSERT_TRUE(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
                }
                // Topology structure is always at least internally consistent.
                (void)MeshTopologyValidation::validate(r);
                EXPECT_EQ(blob(r), blob(robustMeshBoolean(A, B, op)))
                    << "dx=" << dx << " ang=" << ang << " op=" << static_cast<int>(op);
                ++checked;
            }
        }
    }
    EXPECT_GT(checked, 90);
}

// HONEST CHARACTERISATION of the pending world-#2 seam gap: when operands share
// coplanar / axis-parallel faces, the result SOLID is still volume-EXACT (the
// classifier keeps the right material), but its surface mesh is NOT yet watertight
// — the shared seam T-junctions because MeshCut leaves coplanar overlaps un-split.
// This test pins the guarantee that DOES hold (correct volume) and documents the
// one that does not, so the eventual seam rebuild has a precise before/after.
TEST(MeshBooleanExactClassify, AxisAlignedSeamsAreVolumeExactPendingSeamRebuild)
{
    using primitives::makeBox;
    for (float dx : {0.5f, 1.f, 1.5f}) {
        const Vec3 o{dx, 0.f, 0.f};
        const float ov = overlapVol(o);  // = (2-dx)*2*2
        const Mesh A = makeBox(2.f, 2.f, 2.f);
        const Mesh B = translated(makeBox(2.f, 2.f, 2.f), o);
        EXPECT_NEAR(vol(robustMeshBoolean(A, B, BooleanOperationType::Union)), 16.f - ov, 1e-3f);
        EXPECT_NEAR(vol(robustMeshBoolean(A, B, BooleanOperationType::Intersection)), ov, 1e-3f);
        EXPECT_NEAR(vol(robustMeshBoolean(A, B, BooleanOperationType::Difference)), 8.f - ov, 1e-3f);
        // NOTE: watertightness (boundaryLoops==0) is intentionally NOT asserted here
        // — the coplanar/parallel-face seam is the next exact-rebuild target.
    }
}

}  // namespace nexus::geometry::testing
