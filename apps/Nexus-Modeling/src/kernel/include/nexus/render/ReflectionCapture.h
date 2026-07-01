#pragma once
// ─── Nexus Render ── ReflectionCapture ────────────────────────────────────

#include <nexus/render/Camera.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace nexus::render {

using Vec3 = nexus::render::Vec3;

class ReflectionCapture {
public:
    [[nodiscard]] static std::vector<float> capture(const Vec3& pos,
                                                     const std::function<Vec3(const Vec3&)>& sampleFn,
                                                     uint32_t res);

    [[nodiscard]] static std::vector<float> sample(const std::vector<float>& cubemap,
                                                    uint32_t res,
                                                    const Vec3& direction);
};

} // namespace nexus::render
