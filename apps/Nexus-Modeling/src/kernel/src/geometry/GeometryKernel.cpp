#include <nexus/geometry/GeometryKernel.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <map>
#include <queue>
#include <set>
#include <unordered_map>

namespace nexus::geometry {

namespace {

Edge makeUndirected(uint32_t a, uint32_t b)
{
    if (a < b) {
        return {a, b};
    }
    return {b, a};
}

struct EdgeHasher {
    size_t operator()(const Edge& e) const noexcept
    {
        return (static_cast<size_t>(e.v0) << 32ull) ^ static_cast<size_t>(e.v1);
    }
};

bool edgeLess(const Edge& a, const Edge& b)
{
    if (a.v0 != b.v0) {
        return a.v0 < b.v0;
    }
    return a.v1 < b.v1;
}

struct EdgeLess {
    bool operator()(const Edge& a, const Edge& b) const noexcept
    {
        return edgeLess(a, b);
    }
};

std::vector<Edge> faceEdges(const Face& face)
{
    std::vector<Edge> out;
    if (face.indices.size() < 2) {
        return out;
    }

    out.reserve(face.indices.size());
    for (size_t i = 0; i < face.indices.size(); ++i) {
        const uint32_t a = face.indices[i];
        const uint32_t b = face.indices[(i + 1) % face.indices.size()];
        if (a == b) {
            continue;
        }
        out.push_back(makeUndirected(a, b));
    }
    return out;
}

uint32_t semanticSortKey(VertexSemantic semantic)
{
    switch (semantic) {
    case VertexSemantic::Position: return 0;
    case VertexSemantic::Normal: return 1;
    case VertexSemantic::Tangent: return 2;
    case VertexSemantic::UV0: return 3;
    case VertexSemantic::JointIndex4: return 4;
    case VertexSemantic::JointWeight4: return 5;
    }
    return 255;
}

VertexElementFormat semanticFormat(VertexSemantic semantic)
{
    switch (semantic) {
    case VertexSemantic::Position: return VertexElementFormat::Float3;
    case VertexSemantic::Normal: return VertexElementFormat::Float3;
    case VertexSemantic::Tangent: return VertexElementFormat::Float4;
    case VertexSemantic::UV0: return VertexElementFormat::Float2;
    case VertexSemantic::JointIndex4: return VertexElementFormat::Uint16x4;
    case VertexSemantic::JointWeight4: return VertexElementFormat::Float4;
    }
    return VertexElementFormat::Float3;
}

uint32_t elementSizeBytes(VertexElementFormat format)
{
    switch (format) {
    case VertexElementFormat::Float2: return 8;
    case VertexElementFormat::Float3: return 12;
    case VertexElementFormat::Float4: return 16;
    case VertexElementFormat::Uint16x4: return 8;
    }
    return 0;
}

const VertexStreamView* findStream(const MeshUploadView& view, VertexSemantic semantic)
{
    for (const auto& stream : view.vertexStreams) {
        if (stream.semantic == semantic) {
            return &stream;
        }
    }
    return nullptr;
}

} // namespace

TopologyValidationReport TopologyUtilities::validateTopology(const MeshTopology& topology,
                                                             size_t vertexCount)
{
    TopologyValidationReport report{};
    report.faceCount = static_cast<uint32_t>(topology.faceCount());

    std::map<Edge, uint32_t, EdgeLess> edgeCounts;

    for (uint32_t fi = 0; fi < report.faceCount; ++fi) {
        const Face& face = topology.face(fi);
        if (face.indices.size() < 3) {
            report.valid = false;
            report.issues.push_back({
                .code = TopologyIssueCode::FaceTooSmall,
                .faceIndex = fi,
            });
            continue;
        }

        for (uint32_t idx : face.indices) {
            if (static_cast<size_t>(idx) >= vertexCount) {
                report.valid = false;
                report.issues.push_back({
                    .code = TopologyIssueCode::IndexOutOfRange,
                    .faceIndex = fi,
                    .vertexIndex = idx,
                });
            }
        }

        for (size_t i = 0; i < face.indices.size(); ++i) {
            const uint32_t a = face.indices[i];
            const uint32_t b = face.indices[(i + 1) % face.indices.size()];
            if (a == b) {
                report.valid = false;
                report.issues.push_back({
                    .code = TopologyIssueCode::DegenerateEdge,
                    .faceIndex = fi,
                    .edge = {a, b},
                });
                continue;
            }

            edgeCounts[makeUndirected(a, b)] += 1u;
        }
    }

    for (const auto& [edge, count] : edgeCounts) {
        if (count == 1u) {
            report.boundaryEdgeCount += 1u;
            continue;
        }
        if (count > 2u) {
            report.valid = false;
            report.nonManifoldEdgeCount += 1u;
            report.issues.push_back({
                .code = TopologyIssueCode::NonManifoldEdge,
                .edge = edge,
            });
        }
    }

    return report;
}

std::vector<uint32_t> MeshUploadContract::buildTriangulatedIndexBuffer(const Mesh& mesh)
{
    std::vector<uint32_t> indices;

    const MeshTopology& topology = mesh.topology();
    for (size_t fi = 0; fi < topology.faceCount(); ++fi) {
        const Face& face = topology.face(fi);
        if (face.indices.size() < 3) {
            continue;
        }

        if (face.indices.size() == 3) {
            indices.push_back(face.indices[0]);
            indices.push_back(face.indices[1]);
            indices.push_back(face.indices[2]);
            continue;
        }

        for (size_t i = 1; i + 1 < face.indices.size(); ++i) {
            indices.push_back(face.indices[0]);
            indices.push_back(face.indices[i]);
            indices.push_back(face.indices[i + 1]);
        }
    }

    return indices;
}

std::vector<MeshSection> MeshUploadContract::makeSingleSection(
    std::span<const uint32_t> indices,
    nexus::render::MaterialID material)
{
    std::vector<MeshSection> sections;
    if (indices.empty()) {
        return sections;
    }

    sections.push_back({
        .firstIndex = 0,
        .indexCount = static_cast<uint32_t>(indices.size()),
        .material = material,
    });
    return sections;
}

MeshUploadView MeshUploadContract::buildView(const Mesh& mesh,
                                             std::span<const uint32_t> indices,
                                             std::span<const MeshSection> sections)
{
    MeshUploadView view{};
    view.vertexCount = static_cast<uint32_t>(mesh.attributes().vertexCount());
    view.indices = indices;
    view.sections = sections;

    view.vertexStreams.push_back({
        .semantic = VertexSemantic::Position,
        .data = mesh.attributes().positions().data(),
        .elementCount = static_cast<uint32_t>(mesh.attributes().positions().size()),
        .strideBytes = static_cast<uint32_t>(sizeof(nexus::render::Vec3)),
    });

    if (mesh.attributes().hasNormals()) {
        view.vertexStreams.push_back({
            .semantic = VertexSemantic::Normal,
            .data = mesh.attributes().normals().data(),
            .elementCount = static_cast<uint32_t>(mesh.attributes().normals().size()),
            .strideBytes = static_cast<uint32_t>(sizeof(nexus::render::Vec3)),
        });
    }

    if (mesh.attributes().hasTangents()) {
        view.vertexStreams.push_back({
            .semantic = VertexSemantic::Tangent,
            .data = mesh.attributes().tangents().data(),
            .elementCount = static_cast<uint32_t>(mesh.attributes().tangents().size()),
            .strideBytes = static_cast<uint32_t>(sizeof(Vec4)),
        });
    }

    if (mesh.attributes().hasUVs()) {
        view.vertexStreams.push_back({
            .semantic = VertexSemantic::UV0,
            .data = mesh.attributes().uvs().data(),
            .elementCount = static_cast<uint32_t>(mesh.attributes().uvs().size()),
            .strideBytes = static_cast<uint32_t>(sizeof(Vec2)),
        });
    }

    if (mesh.attributes().hasSkinning()) {
        view.vertexStreams.push_back({
            .semantic = VertexSemantic::JointIndex4,
            .data = mesh.attributes().jointIndices().data(),
            .elementCount = static_cast<uint32_t>(mesh.attributes().jointIndices().size()),
            .strideBytes = static_cast<uint32_t>(sizeof(JointIndex4)),
        });
        view.vertexStreams.push_back({
            .semantic = VertexSemantic::JointWeight4,
            .data = mesh.attributes().jointWeights().data(),
            .elementCount = static_cast<uint32_t>(mesh.attributes().jointWeights().size()),
            .strideBytes = static_cast<uint32_t>(sizeof(JointWeight4)),
        });
    }

    return view;
}

UploadValidationReport MeshUploadContract::validateView(const MeshUploadView& view)
{
    UploadValidationReport report{};

    if (view.vertexCount == 0) {
        report.valid = false;
        report.errors.push_back("upload view has zero vertices");
    }

    if (view.indices.size() % 3 != 0) {
        report.valid = false;
        report.errors.push_back("index stream must contain triangle-list indices (multiple of 3)");
    }

    bool hasPosition = false;
    std::set<VertexSemantic> seenSemantics;
    for (const auto& stream : view.vertexStreams) {
        if (stream.data == nullptr) {
            report.valid = false;
            report.errors.push_back("vertex stream has null data pointer");
            continue;
        }
        if (stream.elementCount != view.vertexCount) {
            report.valid = false;
            report.errors.push_back("vertex stream element count mismatch");
        }
        if (stream.strideBytes == 0) {
            report.valid = false;
            report.errors.push_back("vertex stream stride must be non-zero");
        }
        if (stream.semantic == VertexSemantic::Position) {
            hasPosition = true;
        }
        if (!seenSemantics.insert(stream.semantic).second) {
            report.valid = false;
            report.errors.push_back("duplicate vertex semantic in upload view");
        }
    }

    if (!hasPosition) {
        report.valid = false;
        report.errors.push_back("position stream is required");
    }

    const bool hasJointIndices = seenSemantics.count(VertexSemantic::JointIndex4) > 0;
    const bool hasJointWeights = seenSemantics.count(VertexSemantic::JointWeight4) > 0;
    if (hasJointIndices != hasJointWeights) {
        report.valid = false;
        report.errors.push_back("joint index and weight streams must be provided together");
    }

    const bool hasNormals = seenSemantics.count(VertexSemantic::Normal) > 0;
    const bool hasTangents = seenSemantics.count(VertexSemantic::Tangent) > 0;
    const bool hasUV0 = seenSemantics.count(VertexSemantic::UV0) > 0;
    if (hasTangents && !hasNormals) {
        report.valid = false;
        report.errors.push_back("tangent stream requires normal stream");
    }
    if (hasTangents && !hasUV0) {
        report.valid = false;
        report.errors.push_back("tangent stream requires uv0 stream");
    }

    for (const auto& section : view.sections) {
        if (section.indexCount == 0) {
            report.valid = false;
            report.errors.push_back("section indexCount must be non-zero");
            continue;
        }
        const uint64_t begin = section.firstIndex;
        const uint64_t end = static_cast<uint64_t>(section.firstIndex) + section.indexCount;
        if (begin >= view.indices.size() || end > view.indices.size()) {
            report.valid = false;
            report.errors.push_back("section range exceeds index buffer");
        }
    }

    return report;
}

PackedVertexLayout MeshUploadContract::buildPackedVertexLayout(const MeshUploadView& view)
{
    PackedVertexLayout layout{};
    if (view.vertexCount == 0) {
        return layout;
    }

    std::vector<VertexSemantic> semantics;
    semantics.reserve(view.vertexStreams.size());
    for (const auto& stream : view.vertexStreams) {
        semantics.push_back(stream.semantic);
    }

    std::sort(semantics.begin(), semantics.end(),
              [](VertexSemantic a, VertexSemantic b) {
                  return semanticSortKey(a) < semanticSortKey(b);
              });
    semantics.erase(std::unique(semantics.begin(), semantics.end()), semantics.end());

    uint32_t offset = 0;
    uint32_t location = 0;
    for (const VertexSemantic semantic : semantics) {
        const VertexElementFormat format = semanticFormat(semantic);
        layout.attributes.push_back({
            .semantic = semantic,
            .format = format,
            .location = location++,
            .binding = 0,
            .offsetBytes = offset,
        });
        offset += elementSizeBytes(format);
    }

    if (!layout.attributes.empty()) {
        layout.bindings.push_back({
            .binding = 0,
            .strideBytes = offset,
        });
    }

    return layout;
}

GpuVertexInputLayout MeshUploadContract::toGpuVertexInputLayout(const PackedVertexLayout& layout)
{
    auto toGfxFormat = [](VertexElementFormat f) -> nexus::gfx::Format {
        switch (f) {
        case VertexElementFormat::Float2:   return nexus::gfx::Format::R32G32_Float;
        case VertexElementFormat::Float3:   return nexus::gfx::Format::R32G32B32_Float;
        case VertexElementFormat::Float4:   return nexus::gfx::Format::R32G32B32A32_Float;
        case VertexElementFormat::Uint16x4: return nexus::gfx::Format::R16G16B16A16_Uint;
        }
        return nexus::gfx::Format::R32G32B32_Float;
    };

    GpuVertexInputLayout out;
    out.bindings.reserve(layout.bindings.size());
    for (const auto& b : layout.bindings) {
        out.bindings.push_back({ b.binding, b.strideBytes, nexus::gfx::VertexInputRate::Vertex });
    }
    out.attributes.reserve(layout.attributes.size());
    for (const auto& a : layout.attributes) {
        out.attributes.push_back({ a.location, a.binding, toGfxFormat(a.format), a.offsetBytes });
    }
    return out;
}

std::vector<uint8_t> MeshUploadContract::packInterleavedVertexBuffer(
    const MeshUploadView& view,
    const PackedVertexLayout& layout)
{
    if (view.vertexCount == 0 || layout.bindings.empty() || layout.attributes.empty()) {
        return {};
    }

    const uint32_t stride = layout.bindings[0].strideBytes;
    if (stride == 0) {
        return {};
    }

    std::vector<uint8_t> packed;
    packed.resize(static_cast<size_t>(view.vertexCount) * stride, 0u);

    for (const auto& attribute : layout.attributes) {
        const VertexStreamView* stream = findStream(view, attribute.semantic);
        if (!stream || !stream->data || stream->elementCount != view.vertexCount || stream->strideBytes == 0) {
            return {};
        }

        const uint32_t requiredSize = elementSizeBytes(attribute.format);
        if (stream->strideBytes < requiredSize) {
            return {};
        }

        const auto* srcBytes = static_cast<const uint8_t*>(stream->data);
        for (uint32_t v = 0; v < view.vertexCount; ++v) {
            uint8_t* dst = packed.data() + static_cast<size_t>(v) * stride + attribute.offsetBytes;
            const uint8_t* src = srcBytes + static_cast<size_t>(v) * stream->strideBytes;
            std::memcpy(dst, src, requiredSize);
        }
    }

    return packed;
}

nexus::render::Vec3 MeshUploadContract::reconstructBitangent(const nexus::render::Vec3& normal,
                                                             const Vec4& tangent) noexcept
{
    const nexus::render::Vec3 t = {tangent.x, tangent.y, tangent.z};
    const nexus::render::Vec3 bitangent = {
        normal.y * t.z - normal.z * t.y,
        normal.z * t.x - normal.x * t.z,
        normal.x * t.y - normal.y * t.x,
    };

    const float lenSq = bitangent.x * bitangent.x + bitangent.y * bitangent.y + bitangent.z * bitangent.z;
    if (lenSq < 1e-12f) {
        return {0.f, 0.f, 0.f};
    }

    const float invLen = 1.f / std::sqrt(lenSq);
    const float sign = tangent.w < 0.f ? -1.f : 1.f;
    return {
        bitangent.x * invLen * sign,
        bitangent.y * invLen * sign,
        bitangent.z * invLen * sign,
    };
}

UploadToDeviceReport GeometryRenderBridge::uploadToDevice(nexus::gfx::IDevice& device,
                                                          const MeshUploadView& view,
                                                          const PackedVertexLayout& layout,
                                                          std::span<const MeshSection> sections,
                                                          const GeometryUploadOptions& options,
                                                          UploadedGeometryMesh& out)
{
    UploadToDeviceReport report{};

    const UploadValidationReport viewReport = MeshUploadContract::validateView(view);
    if (!viewReport.valid) {
        report.valid = false;
        report.errors = viewReport.errors;
        return report;
    }

    if (layout.bindings.empty() || layout.attributes.empty()) {
        report.valid = false;
        report.errors.push_back("packed vertex layout must not be empty");
        return report;
    }

    const std::vector<uint8_t> interleaved = MeshUploadContract::packInterleavedVertexBuffer(view, layout);
    if (interleaved.empty()) {
        report.valid = false;
        report.errors.push_back("failed to pack interleaved vertex buffer");
        return report;
    }

    const uint64_t vertexBytes = static_cast<uint64_t>(interleaved.size());
    const uint64_t indexBytes = static_cast<uint64_t>(view.indices.size()) * sizeof(uint32_t);
    if (vertexBytes == 0 || indexBytes == 0) {
        report.valid = false;
        report.errors.push_back("vertex/index buffer byte size must be non-zero");
        return report;
    }

    const auto vbo = device.createBuffer({
        .sizeBytes = vertexBytes,
        .usage = nexus::gfx::BufferUsage::VertexBuffer | nexus::gfx::BufferUsage::TransferDst,
        .memory = nexus::gfx::MemoryHint::GpuOnly,
        .debugName = "geometry.upload.vertex",
    });
    if (!vbo.valid()) {
        report.valid = false;
        report.errors.push_back("failed to create vertex buffer");
        return report;
    }

    const auto ibo = device.createBuffer({
        .sizeBytes = indexBytes,
        .usage = nexus::gfx::BufferUsage::IndexBuffer | nexus::gfx::BufferUsage::TransferDst,
        .memory = nexus::gfx::MemoryHint::GpuOnly,
        .debugName = "geometry.upload.index",
    });
    if (!ibo.valid()) {
        device.destroyBuffer(vbo);
        report.valid = false;
        report.errors.push_back("failed to create index buffer");
        return report;
    }

    device.uploadBuffer(vbo, interleaved.data(), vertexBytes, 0);
    device.uploadBuffer(ibo, view.indices.data(), indexBytes, 0);

    out.meshRef.vertexBuffer = vbo;
    out.meshRef.indexBuffer = ibo;
    out.meshRef.indexCount = static_cast<uint32_t>(view.indices.size());
    out.sections.assign(sections.begin(), sections.end());
    out.layout = layout;

    if (options.buildBlasIfSupported && device.caps().rayTracingTier >= 1) {
        out.meshRef.blas = device.buildBLAS(vbo,
                                            ibo,
                                            view.vertexCount,
                                            static_cast<uint32_t>(view.indices.size()));
        if (!out.meshRef.blas.valid()) {
            report.valid = false;
            report.errors.push_back("failed to build BLAS for uploaded geometry");
        }
    }

    return report;
}

void GeometryRenderBridge::destroyUploadedMesh(nexus::gfx::IDevice& device, UploadedGeometryMesh& mesh) noexcept
{
    if (mesh.meshRef.blas.valid()) {
        device.destroyAccelStruct(mesh.meshRef.blas);
    }
    if (mesh.meshRef.vertexBuffer.valid()) {
        device.destroyBuffer(mesh.meshRef.vertexBuffer);
    }
    if (mesh.meshRef.indexBuffer.valid()) {
        device.destroyBuffer(mesh.meshRef.indexBuffer);
    }

    mesh.meshRef = {};
    mesh.sections.clear();
    mesh.layout = {};
}

std::vector<GeometryDrawPacket> GeometryRenderBridge::buildDrawPackets(const UploadedGeometryMesh& mesh)
{
    std::vector<GeometryDrawPacket> packets;

    if (!mesh.meshRef.vertexBuffer.valid() || !mesh.meshRef.indexBuffer.valid() || mesh.meshRef.indexCount == 0) {
        return packets;
    }

    if (mesh.sections.empty()) {
        packets.push_back({
            .vertexBuffer = mesh.meshRef.vertexBuffer,
            .indexBuffer = mesh.meshRef.indexBuffer,
            .firstIndex = 0,
            .indexCount = mesh.meshRef.indexCount,
            .material = nexus::render::kInvalidMaterial,
        });
        return packets;
    }

    for (const auto& section : mesh.sections) {
        if (section.indexCount == 0) {
            continue;
        }

        const uint64_t begin = section.firstIndex;
        const uint64_t end = static_cast<uint64_t>(section.firstIndex) + section.indexCount;
        if (begin >= mesh.meshRef.indexCount || end > mesh.meshRef.indexCount) {
            continue;
        }

        packets.push_back({
            .vertexBuffer = mesh.meshRef.vertexBuffer,
            .indexBuffer = mesh.meshRef.indexBuffer,
            .firstIndex = section.firstIndex,
            .indexCount = section.indexCount,
            .material = section.material,
        });
    }

    return packets;
}

std::vector<nexus::render::MeshSectionDrawPacket>
GeometryRenderBridge::toSceneSectionDrawPackets(std::span<const GeometryDrawPacket> packets)
{
    std::vector<nexus::render::MeshSectionDrawPacket> out;
    out.reserve(packets.size());

    for (const auto& p : packets) {
        if (p.indexCount == 0) {
            continue;
        }
        out.push_back({
            .firstIndex = p.firstIndex,
            .indexCount = p.indexCount,
            .material = p.material,
        });
    }

    return out;
}

void GeometryRenderBridge::assignUploadedMeshToNode(const UploadedGeometryMesh& uploaded,
                                                    nexus::render::Node& node)
{
    node.mesh = uploaded.meshRef;
    const auto packets = buildDrawPackets(uploaded);
    node.sectionDrawPackets = toSceneSectionDrawPackets(packets);
}

void GeometryRenderBridge::adoptUploadedMeshByNode(UploadedGeometryMesh& uploaded,
                                                   nexus::render::Node& node) noexcept
{
    node.mesh = uploaded.meshRef;
    const auto packets = buildDrawPackets(uploaded);
    node.sectionDrawPackets = toSceneSectionDrawPackets(packets);

    uploaded.meshRef = {};
    uploaded.sections.clear();
    uploaded.layout = {};
}

void GeometryRenderBridge::destroyNodeMeshPayload(nexus::gfx::IDevice& device,
                                                  nexus::render::Node& node) noexcept
{
    if (node.mesh.blas.valid()) {
        device.destroyAccelStruct(node.mesh.blas);
    }
    if (node.mesh.vertexBuffer.valid()) {
        device.destroyBuffer(node.mesh.vertexBuffer);
    }
    if (node.mesh.indexBuffer.valid()) {
        device.destroyBuffer(node.mesh.indexBuffer);
    }

    node.mesh = {};
    node.sectionDrawPackets.clear();
}

SkinningValidationReport SkinningBridge::buildJointRemap(
    std::span<const std::string> meshJointNames,
    const nexus::animation::Skeleton& skeleton)
{
    SkinningValidationReport report{};
    report.jointRemap.reserve(meshJointNames.size());

    std::unordered_map<std::string, int32_t> skeletonByName;
    for (size_t i = 0; i < skeleton.boneCount(); ++i) {
        skeletonByName.emplace(skeleton.boneName(i), static_cast<int32_t>(i));
    }

    for (const std::string& meshJointName : meshJointNames) {
        JointRemapEntry entry{};
        entry.meshJointName = meshJointName;

        const auto it = skeletonByName.find(meshJointName);
        if (it == skeletonByName.end()) {
            report.valid = false;
            report.errors.push_back("mesh joint not found in skeleton: " + meshJointName);
        } else {
            entry.skeletonJointIndex = it->second;
            entry.matched = true;
        }

        report.jointRemap.push_back(std::move(entry));
    }

    return report;
}

bool SkinningBridge::remapMeshSkinningToSkeleton(
    Mesh& mesh,
    std::span<const std::string> meshJointNames,
    const nexus::animation::Skeleton& skeleton,
    SkinningValidationReport& outReport)
{
    outReport = buildJointRemap(meshJointNames, skeleton);

    if (!mesh.attributes().hasSkinning()) {
        outReport.valid = false;
        outReport.errors.push_back("mesh has no skinning streams");
        return false;
    }

    auto remappedIndices = mesh.attributes().jointIndices();
    const auto jointWeights = mesh.attributes().jointWeights();

    for (size_t v = 0; v < remappedIndices.size(); ++v) {
        for (uint32_t i = 0; i < kMaxSkinInfluencesPerVertex; ++i) {
            const uint16_t meshSpaceIndex = remappedIndices[v][i];
            if (static_cast<size_t>(meshSpaceIndex) >= meshJointNames.size()) {
                outReport.valid = false;
                outReport.errors.push_back("mesh skinning joint index out of mesh-joint-name range");
                continue;
            }

            const int32_t skeletonIndex = outReport.jointRemap[meshSpaceIndex].skeletonJointIndex;
            if (skeletonIndex < 0) {
                outReport.valid = false;
                outReport.errors.push_back("mesh joint could not be mapped to skeleton");
                continue;
            }

            remappedIndices[v][i] = static_cast<uint16_t>(skeletonIndex);
        }
    }

    if (!outReport.valid) {
        return false;
    }

    mesh.attributes().setSkinning(std::move(remappedIndices), jointWeights);
    mesh.attributes().normalizeSkinningWeights();
    return mesh.isSkinnableForSkeleton(skeleton.boneCount());
}

GeometryCommandReport GeometryCommandSurface::execute(Mesh& mesh, const GeometryCommandDesc& command)
{
    GeometryCommandReport report{};

    switch (command.type) {
    case GeometryCommandType::Transform:
        if (!mesh.applyTransform(command.transform)) {
            report.valid = false;
            report.errors.push_back("transform command failed");
        }
        break;
    case GeometryCommandType::AppendMerge:
        if (command.mergeMesh == nullptr) {
            report.valid = false;
            report.errors.push_back("append-merge command requires merge mesh");
            break;
        }
        if (!mesh.appendMesh(*command.mergeMesh)) {
            report.valid = false;
            report.errors.push_back("append-merge command failed");
        }
        break;
    case GeometryCommandType::SplitFaceRange:
        if (command.splitOutputMesh == nullptr) {
            report.valid = false;
            report.errors.push_back("split command requires output mesh");
            break;
        }
        if (!mesh.splitFaceRange(command.firstFace, command.faceCount, *command.splitOutputMesh)) {
            report.valid = false;
            report.errors.push_back("split command failed");
        }
        break;
    case GeometryCommandType::WeldCoincidentVertices:
        if (command.weldEpsilon < 0.f) {
            report.valid = false;
            report.errors.push_back("weld epsilon must be non-negative");
            break;
        }
        if (!mesh.weldCoincidentVertices(command.weldEpsilon)) {
            report.valid = false;
            report.errors.push_back("weld command failed");
        }
        break;
    }

    return report;
}

std::vector<Edge> TopologyUtilities::extractEdgeList(const MeshTopology& topology)
{
    std::unordered_map<Edge, uint32_t, EdgeHasher> edgeCounts;

    for (size_t fi = 0; fi < topology.faceCount(); ++fi) {
        const auto edges = faceEdges(topology.face(fi));
        for (const auto& e : edges) {
            edgeCounts[e] += 1;
        }
    }

    std::vector<Edge> edges;
    edges.reserve(edgeCounts.size());
    for (const auto& [e, _] : edgeCounts) {
        edges.push_back(e);
    }

    std::sort(edges.begin(), edges.end(), edgeLess);
    return edges;
}

std::vector<std::vector<uint32_t>> TopologyUtilities::extractBoundaryLoops(const MeshTopology& topology)
{
    std::unordered_map<Edge, uint32_t, EdgeHasher> edgeCounts;

    for (size_t fi = 0; fi < topology.faceCount(); ++fi) {
        const auto edges = faceEdges(topology.face(fi));
        for (const auto& e : edges) {
            edgeCounts[e] += 1;
        }
    }

    std::unordered_map<uint32_t, std::vector<uint32_t>> boundaryAdj;
    for (const auto& [e, count] : edgeCounts) {
        if (count == 1) {
            boundaryAdj[e.v0].push_back(e.v1);
            boundaryAdj[e.v1].push_back(e.v0);
        }
    }

    std::vector<Edge> boundaryEdges;
    boundaryEdges.reserve(edgeCounts.size());
    for (const auto& [e, count] : edgeCounts) {
        if (count == 1) {
            boundaryEdges.push_back(e);
        }
    }

    std::sort(boundaryEdges.begin(), boundaryEdges.end(), edgeLess);

    std::unordered_map<Edge, bool, EdgeHasher> usedUndirected;
    std::vector<std::vector<uint32_t>> loops;

    for (const Edge& seed : boundaryEdges) {
        if (usedUndirected[seed]) {
            continue;
        }

        std::vector<uint32_t> loop;
        uint32_t start = seed.v0;
        uint32_t prev = seed.v0;
        uint32_t cur = seed.v1;

        usedUndirected[seed] = true;
        loop.push_back(start);
        loop.push_back(cur);

        while (true) {
            if (cur == start) {
                break;
            }

            const auto it = boundaryAdj.find(cur);
            if (it == boundaryAdj.end() || it->second.empty()) {
                break;
            }

            uint32_t next = UINT32_MAX;
            for (uint32_t n : it->second) {
                if (n == prev) {
                    continue;
                }
                Edge candidate = makeUndirected(cur, n);
                if (!usedUndirected[candidate]) {
                    next = n;
                    break;
                }
            }

            if (next == UINT32_MAX) {
                if (cur != start) {
                    Edge closeEdge = makeUndirected(cur, start);
                    if (!usedUndirected[closeEdge]) {
                        usedUndirected[closeEdge] = true;
                        loop.push_back(start);
                    }
                }
                break;
            }

            Edge chosen = makeUndirected(cur, next);
            usedUndirected[chosen] = true;

            prev = cur;
            cur = next;
            loop.push_back(cur);
        }

        if (loop.size() >= 2) {
            loops.push_back(std::move(loop));
        }
    }

    return loops;
}

std::vector<std::vector<uint32_t>> TopologyUtilities::connectedFaceComponents(const MeshTopology& topology)
{
    const size_t nFaces = topology.faceCount();
    std::vector<std::vector<uint32_t>> components;
    if (nFaces == 0) {
        return components;
    }

    std::unordered_map<Edge, std::vector<uint32_t>, EdgeHasher> edgeToFaces;
    for (size_t fi = 0; fi < nFaces; ++fi) {
        for (const auto& e : faceEdges(topology.face(fi))) {
            edgeToFaces[e].push_back(static_cast<uint32_t>(fi));
        }
    }

    std::vector<std::vector<uint32_t>> adjacency(nFaces);
    for (const auto& [_, faces] : edgeToFaces) {
        for (size_t i = 0; i < faces.size(); ++i) {
            for (size_t j = i + 1; j < faces.size(); ++j) {
                adjacency[faces[i]].push_back(faces[j]);
                adjacency[faces[j]].push_back(faces[i]);
            }
        }
    }

    std::vector<bool> visited(nFaces, false);
    for (uint32_t seed = 0; seed < nFaces; ++seed) {
        if (visited[seed]) {
            continue;
        }

        std::vector<uint32_t> comp;
        std::queue<uint32_t> q;
        q.push(seed);
        visited[seed] = true;

        while (!q.empty()) {
            const uint32_t f = q.front();
            q.pop();
            comp.push_back(f);

            for (uint32_t n : adjacency[f]) {
                if (!visited[n]) {
                    visited[n] = true;
                    q.push(n);
                }
            }
        }

        components.push_back(std::move(comp));
    }

    return components;
}

} // namespace nexus::geometry
