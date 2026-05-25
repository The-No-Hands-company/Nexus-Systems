#include <nexus/geometry/GeometryKernel.h>

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>

using namespace nexus::geometry;

TEST(GeometryKernel, UploadContractTriangulatesQuadFace)
{
    Mesh mesh = primitives::makePlane(1.f, 1.f, 1, 1);  // single quad
    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);

    ASSERT_EQ(indices.size(), 6u);
    EXPECT_EQ(indices[0], 0u);
    EXPECT_EQ(indices[1], 2u);
    EXPECT_EQ(indices[2], 3u);
}

TEST(GeometryKernel, TopologyValidationAcceptsValidTriangleWithBoundaryEdges)
{
    Mesh mesh = primitives::makeTriangle(1.f);
    const TopologyValidationReport report =
        TopologyUtilities::validateTopology(mesh.topology(), mesh.attributes().vertexCount());

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.faceCount, 1u);
    EXPECT_EQ(report.boundaryEdgeCount, 3u);
    EXPECT_EQ(report.nonManifoldEdgeCount, 0u);
    EXPECT_TRUE(report.issues.empty());
}

TEST(GeometryKernel, TopologyValidationRejectsOutOfRangeIndices)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    mesh.topology().addFace(Face{{0u, 1u, 99u}});

    const TopologyValidationReport report =
        TopologyUtilities::validateTopology(mesh.topology(), mesh.attributes().vertexCount());

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.issues.size(), 1u);
    EXPECT_EQ(report.issues[0].code, TopologyIssueCode::IndexOutOfRange);
    EXPECT_EQ(report.issues[0].faceIndex, 0u);
    EXPECT_EQ(report.issues[0].vertexIndex, 99u);
}

TEST(GeometryKernel, TopologyValidationRejectsNonManifoldEdges)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f}, // 0
        {1.f, 0.f, 0.f}, // 1
        {0.f, 1.f, 0.f}, // 2
        {0.f, 0.f, 1.f}, // 3
        {0.f, -1.f, 0.f}, // 4
    });

    // Three triangles share edge (0,1), which is non-manifold.
    mesh.topology().addFace(Face{{0u, 1u, 2u}});
    mesh.topology().addFace(Face{{1u, 0u, 3u}});
    mesh.topology().addFace(Face{{0u, 1u, 4u}});

    const TopologyValidationReport report =
        TopologyUtilities::validateTopology(mesh.topology(), mesh.attributes().vertexCount());

    EXPECT_FALSE(report.valid);
    EXPECT_EQ(report.nonManifoldEdgeCount, 1u);
    ASSERT_EQ(report.issues.size(), 1u);
    EXPECT_EQ(report.issues[0].code, TopologyIssueCode::NonManifoldEdge);
    EXPECT_EQ(report.issues[0].edge.v0, 0u);
    EXPECT_EQ(report.issues[0].edge.v1, 1u);
}

TEST(GeometryKernel, UploadContractBuildViewAndValidate)
{
    Mesh mesh = primitives::makeTriangle(1.f);
    ASSERT_TRUE(mesh.computeVertexNormals());

    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, 17u);

    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const UploadValidationReport report = MeshUploadContract::validateView(view);

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(view.vertexCount, 3u);
    EXPECT_EQ(view.sections.size(), 1u);
    EXPECT_EQ(view.sections[0].material, 17u);
}

TEST(GeometryKernel, UploadContractPackedLayoutIsDeterministic)
{
    Mesh mesh = primitives::makeTriangle(1.f);
    ASSERT_TRUE(mesh.computeVertexNormals());
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });
    mesh.attributes().setSkinning(
        {
            JointIndex4{1, 2, 3, 4},
            JointIndex4{1, 2, 3, 4},
            JointIndex4{1, 2, 3, 4},
        },
        {
            JointWeight4{0.4f, 0.3f, 0.2f, 0.1f},
            JointWeight4{0.4f, 0.3f, 0.2f, 0.1f},
            JointWeight4{0.4f, 0.3f, 0.2f, 0.1f},
        });

    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, 3u);
    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);

    const PackedVertexLayout layout = MeshUploadContract::buildPackedVertexLayout(view);
    ASSERT_EQ(layout.bindings.size(), 1u);
    ASSERT_EQ(layout.attributes.size(), 5u);

    EXPECT_EQ(layout.bindings[0].binding, 0u);
    EXPECT_EQ(layout.bindings[0].strideBytes, 56u);  // float3 + float3 + float2 + u16x4 + float4

    EXPECT_EQ(layout.attributes[0].semantic, VertexSemantic::Position);
    EXPECT_EQ(layout.attributes[0].offsetBytes, 0u);
    EXPECT_EQ(layout.attributes[1].semantic, VertexSemantic::Normal);
    EXPECT_EQ(layout.attributes[1].offsetBytes, 12u);
    EXPECT_EQ(layout.attributes[2].semantic, VertexSemantic::UV0);
    EXPECT_EQ(layout.attributes[2].offsetBytes, 24u);
    EXPECT_EQ(layout.attributes[3].semantic, VertexSemantic::JointIndex4);
    EXPECT_EQ(layout.attributes[3].offsetBytes, 32u);
    EXPECT_EQ(layout.attributes[4].semantic, VertexSemantic::JointWeight4);
    EXPECT_EQ(layout.attributes[4].offsetBytes, 40u);
}

