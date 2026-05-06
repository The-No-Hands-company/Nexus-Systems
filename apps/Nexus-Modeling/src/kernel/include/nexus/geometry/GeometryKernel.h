#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — render upload, skinning bridge, topology utilities
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/animation/AnimationCore.h>
#include <nexus/gfx/Device.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/render/SceneGraph.h>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace nexus::geometry {

enum class VertexSemantic : uint8_t {
    Position,
    Normal,
    Tangent,
    UV0,
    JointIndex4,
    JointWeight4,
};

enum class VertexElementFormat : uint8_t {
    Float2,
    Float3,
    Float4,
    Uint16x4,
};

struct VertexStreamView {
    VertexSemantic semantic = VertexSemantic::Position;
    const void* data = nullptr;
    uint32_t elementCount = 0;
    uint32_t strideBytes = 0;
};

struct MeshSection {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    nexus::render::MaterialID material = nexus::render::kInvalidMaterial;
};

struct MeshUploadView {
    uint32_t vertexCount = 0;
    std::span<const uint32_t> indices{};
    std::span<const MeshSection> sections{};
    std::vector<VertexStreamView> vertexStreams;
};

struct UploadValidationReport {
    bool valid = true;
    std::vector<std::string> errors;
};

struct PackedVertexAttribute {
    VertexSemantic semantic = VertexSemantic::Position;
    VertexElementFormat format = VertexElementFormat::Float3;
    uint32_t location = 0;
    uint32_t binding = 0;
    uint32_t offsetBytes = 0;
};

struct PackedVertexBinding {
    uint32_t binding = 0;
    uint32_t strideBytes = 0;
};

struct PackedVertexLayout {
    std::vector<PackedVertexBinding> bindings;
    std::vector<PackedVertexAttribute> attributes;
};

struct UploadedGeometryMesh {
    nexus::render::MeshRef meshRef{};
    std::vector<MeshSection> sections;
    PackedVertexLayout layout;
};

struct UploadToDeviceReport {
    bool valid = true;
    std::vector<std::string> errors;
};

struct GeometryUploadOptions {
    // When true and device supports RT tier >= 1, upload bridge attempts BLAS build.
    bool buildBlasIfSupported = true;
};

struct GeometryDrawPacket {
    nexus::gfx::BufferHandle vertexBuffer;
    nexus::gfx::BufferHandle indexBuffer;
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    nexus::render::MaterialID material = nexus::render::kInvalidMaterial;
};

class MeshUploadContract {
public:
    // Converts polygon faces to a flat triangle index stream suitable for GPU upload.
    static std::vector<uint32_t> buildTriangulatedIndexBuffer(const Mesh& mesh);

    // Convenience section list that maps the full index range to one material slot.
    static std::vector<MeshSection> makeSingleSection(
        std::span<const uint32_t> indices,
        nexus::render::MaterialID material);

    // Creates an immutable upload view over mesh attributes + supplied index/section ranges.
    static MeshUploadView buildView(const Mesh& mesh,
                                    std::span<const uint32_t> indices,
                                    std::span<const MeshSection> sections);

    // Validates stream counts/strides and section index ranges.
    static UploadValidationReport validateView(const MeshUploadView& view);

    // Builds a deterministic packed layout descriptor for backend vertex-input wiring.
    // Semantics are ordered canonically: Position, Normal, UV0, JointIndex4, JointWeight4.
    static PackedVertexLayout buildPackedVertexLayout(const MeshUploadView& view);

    // Packs all active streams into one tightly interleaved vertex buffer matching layout.
    // Returns empty if layout/view are incompatible.
    static std::vector<uint8_t> packInterleavedVertexBuffer(const MeshUploadView& view,
                                                            const PackedVertexLayout& layout);
};

class GeometryRenderBridge {
public:
    // Creates GPU buffers from upload view + packed layout and populates MeshRef/indexCount.
    // sections are preserved as material slot ranges for render submission.
    static UploadToDeviceReport uploadToDevice(nexus::gfx::IDevice& device,
                                               const MeshUploadView& view,
                                               const PackedVertexLayout& layout,
                                               std::span<const MeshSection> sections,
                                               const GeometryUploadOptions& options,
                                               UploadedGeometryMesh& out);

    static UploadToDeviceReport uploadToDevice(nexus::gfx::IDevice& device,
                                               const MeshUploadView& view,
                                               const PackedVertexLayout& layout,
                                               std::span<const MeshSection> sections,
                                               UploadedGeometryMesh& out)
    {
        return uploadToDevice(device, view, layout, sections, GeometryUploadOptions{}, out);
    }

    // Releases buffers created by uploadToDevice and clears output handles/metadata.
    static void destroyUploadedMesh(nexus::gfx::IDevice& device, UploadedGeometryMesh& mesh) noexcept;

