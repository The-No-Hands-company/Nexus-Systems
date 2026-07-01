#pragma once
// ─── Nexus Geometry ── KnifeTool ───────────────────────────────────────────
//  Interactive cut tool: project a polyline across arbitrary mesh topology,
//  splitting edges and faces cleanly.

#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct KnifeCutResult {
    bool success = false;
    uint32_t edgesSplit = 0;
    uint32_t facesSplit = 0;
    uint32_t newVertices = 0;
    std::vector<uint32_t> cutEdges;
};

class KnifeTool {
public:
    using Vec3 = nexus::render::Vec3;

    [[nodiscard]] static KnifeCutResult cut(Mesh& mesh,
        const std::vector<Vec3>& cutPoints,
        float snapDistance = 0.001f);

    [[nodiscard]] static KnifeCutResult cutLine(Mesh& mesh,
        const Vec3& start, const Vec3& end,
        float snapDistance = 0.001f);
};

} // namespace nexus::geometry