TEST(GeometryKernel, ToGpuVertexInputLayoutMapsAllSemanticFormats)
{
    // Mesh-derived vertex-input wiring: a packed layout translates to gfx pipeline
    // vertex bindings/attributes a graphics pipeline can consume.
    Mesh mesh = primitives::makeTriangle(1.f);
    ASSERT_TRUE(mesh.computeVertexNormals());
    mesh.attributes().setUVs({ {0.f, 0.f}, {1.f, 0.f}, {0.f, 1.f} });
    mesh.attributes().setSkinning(
        { JointIndex4{1, 2, 3, 4}, JointIndex4{1, 2, 3, 4}, JointIndex4{1, 2, 3, 4} },
        { JointWeight4{0.4f, 0.3f, 0.2f, 0.1f},
          JointWeight4{0.4f, 0.3f, 0.2f, 0.1f},
          JointWeight4{0.4f, 0.3f, 0.2f, 0.1f} });

    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, 0u);
    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const PackedVertexLayout packed = MeshUploadContract::buildPackedVertexLayout(view);

    const GpuVertexInputLayout gpu = MeshUploadContract::toGpuVertexInputLayout(packed);

    using nexus::gfx::Format;
    using nexus::gfx::VertexInputRate;

    ASSERT_EQ(gpu.bindings.size(), 1u);
    EXPECT_EQ(gpu.bindings[0].binding, 0u);
    EXPECT_EQ(gpu.bindings[0].stride, 56u);  // mirrors the packed layout stride
    EXPECT_EQ(gpu.bindings[0].inputRate, VertexInputRate::Vertex);

    ASSERT_EQ(gpu.attributes.size(), 5u);
    // Position Float3 @0, Normal Float3 @12, UV0 Float2 @24, JointIndex4 Uint16x4 @32,
    // JointWeight4 Float4 @40 — exercising every VertexElementFormat mapping.
    EXPECT_EQ(gpu.attributes[0].format, Format::R32G32B32_Float);    EXPECT_EQ(gpu.attributes[0].offset, 0u);
    EXPECT_EQ(gpu.attributes[1].format, Format::R32G32B32_Float);    EXPECT_EQ(gpu.attributes[1].offset, 12u);
    EXPECT_EQ(gpu.attributes[2].format, Format::R32G32_Float);       EXPECT_EQ(gpu.attributes[2].offset, 24u);
    EXPECT_EQ(gpu.attributes[3].format, Format::R16G16B16A16_Uint);  EXPECT_EQ(gpu.attributes[3].offset, 32u);
    EXPECT_EQ(gpu.attributes[4].format, Format::R32G32B32A32_Float); EXPECT_EQ(gpu.attributes[4].offset, 40u);
    for (uint32_t i = 0; i < 5u; ++i) {
        EXPECT_EQ(gpu.attributes[i].location, i);
        EXPECT_EQ(gpu.attributes[i].binding, 0u);
    }
}

TEST(GeometryKernel, UploadContractIncludesTangentChannelWhenPresent)
{
    Mesh mesh = primitives::makeTriangle(1.f);
    ASSERT_TRUE(mesh.computeVertexNormals());
    mesh.attributes().setTangents({
        {1.f, 0.f, 0.f, 1.f},
        {1.f, 0.f, 0.f, 1.f},
        {1.f, 0.f, 0.f, 1.f},
    });
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });

    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, 0u);
    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const PackedVertexLayout layout = MeshUploadContract::buildPackedVertexLayout(view);

    ASSERT_EQ(layout.attributes.size(), 4u);
    EXPECT_EQ(layout.attributes[0].semantic, VertexSemantic::Position);
    EXPECT_EQ(layout.attributes[1].semantic, VertexSemantic::Normal);
    EXPECT_EQ(layout.attributes[2].semantic, VertexSemantic::Tangent);
    EXPECT_EQ(layout.attributes[3].semantic, VertexSemantic::UV0);
    EXPECT_EQ(layout.bindings[0].strideBytes, 48u);
    EXPECT_EQ(layout.attributes[2].offsetBytes, 24u);
    EXPECT_EQ(layout.attributes[3].offsetBytes, 40u);
}

