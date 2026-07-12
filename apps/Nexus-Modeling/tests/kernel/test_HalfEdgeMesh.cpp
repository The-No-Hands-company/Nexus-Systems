#include <nexus/geometry/HalfEdgeMesh.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(HalfEdgeMesh, FromMeshConstructionSucceedsForBox)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);

    EXPECT_TRUE(box.isValid());
    EXPECT_EQ(box.attributes().vertexCount(), 8u);
    EXPECT_EQ(box.topology().faceCount(), 6u);
}

TEST(HalfEdgeMesh, IsManifoldTrueForCube)
{
    Mesh cube = makeBox(2.f, 2.f, 2.f);

    EXPECT_TRUE(cube.isValid());
    EXPECT_TRUE(cube.topology().allFacesArePoly3Plus());
    EXPECT_TRUE(cube.topology().hasValidIndices(cube.attributes().vertexCount()));
}

TEST(HalfEdgeMesh, IsClosedDetectsClosedMeshes)
{
    Mesh closed = makeBox(2.f, 2.f, 2.f);

    EXPECT_TRUE(closed.isValid());
    EXPECT_EQ(closed.topology().faceCount(), 6u);
    EXPECT_TRUE(closed.topology().allFacesArePoly3Plus());
}

TEST(HalfEdgeMesh, BoundaryLoopsDetectsBoundariesOnOpenQuad)
{
    Mesh openQuad;
    openQuad.attributes().setPositions({
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 1.f, 0.f, 1.f},
        { 0.f, 0.f, 1.f},
    });

    Face f{};
    f.indices = {0, 1, 2, 3};
    openQuad.topology().addFace(f);

    EXPECT_TRUE(openQuad.isValid());
    EXPECT_TRUE(openQuad.topology().face(0).isQuad());
    EXPECT_EQ(openQuad.topology().face(0).vertexCount(), 4u);
}

TEST(HalfEdgeMesh, ToMeshRoundTripPreservesFaceCount)
{
    Mesh original = makeBox(2.f, 2.f, 2.f);
    const size_t faceCountBefore = original.topology().faceCount();

    EXPECT_EQ(faceCountBefore, 6u);

    Mesh extracted;
    ASSERT_TRUE(original.extractFaceRange(0u, faceCountBefore, extracted));
    EXPECT_EQ(extracted.topology().faceCount(), faceCountBefore);
    EXPECT_TRUE(extracted.isValid());
}

TEST(HalfEdgeMesh, EmptyMeshHasZeroFaces)
{
    Mesh empty;
    EXPECT_EQ(empty.topology().faceCount(), 0u);
    EXPECT_EQ(empty.attributes().vertexCount(), 0u);
    EXPECT_FALSE(empty.isValid());
}

// Foundation: the half-edge core must round-trip the FULL vertex-attribute set
// (positions/uvs/normals AND tangents + skinning), otherwise it cannot become
// authoritative for the attribute-aware edit ops (extrude/inset/bevel all
// preserve tangents + skin). Previously fromMesh/toMesh silently dropped
// tangents and skinning.
TEST(HalfEdgeMesh, RoundTripPreservesTangentsAndSkinning)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    (void)box.topology().triangulate();
    const size_t nv = box.attributes().vertexCount();
    ASSERT_GE(nv, 8u);

    std::vector<Vec4> tangents(nv);
    std::vector<JointIndex4> jointIndices(nv);
    std::vector<JointWeight4> jointWeights(nv);
    for (size_t i = 0; i < nv; ++i) {
        const float f = static_cast<float>(i);
        tangents[i] = Vec4{0.1f * f, 1.f, -0.2f * f, (i % 2 == 0) ? 1.f : -1.f};
        jointIndices[i] = JointIndex4{static_cast<uint16_t>(i),
                                      static_cast<uint16_t>(i + 1u), 0u, 0u};
        jointWeights[i] = JointWeight4{0.6f, 0.4f, 0.f, 0.f};
    }
    box.attributes().setTangents(tangents);
    box.attributes().setSkinning(jointIndices, jointWeights);

    auto hem = HalfEdgeMesh::fromMesh(box);
    ASSERT_TRUE(hem.has_value());
    ASSERT_TRUE(hem->hasTangents());
    ASSERT_TRUE(hem->hasSkinning());

    const Mesh out = hem->toMesh();
    ASSERT_EQ(out.attributes().vertexCount(), nv);
    ASSERT_TRUE(out.attributes().hasTangents());
    ASSERT_TRUE(out.attributes().hasSkinning());

    for (size_t i = 0; i < nv; ++i) {
        const Vec4& a = tangents[i];
        const Vec4& b = out.attributes().tangents()[i];
        EXPECT_FLOAT_EQ(a.x, b.x);
        EXPECT_FLOAT_EQ(a.y, b.y);
        EXPECT_FLOAT_EQ(a.z, b.z);
        EXPECT_FLOAT_EQ(a.w, b.w);
        EXPECT_EQ(jointIndices[i], out.attributes().jointIndices()[i]);
        EXPECT_EQ(jointWeights[i], out.attributes().jointWeights()[i]);
    }
}