    // Builds one draw packet per material section; if no sections exist, creates one full-range packet.
    static std::vector<GeometryDrawPacket> buildDrawPackets(const UploadedGeometryMesh& mesh);

    // Converts geometry draw packets into SceneGraph node section payload format.
    static std::vector<nexus::render::MeshSectionDrawPacket>
    toSceneSectionDrawPackets(std::span<const GeometryDrawPacket> packets);

    // One-call upload-to-scene handoff: assigns MeshRef and converted section packets.
    static void assignUploadedMeshToNode(const UploadedGeometryMesh& uploaded,
                                         nexus::render::Node& node);

    // Move-based handoff: transfers ownership from uploaded payload into node and clears uploaded.
    static void adoptUploadedMeshByNode(UploadedGeometryMesh& uploaded,
                                        nexus::render::Node& node) noexcept;

    // Destroys node mesh GPU payload handles and clears section packets.
    static void destroyNodeMeshPayload(nexus::gfx::IDevice& device,
                                       nexus::render::Node& node) noexcept;
};

struct JointRemapEntry {
    std::string meshJointName;
    int32_t skeletonJointIndex = -1;
    bool matched = false;
};

struct SkinningValidationReport {
    bool valid = true;
    std::vector<std::string> errors;
    std::vector<JointRemapEntry> jointRemap;
};

class SkinningBridge {
public:
    // Builds name-based remap from mesh joint list to skeleton joint indices.
    static SkinningValidationReport buildJointRemap(std::span<const std::string> meshJointNames,
                                                    const nexus::animation::Skeleton& skeleton);

    // Remaps mesh per-vertex joint indices from mesh-joint space into skeleton-joint space.
    // Returns false if remap fails or mesh contains invalid joint references.
    static bool remapMeshSkinningToSkeleton(Mesh& mesh,
                                            std::span<const std::string> meshJointNames,
                                            const nexus::animation::Skeleton& skeleton,
                                            SkinningValidationReport& outReport);
};

enum class GeometryCommandType : uint8_t {
    Transform,
    AppendMerge,
    SplitFaceRange,
    WeldCoincidentVertices,
};

struct GeometryCommandDesc {
    GeometryCommandType type = GeometryCommandType::Transform;
    nexus::render::Mat4 transform = nexus::render::Mat4::identity();
    const Mesh* mergeMesh = nullptr;
    Mesh* splitOutputMesh = nullptr;
    size_t firstFace = 0;
    size_t faceCount = 0;
    float weldEpsilon = 1e-5f;
};

struct GeometryCommandReport {
    bool valid = true;
    std::vector<std::string> errors;
};

class GeometryCommandSurface {
public:
    // Small command-facing operation surface intended for future scripting bindings.
    // Month 2 starts with transform and weld because both are deterministic mesh ops.
    static GeometryCommandReport execute(Mesh& mesh, const GeometryCommandDesc& command);
};

struct Edge {
    uint32_t v0 = 0;
    uint32_t v1 = 0;

    [[nodiscard]] bool operator==(const Edge& other) const noexcept {
        return v0 == other.v0 && v1 == other.v1;
    }
};

enum class TopologyIssueCode : uint8_t {
    FaceTooSmall,
    IndexOutOfRange,
    DegenerateEdge,
    NonManifoldEdge,
};

struct TopologyValidationIssue {
    TopologyIssueCode code = TopologyIssueCode::FaceTooSmall;
    uint32_t faceIndex = 0;
    uint32_t vertexIndex = 0;
    Edge edge{};
};

struct TopologyValidationReport {
    bool valid = true;
    uint32_t faceCount = 0;
    uint32_t boundaryEdgeCount = 0;
    uint32_t nonManifoldEdgeCount = 0;
    std::vector<TopologyValidationIssue> issues;
};

class TopologyUtilities {
public:
    // Validates basic topology constraints and manifold-adjacent edge usage.
    // Boundary edges (single-face usage) are allowed and reported as metadata.
    static TopologyValidationReport validateTopology(const MeshTopology& topology,
                                                     size_t vertexCount);

    // Returns unique undirected edges sorted by (v0, v1), where v0 < v1.
    static std::vector<Edge> extractEdgeList(const MeshTopology& topology);

    // Extracts boundary loops/chains from edges that are referenced by exactly one face.
    // Closed loops are returned with first vertex repeated at the end.
    static std::vector<std::vector<uint32_t>> extractBoundaryLoops(const MeshTopology& topology);

    // Face-level connected components using shared-undirected-edge adjacency.
    static std::vector<std::vector<uint32_t>> connectedFaceComponents(const MeshTopology& topology);
};

} // namespace nexus::geometry