TEST(GeometryKernel, UploadContractPacksTangentChannelAtExpectedOffset)
{
    Mesh mesh = primitives::makeTriangle(1.f);
    ASSERT_TRUE(mesh.computeVertexNormals());
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });
    ASSERT_TRUE(mesh.computeVertexTangents());

    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, 0u);
    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const PackedVertexLayout layout = MeshUploadContract::buildPackedVertexLayout(view);
    const std::vector<uint8_t> packed = MeshUploadContract::packInterleavedVertexBuffer(view, layout);

    ASSERT_EQ(layout.attributes.size(), 4u);
    ASSERT_EQ(packed.size(), static_cast<size_t>(3u * layout.bindings[0].strideBytes));

    float tangentX = 0.f;
    float tangentW = 0.f;
    std::memcpy(&tangentX,
                packed.data() + layout.attributes[2].offsetBytes,
                sizeof(float));
    std::memcpy(&tangentW,
                packed.data() + layout.attributes[2].offsetBytes + 3u * sizeof(float),
                sizeof(float));

    EXPECT_FLOAT_EQ(tangentX, mesh.attributes().tangents()[0].x);
    EXPECT_FLOAT_EQ(tangentW, 1.f);
    EXPECT_EQ(layout.bindings[0].strideBytes, 48u);
}

TEST(GeometryKernel, ReconstructBitangentUsesHandednessSign)
{
    const nexus::render::Vec3 normal{0.f, 1.f, 0.f};
    const Vec4 tangentPositive{1.f, 0.f, 0.f, 1.f};
    const Vec4 tangentNegative{1.f, 0.f, 0.f, -1.f};

    const nexus::render::Vec3 bitangentPositive =
        MeshUploadContract::reconstructBitangent(normal, tangentPositive);
    const nexus::render::Vec3 bitangentNegative =
        MeshUploadContract::reconstructBitangent(normal, tangentNegative);

    EXPECT_NEAR(bitangentPositive.x, 0.f, 1e-6f);
    EXPECT_NEAR(bitangentPositive.y, 0.f, 1e-6f);
    EXPECT_NEAR(bitangentPositive.z, -1.f, 1e-6f);

    EXPECT_NEAR(bitangentNegative.x, 0.f, 1e-6f);
    EXPECT_NEAR(bitangentNegative.y, 0.f, 1e-6f);
    EXPECT_NEAR(bitangentNegative.z, 1.f, 1e-6f);
}

TEST(GeometryKernel, PackedTangentSpaceParityWithMirroredUVs)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, -1.f},
    });
    mesh.topology().addFace(Face{{0u, 1u, 2u}});

    ASSERT_TRUE(mesh.computeVertexNormals());
    ASSERT_TRUE(mesh.computeVertexTangents());

    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, 0u);
    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const PackedVertexLayout layout = MeshUploadContract::buildPackedVertexLayout(view);
    const std::vector<uint8_t> packed = MeshUploadContract::packInterleavedVertexBuffer(view, layout);

    ASSERT_EQ(layout.attributes.size(), 4u);
    ASSERT_EQ(packed.size(), static_cast<size_t>(3u * layout.bindings[0].strideBytes));

    float nx = 0.f;
    float ny = 0.f;
    float nz = 0.f;
    float tx = 0.f;
    float ty = 0.f;
    float tz = 0.f;
    float tw = 0.f;
    std::memcpy(&nx, packed.data() + layout.attributes[1].offsetBytes + 0u * sizeof(float), sizeof(float));
    std::memcpy(&ny, packed.data() + layout.attributes[1].offsetBytes + 1u * sizeof(float), sizeof(float));
    std::memcpy(&nz, packed.data() + layout.attributes[1].offsetBytes + 2u * sizeof(float), sizeof(float));
    std::memcpy(&tx, packed.data() + layout.attributes[2].offsetBytes + 0u * sizeof(float), sizeof(float));
    std::memcpy(&ty, packed.data() + layout.attributes[2].offsetBytes + 1u * sizeof(float), sizeof(float));
    std::memcpy(&tz, packed.data() + layout.attributes[2].offsetBytes + 2u * sizeof(float), sizeof(float));
    std::memcpy(&tw, packed.data() + layout.attributes[2].offsetBytes + 3u * sizeof(float), sizeof(float));

    EXPECT_FLOAT_EQ(tw, -1.f);

    const nexus::render::Vec3 normal{nx, ny, nz};
    const Vec4 tangent{tx, ty, tz, tw};
    const nexus::render::Vec3 bitangent = MeshUploadContract::reconstructBitangent(normal, tangent);

    EXPECT_NEAR(bitangent.x, 0.f, 1e-5f);
    EXPECT_NEAR(bitangent.y, 0.f, 1e-5f);
    EXPECT_LT(bitangent.z, 0.f);
    EXPECT_NEAR(std::abs(bitangent.z), 1.f, 1e-5f);

    const float bitangentLen = std::sqrt(bitangent.x * bitangent.x + bitangent.y * bitangent.y + bitangent.z * bitangent.z);
    EXPECT_NEAR(bitangentLen, 1.f, 1e-5f);
}

