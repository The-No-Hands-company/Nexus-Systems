#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Agent Visual Bridge — Pixel-to-Entity Resolution
//
//  Maps screen-space coordinates to geometric entity IDs so an AI agent
//  can correlate visual references ("top-left corner") with B-Rep topology.
//  Headless fallback uses BVH ray-cast; GPU path uses object-ID render buffer.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBVH.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::agent {

using Vec3 = nexus::render::Vec3;

struct VisualHit {
    uint32_t faceId = ~0u;
    uint32_t edgeId = ~0u;
    uint32_t vertexId = ~0u;
    float depth = 0.0f;
    Vec3 worldPosition{0, 0, 0};
    Vec3 worldNormal{0, 1, 0};
    bool valid = false;
};

struct VisualQuery {
    float screenX = 0.0f;
    float screenY = 0.0f;
    float screenWidth = 1920.0f;
    float screenHeight = 1080.0f;
    float radius = 5.0f; // search radius in pixels
};

struct VisualFeedback {
    std::vector<VisualHit> hits;
    uint32_t width = 1920;
    uint32_t height = 1080;
    bool valid = false;
};

struct VisualRegion {
    float minX = 0.0f, minY = 0.0f, maxX = 1.0f, maxY = 1.0f; // normalized 0..1
    std::string description;
};

class AgentVisualBridge {
public:
    AgentVisualBridge();

    void setCamera(const nexus::render::Camera& camera);

    [[nodiscard]] VisualFeedback query(const VisualQuery& query,
                                        const nexus::geometry::Mesh& mesh) const;

    [[nodiscard]] VisualFeedback queryRegion(const VisualRegion& region,
                                              const nexus::geometry::Mesh& mesh) const;

    [[nodiscard]] VisualFeedback queryByIdBuffer(
        const VisualQuery& query,
        const nexus::geometry::Mesh& mesh,
        const std::vector<uint32_t>& objectIdBuffer,
        uint32_t bufferWidth, uint32_t bufferHeight) const;

    [[nodiscard]] Vec3 screenToWorldRay(const VisualQuery& query,
                                         Vec3& origin, Vec3& direction) const;

    [[nodiscard]] const nexus::render::Camera& camera() const noexcept { return m_camera; }

    [[nodiscard]] VisualHit rayCastFace(const Vec3& origin, const Vec3& direction,
                                         const nexus::geometry::Mesh& mesh) const;

    mutable nexus::render::Camera m_camera;
};

} // namespace nexus::agent
