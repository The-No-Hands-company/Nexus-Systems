#pragma once
// ── Nexus Geometry — MeshSurfaceNets

#include <nexus/geometry/Mesh.h>

#include <cstdint>

namespace nexus::geometry {

struct SurfaceNetsGrid {
    int32_t nx = 0, ny = 0, nz = 0;
    std::vector<uint8_t> data;

    [[nodiscard]] bool occupied(int32_t x, int32_t y, int32_t z) const noexcept {
        if (x < 0 || x >= nx || y < 0 || y >= ny || z < 0 || z >= nz) return false;
        return data[static_cast<size_t>(x + nx * (y + ny * z))] != 0;
    }

    [[nodiscard]] size_t occupiedCount() const noexcept {
        size_t count = 0;
        for (uint8_t v : data) if (v) ++count;
        return count;
    }
};

struct SurfaceNetsOptions {
    bool computeNormals = true;
};

class MeshSurfaceNets {
public:
    [[nodiscard]] static Mesh extract(const SurfaceNetsGrid& grid,
                                      const SurfaceNetsOptions& opts = {});
};

} // namespace nexus::geometry