TEST(GeometryKernel, UploadContractValidationRejectsTangentWithoutNormals)
{
    Mesh mesh = primitives::makeTriangle(1.f);
    mesh.attributes().setTangents({
        {1.f, 0.f, 0.f, 1.f},
        {1.f, 0.f, 0.f, 1.f},
        {1.f, 0.f, 0.f, 1.f},
    });
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });

    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, 0u);
    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const UploadValidationReport report = MeshUploadContract::validateView(view);

    EXPECT_FALSE(report.valid);
}

TEST(GeometryKernel, UploadContractValidationRejectsTangentWithoutUV0)
{
    Mesh mesh = primitives::makeTriangle(1.f);
    ASSERT_TRUE(mesh.computeVertexNormals());
    mesh.attributes().setTangents({
        {1.f, 0.f, 0.f, 1.f},
        {1.f, 0.f, 0.f, 1.f},
        {1.f, 0.f, 0.f, 1.f},
    });

    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, 0u);
    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const UploadValidationReport report = MeshUploadContract::validateView(view);

    EXPECT_FALSE(report.valid);
}

TEST(GeometryKernel, UploadContractPacksInterleavedVertexBuffer)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {1.f, 2.f, 3.f},
        {4.f, 5.f, 6.f},
        {7.f, 8.f, 9.f},
    });
    mesh.attributes().setUVs({
        {0.1f, 0.2f},
        {0.3f, 0.4f},
        {0.5f, 0.6f},
    });
    mesh.topology().addFace(Face{{0, 1, 2}});

    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, 0u);
    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);

    const PackedVertexLayout layout = MeshUploadContract::buildPackedVertexLayout(view);
    const std::vector<uint8_t> packed = MeshUploadContract::packInterleavedVertexBuffer(view, layout);

    ASSERT_EQ(layout.bindings.size(), 1u);
    ASSERT_EQ(layout.attributes.size(), 2u);
    ASSERT_EQ(packed.size(), static_cast<size_t>(3u * layout.bindings[0].strideBytes));

    float posX = 0.f;
    float uvU = 0.f;
    const uint32_t stride = layout.bindings[0].strideBytes;

    std::memcpy(&posX,
                packed.data() + static_cast<size_t>(1u) * stride + layout.attributes[0].offsetBytes,
                sizeof(float));
    std::memcpy(&uvU,
                packed.data() + static_cast<size_t>(1u) * stride + layout.attributes[1].offsetBytes,
                sizeof(float));

    EXPECT_FLOAT_EQ(posX, 4.f);
    EXPECT_FLOAT_EQ(uvU, 0.3f);
}

TEST(GeometryKernel, UploadContractValidationRejectsDuplicateSemantic)
{
    Mesh mesh = primitives::makeTriangle(1.f);
    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, 0u);

    MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    view.vertexStreams.push_back(view.vertexStreams.front());

    const UploadValidationReport report = MeshUploadContract::validateView(view);
    EXPECT_FALSE(report.valid);
}

TEST(GeometryKernel, CommandSurfaceExecutesTransformOperation)
{
    Mesh mesh = primitives::makeTriangle(1.f);
    mesh.rebuildStableElementIds();
    const auto before = mesh.attributes().positions();
    const auto beforeIds = mesh.stableElementIds();

    GeometryCommandDesc command{};
    command.type = GeometryCommandType::Transform;
    command.transform = nexus::render::Mat4::identity();
    command.transform.m[0][3] = 2.f;
    command.transform.m[1][3] = -1.f;

    const GeometryCommandReport report = GeometryCommandSurface::execute(mesh, command);

    EXPECT_TRUE(report.valid);
    EXPECT_FLOAT_EQ(mesh.attributes().positions()[0].x, before[0].x + 2.f);
    EXPECT_FLOAT_EQ(mesh.attributes().positions()[0].y, before[0].y - 1.f);
    EXPECT_EQ(mesh.stableElementIds().vertexIds, beforeIds.vertexIds);
}

TEST(GeometryKernel, CommandSurfaceExecutesAppendMergeOperation)
{
    Mesh base = primitives::makeTriangle(1.f);
    ASSERT_TRUE(base.computeVertexNormals());
    base.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });
    ASSERT_TRUE(base.computeVertexTangents());

    Mesh mergeMesh = primitives::makeTriangle(2.f);
    ASSERT_TRUE(mergeMesh.computeVertexNormals());
    mergeMesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });
    ASSERT_TRUE(mergeMesh.computeVertexTangents());

    GeometryCommandDesc command{};
    command.type = GeometryCommandType::AppendMerge;
    command.mergeMesh = &mergeMesh;

    const GeometryCommandReport report = GeometryCommandSurface::execute(base, command);

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(base.attributes().vertexCount(), 6u);
    EXPECT_EQ(base.topology().faceCount(), 2u);
    EXPECT_EQ(base.topology().face(1).indices, (std::vector<uint32_t>{3, 4, 5}));
}

