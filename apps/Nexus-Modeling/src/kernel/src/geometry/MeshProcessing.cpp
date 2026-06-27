#include <nexus/geometry/MeshProcessing.h>

#include <nexus/geometry/BooleanOperation.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/MeshTransform.h>
#include <nexus/geometry/MeshVertexWeld.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <queue>
#include <unordered_set>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;
using Mat4 = nexus::render::Mat4;

namespace {

Vec3 faceNormal(const Mesh& mesh, uint32_t fi) {
    const auto& f = mesh.topology().face(fi);
    if (f.vertexCount() < 3) return {0, 0, 1};
    const auto& pos = mesh.attributes().positions();
    if (!f.indicesInBounds(pos.size())) return {0, 0, 1};
    Vec3 a = pos[f.indices[0]], b = pos[f.indices[1]], c = pos[f.indices[2]];
    Vec3 n = (b - a).cross(c - a);
    float len = n.length();
    return (len > 1e-10f) ? n * (1.f / len) : Vec3{0, 0, 1};
}

} // namespace

// ── Segment ────────────────────────────────────────────────────────────

std::vector<Segment> segmentMesh(const Mesh& mesh, float angleThresholdDeg)
{
    std::vector<Segment> segments;
    uint32_t fc = mesh.topology().faceCount();
    if (fc == 0) return segments;

    std::vector<bool> visited(fc, false);
    float cosThreshold = std::cos(angleThresholdDeg * static_cast<float>(std::numbers::pi) / 180.f);

    // Build face adjacency: faces share an edge → they're neighbors.
    std::unordered_map<uint64_t, uint32_t> edgeToFace;
    auto edgeHash = [](uint32_t a, uint32_t b) -> uint64_t {
        if (a > b) std::swap(a, b);
        return (static_cast<uint64_t>(a) << 32) | b;
    };

    std::vector<std::vector<uint32_t>> neighbors(fc);
    for (uint32_t fi = 0; fi < fc; ++fi) {
        const auto& f = mesh.topology().face(fi);
        for (size_t i = 0; i < f.vertexCount(); ++i) {
            uint32_t a = f.indices[i];
            uint32_t b = f.indices[(i+1) % f.vertexCount()];
            uint64_t h = edgeHash(a, b);
            auto it = edgeToFace.find(h);
            if (it != edgeToFace.end()) {
                uint32_t other = it->second;
                neighbors[fi].push_back(other);
                neighbors[other].push_back(fi);
            } else {
                edgeToFace[h] = fi;
            }
        }
    }

    for (uint32_t start = 0; start < fc; ++start) {
        if (visited[start]) continue;

        Segment seg;
        Vec3 avgNormal = faceNormal(mesh, start);
        uint32_t normalCount = 1;

        std::queue<uint32_t> queue;
        queue.push(start);
        visited[start] = true;

        while (!queue.empty()) {
            uint32_t fi = queue.front(); queue.pop();
            seg.faceIndices.push_back(fi);

            for (uint32_t nb : neighbors[fi]) {
                if (visited[nb]) continue;
                Vec3 nbNormal = faceNormal(mesh, nb);
                // Compare against both the seed normal and the running average.
                Vec3 seedNorm = faceNormal(mesh, start);
                float dotSeed = seedNorm.dot(nbNormal);
                float dotAvg = avgNormal.dot(nbNormal);
                if (dotSeed >= cosThreshold || dotAvg >= cosThreshold) {
                    visited[nb] = true;
                    queue.push(nb);
                    avgNormal = avgNormal + nbNormal;
                    normalCount++;
                }
            }
        }
        if (normalCount > 1) avgNormal = avgNormal * (1.f / static_cast<float>(normalCount));

        segments.push_back(std::move(seg));
    }
    return segments;
}

// ── Denoise ────────────────────────────────────────────────────────────────

