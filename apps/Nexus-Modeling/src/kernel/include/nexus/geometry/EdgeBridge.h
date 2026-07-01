#pragma once
// ─── Nexus Geometry ── EdgeBridge ──────────────────────────────────────────
//  Bridge two edge loops with interpolation, handling mismatched vertex counts.

#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct BridgeOptions {
    uint32_t segments = 1;
    bool closed = false;
    bool matchOrientation = true;
};

struct BridgeReport {
    bool success = false;
    uint32_t facesCreated = 0;
    uint32_t verticesCreated = 0;
};

class EdgeBridge {
public:
    using Vec3 = nexus::render::Vec3;

    [[nodiscard]] static BridgeReport bridge(
        const std::vector<uint32_t>& loopA,
        const std::vector<uint32_t>& loopB,
        const std::vector<Vec3>& positions,
        Mesh& output,
        const BridgeOptions& opts = {});

    [[nodiscard]] static BridgeReport bridgeEdgeLoopsFromMesh(
        const Mesh& mesh,
        const std::vector<uint32_t>& loopA,
        const std::vector<uint32_t>& loopB,
        Mesh& output,
        const BridgeOptions& opts = {});

private:
    static std::vector<uint32_t> interpolateLoop(
        const std::vector<uint32_t>& loop,
        const std::vector<Vec3>& positions,
        uint32_t targetCount);
};

} // namespace nexus::geometry