TEST(GeometryKernel, CommandSurfaceExecutesSplitFaceRangeOperation)
{
    Mesh mesh = primitives::makePlane(1.f, 1.f, 1, 2);
    mesh.rebuildStableElementIds();
    Mesh splitOut;

    GeometryCommandDesc command{};
    command.type = GeometryCommandType::SplitFaceRange;
    command.firstFace = 1u;
    command.faceCount = 1u;
    command.splitOutputMesh = &splitOut;

    const GeometryCommandReport report = GeometryCommandSurface::execute(mesh, command);

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(mesh.topology().faceCount(), 1u);
    EXPECT_EQ(splitOut.topology().faceCount(), 1u);
    EXPECT_TRUE(mesh.hasStableElementIds());
    EXPECT_TRUE(splitOut.hasStableElementIds());
}

TEST(GeometryKernel, CommandSurfaceExecutesWeldOperation)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
    });
    mesh.attributes().setNormals({
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {1.f, 1.f},
        {0.f, 0.f},
        {1.f, 1.f},
        {0.f, 1.f},
    });
    Face first{};
    first.indices = {0, 1, 2};
    mesh.topology().addFace(first);
    Face second{};
    second.indices = {3, 4, 5};
    mesh.topology().addFace(second);
    ASSERT_TRUE(mesh.computeVertexTangents());

    GeometryCommandDesc command{};
    command.type = GeometryCommandType::WeldCoincidentVertices;
    command.weldEpsilon = 1e-5f;

    const GeometryCommandReport report = GeometryCommandSurface::execute(mesh, command);

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(mesh.attributes().vertexCount(), 4u);
    EXPECT_EQ(mesh.topology().face(1).indices, (std::vector<uint32_t>{0, 2, 3}));
}

TEST(GeometryKernel, CommandSurfaceRejectsAppendMergeWithoutSourceMesh)
{
    Mesh mesh = primitives::makeTriangle(1.f);

    GeometryCommandDesc command{};
    command.type = GeometryCommandType::AppendMerge;

    const GeometryCommandReport report = GeometryCommandSurface::execute(mesh, command);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.errors.size(), 1u);
}

TEST(GeometryKernel, CommandSurfaceRejectsSplitWithoutOutputMesh)
{
    Mesh mesh = primitives::makePlane(1.f, 1.f, 1, 2);

    GeometryCommandDesc command{};
    command.type = GeometryCommandType::SplitFaceRange;
    command.firstFace = 0u;
    command.faceCount = 1u;

    const GeometryCommandReport report = GeometryCommandSurface::execute(mesh, command);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.errors.size(), 1u);
}

TEST(GeometryKernel, CommandSurfaceRejectsNegativeWeldEpsilon)
{
    Mesh mesh = primitives::makeTriangle(1.f);

    GeometryCommandDesc command{};
    command.type = GeometryCommandType::WeldCoincidentVertices;
    command.weldEpsilon = -1.f;

    const GeometryCommandReport report = GeometryCommandSurface::execute(mesh, command);

    EXPECT_FALSE(report.valid);
    ASSERT_EQ(report.errors.size(), 1u);
}

TEST(GeometryKernel, RenderBridgeUploadsBuffersAndPreservesSections)
{
    auto device = nexus::gfx::createDevice(nexus::gfx::Backend::Null);
    ASSERT_NE(device, nullptr);

    Mesh mesh = primitives::makeTriangle(1.f);
    ASSERT_TRUE(mesh.computeVertexNormals());

    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const std::vector<MeshSection> sections = {
        MeshSection{.firstIndex = 0u, .indexCount = static_cast<uint32_t>(indices.size()), .material = 11u},
    };

    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const PackedVertexLayout layout = MeshUploadContract::buildPackedVertexLayout(view);

    UploadedGeometryMesh uploaded{};
    const UploadToDeviceReport report =
        GeometryRenderBridge::uploadToDevice(*device, view, layout, sections, uploaded);

    EXPECT_TRUE(report.valid);
    EXPECT_TRUE(uploaded.meshRef.vertexBuffer.valid());
    EXPECT_TRUE(uploaded.meshRef.indexBuffer.valid());
    EXPECT_EQ(uploaded.meshRef.indexCount, indices.size());
    ASSERT_EQ(uploaded.sections.size(), 1u);
    EXPECT_EQ(uploaded.sections[0].material, 11u);

    GeometryRenderBridge::destroyUploadedMesh(*device, uploaded);
    EXPECT_FALSE(uploaded.meshRef.vertexBuffer.valid());
    EXPECT_FALSE(uploaded.meshRef.indexBuffer.valid());
    EXPECT_TRUE(uploaded.sections.empty());
}

