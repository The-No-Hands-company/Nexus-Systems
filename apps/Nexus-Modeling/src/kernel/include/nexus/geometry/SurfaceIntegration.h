#pragma once
// ─── Nexus Geometry ── SurfaceIntegration ──────────────────────────────

#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <cstdint>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct SurfaceIntegrationOptions {
    int32_t samplesU = 32;
    int32_t samplesV = 32;
};

struct SurfaceIntegrationResult {
    float area         = 0.f;
    float signedVolume = 0.f;
    nexus::render::Vec3 centroid{};
    float momentInertia[6] = {};
};

class SurfaceIntegration {
public:
    static SurfaceIntegrationResult compute(
        const NurbsSurface& surface,
        const SurfaceIntegrationOptions& opts = {});
};

} // namespace nexus::geometry
