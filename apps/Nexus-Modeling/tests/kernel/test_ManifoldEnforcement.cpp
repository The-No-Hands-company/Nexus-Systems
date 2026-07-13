// Foundation sweep — manifold / degenerate enforcement. Locks in the invariant
// that the ops which MUST produce manifold, watertight output actually do, using
// HalfEdgeMesh::fromMesh (2-manifold constructibility) + MeshTopologyValidation
// (Euler characteristic) as the oracle, and that degenerate faces are rejected
// at the half-edge boundary rather than silently corrupting the structure.

#include <nexus/geometry/BooleanOperation.h>
#include <nexus/geometry/ExtrudeOperation.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/InsetFacesOperation.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshThicken.h>
#include <nexus/geometry/MeshTopologyValidation.h>

#include <gtest/gtest.h>

#include <utility>

namespace nexus::geometry::testing {

using namespace nexus::geometry::primitives;

namespace {
bool isManifold(const Mesh& m) { return HalfEdgeMesh::fromMesh(m).has_value(); }
int euler(const Mesh& m) { return MeshTopologyValidation::validate(m).euler; }

// Two axis-non-coplanar overlapping boxes (A=[-1,1]^3, B=[0,2]^3).
std::pair<Mesh, Mesh> overlappingBoxes()
{
    Mesh a = makeBox(2.f, 2.f, 2.f);
    Mesh b = makeBox(2.f, 2.f, 2.f);
    auto p = b.attributes().positions();
    for (auto& v : p) { v.x += 1.f; v.y += 1.f; v.z += 1.f; }
    b.attributes().setPositions(std::move(p));
    return {a, b};
}
}  // namespace

// Solidifying an OPEN surface must yield a closed, watertight, 2-manifold shell.
TEST(ManifoldEnforcement, SolidifyOpenPlaneIsWatertightManifold)
{
    Mesh plane = makePlane(1.f, 1.f, 1, 1);
    ThickenOptions opts;
    opts.thickness = 0.1f;
    const Mesh shell = MeshThicken::solidify(plane, opts);
    EXPECT_TRUE(shell.isValid());
    EXPECT_TRUE(isManifold(shell));
    EXPECT_EQ(euler(shell), 2);  // closed genus-0 — the boundary was capped/walled shut
}

// Solidifying a CLOSED box yields a manifold double-walled shell (2 components).
TEST(ManifoldEnforcement, SolidifyClosedBoxIsManifold)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    ThickenOptions opts;
    opts.thickness = 0.1f;
    const Mesh shell = MeshThicken::solidify(box, opts);
    EXPECT_TRUE(shell.isValid());
    EXPECT_TRUE(isManifold(shell));
    EXPECT_EQ(euler(shell), 4);  // two nested closed shells (2 + 2)
}

// Every boolean result on solids is watertight + 2-manifold + genus-0.
TEST(ManifoldEnforcement, BooleanResultsAreWatertightManifold)
{
    for (auto op : {BooleanOperationType::Union, BooleanOperationType::Difference,
                    BooleanOperationType::Intersection}) {
        auto [a, b] = overlappingBoxes();
        Mesh out;
        const auto r = BooleanOperation::compute(a, b, op, {}, out);
        ASSERT_TRUE(r.isSuccess()) << "op=" << static_cast<int>(op);
        EXPECT_TRUE(out.isValid()) << "op=" << static_cast<int>(op);
        EXPECT_TRUE(isManifold(out)) << "op=" << static_cast<int>(op);
        EXPECT_EQ(euler(out), 2) << "op=" << static_cast<int>(op);
    }
}

// The migrated per-face edit ops keep the shell closed 2-manifold genus-0.
TEST(ManifoldEnforcement, MigratedInsetAndExtrudeAreManifold)
{
    {
        Mesh box = makeBox(1.f, 1.f, 1.f);
        InsetDesc d;
        d.amount = 0.3f;
        d.mode = InsetMode::Factor;
        d.replaceOriginal = true;
        Mesh out;
        ASSERT_TRUE(InsetFacesOperation::applyToAllFaces(box, d, out).valid);
        EXPECT_TRUE(isManifold(out));
        EXPECT_EQ(euler(out), 2);
    }
    {
        Mesh box = makeBox(1.f, 1.f, 1.f);
        ExtrudeDesc d;
        d.distance = 0.3f;
        d.keepOriginalFaces = false;
        Mesh out;
        ASSERT_TRUE(ExtrudeOperation::applyToAllFaces(box, d, out).valid);
        EXPECT_TRUE(isManifold(out));
        EXPECT_EQ(euler(out), 2);
    }
}

// Degenerate faces (a repeated vertex index → zero-area / non-simple) must be
// rejected at the half-edge boundary, not silently built into a broken twin map.
TEST(ManifoldEnforcement, FromMeshRejectsDegenerateFace)
{
    Mesh m;
    m.attributes().setPositions({{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0}});
    m.topology().addFace(Face{{0u, 1u, 2u}});
    m.topology().addFace(Face{{0u, 2u, 2u}});  // repeated index → degenerate
    EXPECT_FALSE(HalfEdgeMesh::fromMesh(m).has_value());
}

// A face with fewer than 3 vertices is not a valid polygon and is rejected.
TEST(ManifoldEnforcement, FromMeshRejectsSubTriangleFace)
{
    Mesh m;
    m.attributes().setPositions({{0, 0, 0}, {1, 0, 0}, {1, 1, 0}});
    m.topology().addFace(Face{{0u, 1u, 2u}});
    m.topology().addFace(Face{{0u, 1u}});  // 2-vertex "face"
    EXPECT_FALSE(HalfEdgeMesh::fromMesh(m).has_value());
}

}  // namespace nexus::geometry::testing