TEST(GeometryKernel, RenderBridgeRejectsInvalidUploadView)
{
    auto device = nexus::gfx::createDevice(nexus::gfx::Backend::Null);
    ASSERT_NE(device, nullptr);

    Mesh mesh = primitives::makeTriangle(1.f);
    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, 1u);

    MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    view.vertexStreams.push_back(view.vertexStreams.front()); // duplicate semantic -> invalid

    const PackedVertexLayout layout = MeshUploadContract::buildPackedVertexLayout(view);
    UploadedGeometryMesh uploaded{};
    const UploadToDeviceReport report =
        GeometryRenderBridge::uploadToDevice(*device, view, layout, sections, uploaded);

    EXPECT_FALSE(report.valid);
    EXPECT_FALSE(uploaded.meshRef.vertexBuffer.valid());
    EXPECT_FALSE(uploaded.meshRef.indexBuffer.valid());
}

TEST(GeometryKernel, RenderBridgeBlasHookSkipsOnNonRTDevice)
{
    auto device = nexus::gfx::createDevice(nexus::gfx::Backend::Null);
    ASSERT_NE(device, nullptr);

    Mesh mesh = primitives::makeTriangle(1.f);
    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, 2u);

    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const PackedVertexLayout layout = MeshUploadContract::buildPackedVertexLayout(view);

    UploadedGeometryMesh uploaded{};
    GeometryUploadOptions options{};
    options.buildBlasIfSupported = true;

    const UploadToDeviceReport report =
        GeometryRenderBridge::uploadToDevice(*device, view, layout, sections, options, uploaded);

    EXPECT_TRUE(report.valid);
    EXPECT_FALSE(uploaded.meshRef.blas.valid());

    GeometryRenderBridge::destroyUploadedMesh(*device, uploaded);
}

TEST(GeometryKernel, RenderBridgeBuildsOnePacketPerSection)
{
    auto device = nexus::gfx::createDevice(nexus::gfx::Backend::Null);
    ASSERT_NE(device, nullptr);

    Mesh mesh = primitives::makePlane(1.f, 1.f, 1, 1); // 2 triangles => 6 indices
    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    ASSERT_EQ(indices.size(), 6u);

    std::vector<MeshSection> sections = {
        MeshSection{.firstIndex = 0u, .indexCount = 3u, .material = 10u},
        MeshSection{.firstIndex = 3u, .indexCount = 3u, .material = 20u},
    };

    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const PackedVertexLayout layout = MeshUploadContract::buildPackedVertexLayout(view);

    UploadedGeometryMesh uploaded{};
    const UploadToDeviceReport report =
        GeometryRenderBridge::uploadToDevice(*device, view, layout, sections, uploaded);
    ASSERT_TRUE(report.valid);

    const auto packets = GeometryRenderBridge::buildDrawPackets(uploaded);
    ASSERT_EQ(packets.size(), 2u);
    EXPECT_EQ(packets[0].firstIndex, 0u);
    EXPECT_EQ(packets[0].indexCount, 3u);
    EXPECT_EQ(packets[0].material, 10u);
    EXPECT_EQ(packets[1].firstIndex, 3u);
    EXPECT_EQ(packets[1].indexCount, 3u);
    EXPECT_EQ(packets[1].material, 20u);

    GeometryRenderBridge::destroyUploadedMesh(*device, uploaded);
}

TEST(GeometryKernel, RenderBridgeDrawPacketFallsBackToFullRangeWhenNoSections)
{
    UploadedGeometryMesh uploaded{};
    uploaded.meshRef.vertexBuffer.id = 11u;
    uploaded.meshRef.indexBuffer.id = 12u;
    uploaded.meshRef.indexCount = 9u;

    const auto packets = GeometryRenderBridge::buildDrawPackets(uploaded);
    ASSERT_EQ(packets.size(), 1u);
    EXPECT_EQ(packets[0].firstIndex, 0u);
    EXPECT_EQ(packets[0].indexCount, 9u);
    EXPECT_EQ(packets[0].material, nexus::render::kInvalidMaterial);
}

TEST(GeometryKernel, RenderBridgeConvertsDrawPacketsToSceneSectionPackets)
{
    std::vector<GeometryDrawPacket> drawPackets = {
        GeometryDrawPacket{.firstIndex = 0u, .indexCount = 3u, .material = 10u},
        GeometryDrawPacket{.firstIndex = 3u, .indexCount = 3u, .material = 20u},
    };

    const auto scenePackets = GeometryRenderBridge::toSceneSectionDrawPackets(drawPackets);
    ASSERT_EQ(scenePackets.size(), 2u);
    EXPECT_EQ(scenePackets[0].firstIndex, 0u);
    EXPECT_EQ(scenePackets[0].indexCount, 3u);
    EXPECT_EQ(scenePackets[0].material, 10u);
    EXPECT_EQ(scenePackets[1].firstIndex, 3u);
    EXPECT_EQ(scenePackets[1].indexCount, 3u);
    EXPECT_EQ(scenePackets[1].material, 20u);
}