Mesh denoiseMeshBilateral(const Mesh& mesh, float sigmaC, float /*sigmaS*/,
                           uint32_t iterations)
{
    // Simplified bilateral: Laplacian smooth with face-area weighting.
    Mesh result = mesh;
    auto heOpt = HalfEdgeMesh::fromMesh(mesh);
    if (!heOpt) return mesh;

    HalfEdgeMesh heMesh = *heOpt;
    for (uint32_t iter = 0; iter < iterations; ++iter) {
        auto& pos = heMesh.positions();
        std::vector<Vec3> newPos = pos;

        for (uint32_t v = 0; v < heMesh.vertexCount(); ++v) {
            auto neighbors = heMesh.vertexNeighbors(v);
            if (neighbors.empty()) continue;

            Vec3 avg{0, 0, 0};
            float totalW = 0.f;
            for (uint32_t nb : neighbors) {
                Vec3 diff = pos[nb] - pos[v];
                float dist = diff.length();
                float wC = std::exp(-0.5f * dist * dist / (sigmaC * sigmaC + 1e-10f));
                avg = avg + pos[nb] * wC;
                totalW += wC;
            }
            if (totalW > 1e-10f)
                newPos[v] = pos[v] + (avg * (1.f / totalW) - pos[v]) * 0.5f;
        }
        pos = std::move(newPos);
    }
    return heMesh.toMesh();
}

// ── Hole ────────────────────────────────────────────────────────────────────

Mesh createHole(const Mesh& solid, const Vec3& position, const Vec3& direction,
                 float radius, float depth)
{
    if (!solid.topology().hasValidIndices(solid.attributes().vertexCount()))
        return solid;

    // Build a cylinder mesh at the given position/direction.
    Vec3 n = direction.normalize();
    Vec3 u, v;
    if (std::abs(n.x) < 0.9f) {
        u = Vec3{1, 0, 0}.cross(n).normalize();
    } else {
        u = Vec3{0, 1, 0}.cross(n).normalize();
    }
    v = n.cross(u).normalize();

    const uint32_t circSamples = 16;
    const uint32_t heightSamples = 4;
    std::vector<Vec3> verts;

    // Bottom and top center points.
    Vec3 bottomCenter = position - n * (depth * 0.5f);
    Vec3 topCenter    = position + n * (depth * 0.5f);

    // Side vertices.
    for (uint32_t h = 0; h < heightSamples; ++h) {
        float t = static_cast<float>(h) / static_cast<float>(heightSamples - 1);
        Vec3 center = bottomCenter + (topCenter - bottomCenter) * t;
        for (uint32_t c = 0; c < circSamples; ++c) {
            float angle = 2.f * static_cast<float>(std::numbers::pi) *
                static_cast<float>(c) / static_cast<float>(circSamples);
            verts.push_back(center + (u * std::cos(angle) + v * std::sin(angle)) * radius);
        }
    }

    // Bottom center, then top center.
    uint32_t bcIdx = static_cast<uint32_t>(verts.size());
    verts.push_back(bottomCenter);
    uint32_t tcIdx = static_cast<uint32_t>(verts.size());
    verts.push_back(topCenter);

    Mesh cylinder;
    cylinder.attributes().setPositions(verts);
    auto& topo = cylinder.topology();

    // Side quads → triangles.
    for (uint32_t h = 0; h + 1 < heightSamples; ++h) {
        for (uint32_t c = 0; c < circSamples; ++c) {
            uint32_t v00 = h * circSamples + c;
            uint32_t v01 = (h + 1) * circSamples + c;
            uint32_t v11 = (h + 1) * circSamples + (c + 1) % circSamples;
            uint32_t v10 = h * circSamples + (c + 1) % circSamples;
            topo.addFace(Face{{v00, v01, v11}});
            topo.addFace(Face{{v00, v11, v10}});
        }
    }

    // Bottom cap.
    for (uint32_t c = 0; c < circSamples; ++c) {
        uint32_t a = c;
        uint32_t b = (c + 1) % circSamples;
        topo.addFace(Face{{bcIdx, b, a}});
    }

    // Top cap.
    for (uint32_t c = 0; c < circSamples; ++c) {
        uint32_t a = (heightSamples - 1) * circSamples + c;
        uint32_t b = (heightSamples - 1) * circSamples + (c + 1) % circSamples;
        topo.addFace(Face{{tcIdx, a, b}});
    }

    // Boolean difference: solid ∖ cylinder.
    BooleanOperationOptions opts;
    opts.attemptRepair = true;
    Mesh output;
    if (!BooleanOperation::compute(solid, cylinder, BooleanOperationType::Difference,
                                opts, output).valid) {
        output = solid;
    }
    return output;
}

