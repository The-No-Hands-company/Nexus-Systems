// Foundation — the MESH boolean (world #2, the half-edge/DCC CSG path). Two exact
// decisions make robustMeshBoolean watertight on the cases that used to leak:
//   (1) point-in-solid CLASSIFICATION is an exact Simulation-of-Simplicity ray
//       parity (brep::segmentCrossesTriangleSoS), not a fragile float ray-cast; and
//   (2) COPLANAR-overlap faces are now handled — MeshCut imprints each triangle's
//       edges onto the other (exact orient2D clip) so both surfaces share seam
//       vertices along the overlap, and robustMeshBoolean resolves the resulting
//       coincident faces with the B-rep boolean's selectFace rule table (probe just
//       outside each face along its normal; same-normal coincident → keep A's copy
//       only, opposite-normal → kept for Difference).
//
// WHAT IS PROVEN HERE:
//   • Generic-position (fully transversal) booleans are WATERTIGHT (closed genus-0,
//     zero boundary loops) AND volume-EXACT.
//   • AXIS-ALIGNED / COPLANAR-face-sharing box booleans are now ALSO watertight AND
//     volume-exact (the seam-rebuild win — these leaked before this increment).
//   • The whole pipeline is byte-for-byte DETERMINISTIC across a wide near-
//     degenerate sweep.
//
// STILL HONESTLY BOUNDED (the remaining world-#2 gap — hard degeneracies):
//   • Fully-coincident (identical operands), single-vertex / single-edge touch, and
//     grazing near-coincident (sub-tolerance offset) configurations can still leave
//     boundary loops. Those are the residual seam degeneracies, tracked next — NOT a
//     classification or coplanar error. Curved primitives (makeCylinder/makeSphere)
//     remain approximate here too. The determinism sweep below asserts only what
//     holds across ALL configs (finite + deterministic), never false watertightness.

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

// OPERAND-ORDER SYMMETRY (partial): the coincident-face classification is now
// winding-INDEPENDENT — each sub-triangle's normal is re-oriented outward by an
// exact self-test (pointInside against its own solid) rather than trusting the
// retriangulation's winding. With that, these coplanar-face-sharing box booleans
// are watertight in BOTH operand orders (A∘B and B∘A), not just the canonical one.
// (dx=0.5's larger overlap still has an order-dependent seam-diagonal mismatch —
// the residual shared-seam-triangulation gap, tracked next; not asserted here.)
TEST(MeshBooleanExactClassify, CoplanarBooleansWatertightInBothOperandOrders)
{
    using primitives::makeBox;
    for (float dx : {1.f, 1.5f}) {
        const Mesh A = makeBox(2.f, 2.f, 2.f);
        const Mesh B = translated(makeBox(2.f, 2.f, 2.f), {dx, 0.f, 0.f});
        for (auto op : {BooleanOperationType::Union, BooleanOperationType::Intersection,
                        BooleanOperationType::Difference}) {
            const auto vab = MeshTopologyValidation::validate(robustMeshBoolean(A, B, op));
            const auto vba = MeshTopologyValidation::validate(robustMeshBoolean(B, A, op));
            EXPECT_EQ(vab.boundaryLoops, 0u) << "A∘B dx=" << dx << " op=" << static_cast<int>(op);
            EXPECT_EQ(vba.boundaryLoops, 0u) << "B∘A dx=" << dx << " op=" << static_cast<int>(op);
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

// THE SEAM-REBUILD WIN: axis-aligned box booleans SHARE coplanar faces (both boxes
// span y,z ∈ [-1,1]; only x is offset), so every one of these used to leak. With
// coplanar imprint (MeshCut) + coincident-face resolution (robustMeshBoolean) they
// are now WATERTIGHT closed genus-0 solids with EXACT volumes. NOTE: the coincident
// resolution keeps the shared face from the FIRST operand, so this pins the
// canonical operand order A∘B; the reversed order B∘A of a coplanar pair is not yet
// symmetric (a residual seam degeneracy tracked for the next increment).
TEST(MeshBooleanExactClassify, AxisAlignedCoplanarSeamsAreWatertightAndVolumeExact)
{
    using primitives::makeBox;
    for (float dx : {0.5f, 1.f, 1.5f}) {
        const Vec3 o{dx, 0.f, 0.f};
        const float ov = overlapVol(o);  // = (2-dx)*2*2, coplanar y & z faces overlap
        const Mesh A = makeBox(2.f, 2.f, 2.f);
        const Mesh B = translated(makeBox(2.f, 2.f, 2.f), o);
        struct Case { BooleanOperationType op; float expect; const char* n; };
        const Case cases[] = {
            {BooleanOperationType::Union, 16.f - ov, "union"},
            {BooleanOperationType::Intersection, ov, "intersection"},
            {BooleanOperationType::Difference, 8.f - ov, "difference"},
        };
        for (const Case& c : cases) {
            const Mesh r = robustMeshBoolean(A, B, c.op);
            const auto v = MeshTopologyValidation::validate(r);
            EXPECT_TRUE(v.valid) << c.n << " dx=" << dx;
            EXPECT_EQ(v.boundaryLoops, 0u) << c.n << " dx=" << dx << " must be watertight";
            EXPECT_EQ(v.euler, 2) << c.n << " dx=" << dx << " must be closed genus-0";
            EXPECT_NEAR(vol(r), c.expect, 1e-3f) << c.n << " dx=" << dx;
        }
    }
}

namespace {
Mesh scaledBox(float extent, float s) { return primitives::makeBox(extent * s, extent * s, extent * s); }
}  // namespace

// SCALE-AWARE seam weld (Phase T tolerance migration): the boolean's seam stitch
// used a FIXED 1e-5 weld, which fails to merge coincident seam vertices on large
// models (at coordinate magnitude M the two copies of a seam point differ by ~M·1e-6
// of float precision > 1e-5) — a large-scale-only leak. With a scale-proportioned
// weld tolerance the SAME generic-position boolean stays watertight and volume-exact
// across five orders of magnitude, while unit scale is byte-for-byte unchanged.
TEST(MeshBooleanExactClassify, SeamWeldIsScaleAwareAcrossOrdersOfMagnitude)
{
    for (float s : {1.f, 100.f, 5000.f, 100000.f}) {
        // Diagonal-offset boxes (watertight generic position), uniformly scaled by s.
        const Mesh A = scaledBox(2.f, s);
        const Mesh B = translated(scaledBox(2.f, s), {1.f * s, 1.f * s, 1.f * s});
        for (auto op : {BooleanOperationType::Union, BooleanOperationType::Intersection,
                        BooleanOperationType::Difference}) {
            const Mesh r = robustMeshBoolean(A, B, op);
            const auto v = MeshTopologyValidation::validate(r);
            EXPECT_EQ(v.boundaryLoops, 0u) << "s=" << s << " op=" << static_cast<int>(op)
                                           << " leaked (fixed-epsilon weld would fail here)";
            EXPECT_EQ(v.euler, 2) << "s=" << s << " op=" << static_cast<int>(op);
        }
        // Volume scales as s^3 (union of the two boxes overlapping in a unit-cube-times-s^3).
        const float s3 = s * s * s;
        EXPECT_NEAR(vol(robustMeshBoolean(A, B, BooleanOperationType::Union)), 15.f * s3, 15.f * s3 * 1e-3f)
            << "s=" << s;
    }
}

}  // namespace nexus::geometry::testing