TEST(GeometryKernel, RenderBridgeAssignUploadedMeshToNodeOneCall)
{
    auto device = nexus::gfx::createDevice(nexus::gfx::Backend::Null);
    ASSERT_NE(device, nullptr);

    Mesh mesh = primitives::makePlane(1.f, 1.f, 1, 1);
    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    std::vector<MeshSection> sections = {
        MeshSection{.firstIndex = 0u, .indexCount = 3u, .material = 31u},
        MeshSection{.firstIndex = 3u, .indexCount = 3u, .material = 41u},
    };

    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const PackedVertexLayout layout = MeshUploadContract::buildPackedVertexLayout(view);

    UploadedGeometryMesh uploaded{};
    const UploadToDeviceReport report =
        GeometryRenderBridge::uploadToDevice(*device, view, layout, sections, uploaded);
    ASSERT_TRUE(report.valid);

    nexus::render::SceneGraph scene;
    nexus::render::Node* node = scene.createNode("handoff");
    ASSERT_NE(node, nullptr);

    GeometryRenderBridge::assignUploadedMeshToNode(uploaded, *node);

    EXPECT_TRUE(node->mesh.vertexBuffer.valid());
    EXPECT_TRUE(node->mesh.indexBuffer.valid());
    EXPECT_EQ(node->mesh.indexCount, 6u);
    ASSERT_EQ(node->sectionDrawPackets.size(), 2u);
    EXPECT_EQ(node->sectionDrawPackets[0].material, 31u);
    EXPECT_EQ(node->sectionDrawPackets[1].material, 41u);

    GeometryRenderBridge::destroyUploadedMesh(*device, uploaded);
}

TEST(GeometryKernel, RenderBridgeAdoptUploadedMeshByNodeMovesOwnership)
{
    auto device = nexus::gfx::createDevice(nexus::gfx::Backend::Null);
    ASSERT_NE(device, nullptr);

    Mesh mesh = primitives::makePlane(1.f, 1.f, 1, 1);
    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    std::vector<MeshSection> sections = {
        MeshSection{.firstIndex = 0u, .indexCount = 3u, .material = 51u},
        MeshSection{.firstIndex = 3u, .indexCount = 3u, .material = 61u},
    };

    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const PackedVertexLayout layout = MeshUploadContract::buildPackedVertexLayout(view);

    UploadedGeometryMesh uploaded{};
    const UploadToDeviceReport report =
        GeometryRenderBridge::uploadToDevice(*device, view, layout, sections, uploaded);
    ASSERT_TRUE(report.valid);

    const uint64_t oldVbo = uploaded.meshRef.vertexBuffer.id;
    const uint64_t oldIbo = uploaded.meshRef.indexBuffer.id;

    nexus::render::SceneGraph scene;
    nexus::render::Node* node = scene.createNode("adopt");
    ASSERT_NE(node, nullptr);

    GeometryRenderBridge::adoptUploadedMeshByNode(uploaded, *node);

    EXPECT_EQ(node->mesh.vertexBuffer.id, oldVbo);
    EXPECT_EQ(node->mesh.indexBuffer.id, oldIbo);
    ASSERT_EQ(node->sectionDrawPackets.size(), 2u);
    EXPECT_FALSE(uploaded.meshRef.vertexBuffer.valid());
    EXPECT_FALSE(uploaded.meshRef.indexBuffer.valid());
    EXPECT_TRUE(uploaded.sections.empty());

    GeometryRenderBridge::destroyNodeMeshPayload(*device, *node);
}

TEST(GeometryKernel, RenderBridgeDestroyNodeMeshPayloadClearsNodeState)
{
    auto device = nexus::gfx::createDevice(nexus::gfx::Backend::Null);
    ASSERT_NE(device, nullptr);

    nexus::render::SceneGraph scene;
    nexus::render::Node* node = scene.createNode("destroy-payload");
    ASSERT_NE(node, nullptr);

    node->mesh.vertexBuffer = device->createBuffer({
        .sizeBytes = 64,
        .usage = nexus::gfx::BufferUsage::VertexBuffer,
        .memory = nexus::gfx::MemoryHint::GpuOnly,
        .debugName = "test.node.vbo",
    });
    node->mesh.indexBuffer = device->createBuffer({
        .sizeBytes = 64,
        .usage = nexus::gfx::BufferUsage::IndexBuffer,
        .memory = nexus::gfx::MemoryHint::GpuOnly,
        .debugName = "test.node.ibo",
    });
    node->mesh.indexCount = 6u;
    node->sectionDrawPackets = {
        nexus::render::MeshSectionDrawPacket{.firstIndex = 0u, .indexCount = 3u, .material = 7u},
    };

    ASSERT_TRUE(node->mesh.vertexBuffer.valid());
    ASSERT_TRUE(node->mesh.indexBuffer.valid());
    ASSERT_FALSE(node->sectionDrawPackets.empty());

    GeometryRenderBridge::destroyNodeMeshPayload(*device, *node);

    EXPECT_FALSE(node->mesh.vertexBuffer.valid());
    EXPECT_FALSE(node->mesh.indexBuffer.valid());
    EXPECT_EQ(node->mesh.indexCount, 0u);
    EXPECT_TRUE(node->sectionDrawPackets.empty());
}