// ── Pattern ────────────────────────────────────────────────────────────────

Mesh createRectangularArray(const Mesh& instance, uint32_t countX, uint32_t countY,
                              float spacingX, float spacingY)
{
    Mesh result;
    for (uint32_t y = 0; y < countY; ++y) {
        for (uint32_t x = 0; x < countX; ++x) {
            Mat4 mat = Mat4::identity();
            mat.m[0][3] = static_cast<float>(x) * spacingX;
            mat.m[1][3] = static_cast<float>(y) * spacingY;
            Mesh transformed = MeshTransform::apply(instance, mat);
            (void)result.appendMesh(transformed);
        }
    }
    return result;
}

Mesh createCircularArray(const Mesh& instance, const Vec3& center,
                          const Vec3& axis, uint32_t count, float radius)
{
    Mesh result;
    Vec3 n = axis.normalize();
    Vec3 u, v;
    if (std::abs(n.x) < 0.9f) {
        u = Vec3{1, 0, 0}.cross(n).normalize();
    } else {
        u = Vec3{0, 1, 0}.cross(n).normalize();
    }
    v = n.cross(u).normalize();

    for (uint32_t i = 0; i < count; ++i) {
        float angle = 2.f * static_cast<float>(std::numbers::pi) * static_cast<float>(i) / static_cast<float>(count);
        Vec3 offset = center + (u * std::cos(angle) + v * std::sin(angle)) * radius;

        Mat4 mat = Mat4::identity();
        mat.m[0][3] = offset.x;
        mat.m[1][3] = offset.y;
        mat.m[2][3] = offset.z;
        Mesh transformed = MeshTransform::apply(instance, mat);
        if (!result.appendMesh(transformed)) continue;
    }
    return result;
}

// ── Developable ──────────────────────────────────────────────────────────

bool isMeshDevelopable(const Mesh& mesh, float epsilon)
{
    if (!mesh.topology().hasValidIndices(mesh.attributes().vertexCount()))
        return false;

    // Approximate Gaussian curvature at each vertex by angle deficit.
    const auto& pos = mesh.attributes().positions();

    for (uint32_t v = 0; v < mesh.attributes().vertexCount(); ++v) {
        double angleSum = 0.0;
        for (uint32_t fi = 0; fi < mesh.topology().faceCount(); ++fi) {
            const auto& f = mesh.topology().face(fi);
            for (size_t j = 0; j < f.vertexCount(); ++j) {
                if (f.indices[j] != v) continue;
                uint32_t prev = f.indices[(j + f.vertexCount() - 1) % f.vertexCount()];
                uint32_t next = f.indices[(j + 1) % f.vertexCount()];
                Vec3 e1 = (pos[prev] - pos[v]).normalize();
                Vec3 e2 = (pos[next] - pos[v]).normalize();
                float dot = std::clamp(e1.dot(e2), -1.f, 1.f);
                angleSum += std::acos(dot);
            }
        }
        if (std::abs(angleSum - 2.0 * std::numbers::pi) > epsilon)
            return false;
    }
    return true;
}

// ── Sew ────────────────────────────────────────────────────────────────────

Mesh sewMesh(const Mesh& mesh, float tolerance)
{
    WeldOptions opts;
    opts.tolerance = tolerance;
    return MeshVertexWeld::weld(mesh, opts);
}

} // namespace nexus::geometry
