#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

struct MorphTarget {
    std::string name;
    std::vector<nexus::render::Vec3> deltas;
};

struct BlendOptions {
    bool recomputeNormals = true;
};

class MeshMorphTarget {
public:
    [[nodiscard]] static Mesh blend(const Mesh& base,
                                     const std::vector<MorphTarget>& targets,
                                     const std::vector<float>& weights,
                                     const BlendOptions& opts = {});

    [[nodiscard]] static Mesh blendMasked(const Mesh& base,
                                           const std::vector<MorphTarget>& targets,
                                           const std::vector<float>& weights,
                                           const std::vector<float>& vertexMask,
                                           const BlendOptions& opts = {});

    [[nodiscard]] static Mesh blendInBetween(const Mesh& base,
                                              const std::vector<MorphTarget>& targets,
                                              float t);

    [[nodiscard]] static Mesh createMorphTarget(const Mesh& base,
                                                 const Mesh& target,
                                                 const std::string& name = "");
};

} // namespace nexus::geometry