TEST(GeometryKernel, UploadContractDetectsInvalidSectionRange)
{
    Mesh mesh = primitives::makeTriangle(1.f);
    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);

    const std::vector<MeshSection> badSections = {
        MeshSection{.firstIndex = 1u, .indexCount = 9u, .material = 1u},
    };

    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, badSections);
    const UploadValidationReport report = MeshUploadContract::validateView(view);
    EXPECT_FALSE(report.valid);
    EXPECT_FALSE(report.errors.empty());
}

TEST(GeometryKernel, SkinningBridgeBuildJointRemapByName)
{
    nexus::animation::Skeleton skeleton;
    EXPECT_EQ(skeleton.addBone({.name = "root", .parentIndex = -1}), 0);
    EXPECT_EQ(skeleton.addBone({.name = "spine", .parentIndex = 0}), 1);

    const std::vector<std::string> meshJoints = {"root", "spine"};
    const auto report = SkinningBridge::buildJointRemap(meshJoints, skeleton);

    EXPECT_TRUE(report.valid);
    ASSERT_EQ(report.jointRemap.size(), 2u);
    EXPECT_EQ(report.jointRemap[0].skeletonJointIndex, 0);
    EXPECT_EQ(report.jointRemap[1].skeletonJointIndex, 1);
}

TEST(GeometryKernel, SkinningBridgeReportsMissingJointNames)
{
    nexus::animation::Skeleton skeleton;
    EXPECT_EQ(skeleton.addBone({.name = "root", .parentIndex = -1}), 0);

    const std::vector<std::string> meshJoints = {"root", "arm"};
    const auto report = SkinningBridge::buildJointRemap(meshJoints, skeleton);

    EXPECT_FALSE(report.valid);
    ASSERT_FALSE(report.errors.empty());
}

TEST(GeometryKernel, SkinningBridgeRemapsMeshJointIndicesToSkeletonSpace)
{
    Mesh mesh = primitives::makeTriangle(1.f);
    mesh.attributes().setSkinning(
        {
            JointIndex4{0, 1, 1, 0},
            JointIndex4{0, 1, 1, 0},
            JointIndex4{0, 1, 1, 0},
        },
        {
            JointWeight4{0.5f, 0.5f, 0.f, 0.f},
            JointWeight4{0.5f, 0.5f, 0.f, 0.f},
            JointWeight4{0.5f, 0.5f, 0.f, 0.f},
        });

    nexus::animation::Skeleton skeleton;
    EXPECT_EQ(skeleton.addBone({.name = "hip", .parentIndex = -1}), 0);
    EXPECT_EQ(skeleton.addBone({.name = "knee", .parentIndex = 0}), 1);
    EXPECT_EQ(skeleton.addBone({.name = "ankle", .parentIndex = 1}), 2);

    // Mesh joint 0 maps to skeleton joint 2, mesh joint 1 maps to skeleton joint 0.
    const std::vector<std::string> meshJointNames = {"ankle", "hip"};

    SkinningValidationReport report;
    const bool ok = SkinningBridge::remapMeshSkinningToSkeleton(mesh, meshJointNames, skeleton, report);

    EXPECT_TRUE(ok);
    EXPECT_TRUE(report.valid);
    EXPECT_EQ(mesh.attributes().jointIndices()[0][0], 2u);
    EXPECT_EQ(mesh.attributes().jointIndices()[0][1], 0u);
}

TEST(GeometryKernel, TopologyUtilitiesExtractEdgeListUniquely)
{
    Mesh mesh = primitives::makePlane(1.f, 1.f, 1, 1);  // 1 quad -> 4 unique edges
    const auto edges = TopologyUtilities::extractEdgeList(mesh.topology());

    EXPECT_EQ(edges.size(), 4u);
}

TEST(GeometryKernel, TopologyUtilitiesFindBoundaryLoopOnPlane)
{
    Mesh mesh = primitives::makePlane(1.f, 1.f, 1, 1);
    const auto loops = TopologyUtilities::extractBoundaryLoops(mesh.topology());

    ASSERT_FALSE(loops.empty());
    // One outer boundary for a single quad.
    EXPECT_EQ(loops.size(), 1u);
    EXPECT_GE(loops[0].size(), 5u);  // 4 verts + repeated start
}

TEST(GeometryKernel, TopologyUtilitiesDetectConnectedComponents)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
        {10.f, 0.f, 0.f},
        {11.f, 0.f, 0.f},
        {10.f, 0.f, 1.f},
    });
    mesh.topology().addFace(Face{{0, 1, 2}});
    mesh.topology().addFace(Face{{3, 4, 5}});

    const auto comps = TopologyUtilities::connectedFaceComponents(mesh.topology());
    EXPECT_EQ(comps.size(), 2u);
}
