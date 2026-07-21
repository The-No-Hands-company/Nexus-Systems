#include <nexus/geometry/MeshSurfaceNets.h>

#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

Mesh MeshSurfaceNets::extract(const SurfaceNetsGrid& grid, const SurfaceNetsOptions& opts) {
    Mesh result;

    if (grid.nx < 2 || grid.ny < 2 || grid.nz < 2) return result;

    // Index of the vertex placed at the center of cell (x,y,z).
    // cellVertex[z][y][x] = vertex index, or kInvalid if no vertex placed.
    static constexpr uint32_t kInvalid = 0xFFFFFFFFu;
    std::vector<std::vector<std::vector<uint32_t>>> cellVertex(
        static_cast<size_t>(grid.nz) - 1,
        std::vector<std::vector<uint32_t>>(
            static_cast<size_t>(grid.ny) - 1,
            std::vector<uint32_t>(static_cast<size_t>(grid.nx) - 1, kInvalid)));

    std::vector<Vec3> positions;

    // Pass 1: place a vertex at the centre of every active cell.
    for (int32_t z = 0; z < grid.nz - 1; ++z) {
        for (int32_t y = 0; y < grid.ny - 1; ++y) {
            for (int32_t x = 0; x < grid.nx - 1; ++x) {
                bool c000 = grid.occupied(x,     y,     z);
                bool c100 = grid.occupied(x + 1, y,     z);
                bool c010 = grid.occupied(x,     y + 1, z);
                bool c110 = grid.occupied(x + 1, y + 1, z);
                bool c001 = grid.occupied(x,     y,     z + 1);
                bool c101 = grid.occupied(x + 1, y,     z + 1);
                bool c011 = grid.occupied(x,     y + 1, z + 1);
                bool c111 = grid.occupied(x + 1, y + 1, z + 1);

                int bitmask = (c000 ? 1 : 0) | (c100 ? 2 : 0) | (c010 ? 4 : 0) | (c110 ? 8 : 0) |
                              (c001 ? 16 : 0) | (c101 ? 32 : 0) | (c011 ? 64 : 0) | (c111 ? 128 : 0);

                if (bitmask == 0 || bitmask == 255) continue;

                uint32_t idx = static_cast<uint32_t>(positions.size());
                positions.push_back(Vec3{
                    static_cast<float>(x) + 0.5f,
                    static_cast<float>(y) + 0.5f,
                    static_cast<float>(z) + 0.5f,
                });
                cellVertex[static_cast<size_t>(z)]
                          [static_cast<size_t>(y)]
                          [static_cast<size_t>(x)] = idx;
            }
        }
    }

    if (positions.empty()) return result;

    auto& topo = result.topology();

    auto addQuad = [&](uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
        if (a == kInvalid || b == kInvalid || c == kInvalid || d == kInvalid) return;
        Face f1; f1.indices = {a, b, c}; topo.addFace(f1);
        Face f2; f2.indices = {a, c, d}; topo.addFace(f2);
    };

    // Pass 2: for each internal face between adjacent active cells whose
    // occupancy differs, emit a quad.
    // X-aligned cell edges (differences around the yz plane).
    for (int32_t z = 0; z < grid.nz - 1; ++z) {
        for (int32_t y = 0; y < grid.ny - 1; ++y) {
            for (int32_t x = 0; x < grid.nx - 2; ++x) {
                uint32_t vA = cellVertex[static_cast<size_t>(z)]
                                        [static_cast<size_t>(y)]
                                        [static_cast<size_t>(x)];
                uint32_t vB = cellVertex[static_cast<size_t>(z)]
                                        [static_cast<size_t>(y)]
                                        [static_cast<size_t>(x + 1)];
                if (vA == kInvalid || vB == kInvalid) continue;

                // Check if the two cells have different occupancy.
                int maskA = 0;
                for (int32_t dz = 0; dz <= 1; ++dz) {
                    for (int32_t dy = 0; dy <= 1; ++dy) {
                        if (grid.occupied(x, y + dy, z + dz)) maskA |= (1 << (dy * 2 + dz));
                    }
                }
                // For cell at x+1, the voxel at (x+1, ...) belongs to both cells.
                // The separating face contains the 4 voxels at x+1, {y,y+1}×{z,z+1}.
                bool diff = false;
                for (int32_t dz = 0; dz <= 1; ++dz) {
                    for (int32_t dy = 0; dy <= 1; ++dy) {
                        bool v0 = grid.occupied(x,     y + dy, z + dz);
                        bool v1 = grid.occupied(x + 1, y + dy, z + dz);
                        if (v0 != v1) { diff = true; break; }
                    }
                    if (diff) break;
                }
                if (!diff) continue;

                // Gather the 4 cell vertices surrounding the shared face.
                // The face is at constant x+1, spanning y..y+1, z..z+1.
                // It touches cells (x,y,z), (x+1,y,z), (x,y+1,z), (x+1,y+1,z)
                // and their z+1 counterparts.
                uint32_t cy0z0 = cellVertex[static_cast<size_t>(z)]
                                            [static_cast<size_t>(y)]
                                            [static_cast<size_t>(x)];
                uint32_t cy0z1 = cellVertex[static_cast<size_t>(z + 1)]
                                            [static_cast<size_t>(y)]
                                            [static_cast<size_t>(x)];
                uint32_t cy1z1 = cellVertex[static_cast<size_t>(z + 1)]
                                            [static_cast<size_t>(y + 1)]
                                            [static_cast<size_t>(x)];
                uint32_t cy1z0 = cellVertex[static_cast<size_t>(z)]
                                            [static_cast<size_t>(y + 1)]
                                            [static_cast<size_t>(x)];

                addQuad(cy0z0, cy0z1, cy1z1, cy1z0);  // left face

                uint32_t cy0z0r = cellVertex[static_cast<size_t>(z)]
                                             [static_cast<size_t>(y)]
                                             [static_cast<size_t>(x + 1)];
                uint32_t cy0z1r = cellVertex[static_cast<size_t>(z + 1)]
                                             [static_cast<size_t>(y)]
                                             [static_cast<size_t>(x + 1)];
                uint32_t cy1z1r = cellVertex[static_cast<size_t>(z + 1)]
                                             [static_cast<size_t>(y + 1)]
                                             [static_cast<size_t>(x + 1)];
                uint32_t cy1z0r = cellVertex[static_cast<size_t>(z)]
                                             [static_cast<size_t>(y + 1)]
                                             [static_cast<size_t>(x + 1)];

                addQuad(cy1z0r, cy1z1r, cy0z1r, cy0z0r);  // right face (winding flipped for outward normals)
            }
        }
    }

    // Y-aligned cell edges.
    for (int32_t z = 0; z < grid.nz - 1; ++z) {
        for (int32_t y = 0; y < grid.ny - 2; ++y) {
            for (int32_t x = 0; x < grid.nx - 1; ++x) {
                uint32_t vA = cellVertex[static_cast<size_t>(z)]
                                        [static_cast<size_t>(y)]
                                        [static_cast<size_t>(x)];
                uint32_t vB = cellVertex[static_cast<size_t>(z)]
                                        [static_cast<size_t>(y + 1)]
                                        [static_cast<size_t>(x)];
                if (vA == kInvalid || vB == kInvalid) continue;

                bool diff = false;
                for (int32_t dz = 0; dz <= 1; ++dz) {
                    for (int32_t dx = 0; dx <= 1; ++dx) {
                        bool v0 = grid.occupied(x + dx, y,     z + dz);
                        bool v1 = grid.occupied(x + dx, y + 1, z + dz);
                        if (v0 != v1) { diff = true; break; }
                    }
                    if (diff) break;
                }
                if (!diff) continue;

                uint32_t cx0z0 = cellVertex[static_cast<size_t>(z)]
                                            [static_cast<size_t>(y)]
                                            [static_cast<size_t>(x)];
                uint32_t cx1z0 = cellVertex[static_cast<size_t>(z)]
                                            [static_cast<size_t>(y)]
                                            [static_cast<size_t>(x + 1)];
                uint32_t cx1z1 = cellVertex[static_cast<size_t>(z + 1)]
                                            [static_cast<size_t>(y)]
                                            [static_cast<size_t>(x + 1)];
                uint32_t cx0z1 = cellVertex[static_cast<size_t>(z + 1)]
                                            [static_cast<size_t>(y)]
                                            [static_cast<size_t>(x)];

                addQuad(cx0z0, cx1z0, cx1z1, cx0z1);  // bottom face

                uint32_t cx0z0t = cellVertex[static_cast<size_t>(z)]
                                             [static_cast<size_t>(y + 1)]
                                             [static_cast<size_t>(x)];
                uint32_t cx1z0t = cellVertex[static_cast<size_t>(z)]
                                             [static_cast<size_t>(y + 1)]
                                             [static_cast<size_t>(x + 1)];
                uint32_t cx1z1t = cellVertex[static_cast<size_t>(z + 1)]
                                             [static_cast<size_t>(y + 1)]
                                             [static_cast<size_t>(x + 1)];
                uint32_t cx0z1t = cellVertex[static_cast<size_t>(z + 1)]
                                             [static_cast<size_t>(y + 1)]
                                             [static_cast<size_t>(x)];

                addQuad(cx0z1t, cx1z1t, cx1z0t, cx0z0t);  // top face
            }
        }
    }

    // Z-aligned cell edges.
    for (int32_t z = 0; z < grid.nz - 2; ++z) {
        for (int32_t y = 0; y < grid.ny - 1; ++y) {
            for (int32_t x = 0; x < grid.nx - 1; ++x) {
                uint32_t vA = cellVertex[static_cast<size_t>(z)]
                                        [static_cast<size_t>(y)]
                                        [static_cast<size_t>(x)];
                uint32_t vB = cellVertex[static_cast<size_t>(z + 1)]
                                        [static_cast<size_t>(y)]
                                        [static_cast<size_t>(x)];
                if (vA == kInvalid || vB == kInvalid) continue;

                bool diff = false;
                for (int32_t dy = 0; dy <= 1; ++dy) {
                    for (int32_t dx = 0; dx <= 1; ++dx) {
                        bool v0 = grid.occupied(x + dx, y + dy, z);
                        bool v1 = grid.occupied(x + dx, y + dy, z + 1);
                        if (v0 != v1) { diff = true; break; }
                    }
                    if (diff) break;
                }
                if (!diff) continue;

                uint32_t cx0y0 = cellVertex[static_cast<size_t>(z)]
                                            [static_cast<size_t>(y)]
                                            [static_cast<size_t>(x)];
                uint32_t cx1y0 = cellVertex[static_cast<size_t>(z)]
                                            [static_cast<size_t>(y)]
                                            [static_cast<size_t>(x + 1)];
                uint32_t cx1y1 = cellVertex[static_cast<size_t>(z)]
                                            [static_cast<size_t>(y + 1)]
                                            [static_cast<size_t>(x + 1)];
                uint32_t cx0y1 = cellVertex[static_cast<size_t>(z)]
                                            [static_cast<size_t>(y + 1)]
                                            [static_cast<size_t>(x)];

                addQuad(cx0y0, cx1y0, cx1y1, cx0y1);  // near face

                uint32_t cx0y0f = cellVertex[static_cast<size_t>(z + 1)]
                                             [static_cast<size_t>(y)]
                                             [static_cast<size_t>(x)];
                uint32_t cx1y0f = cellVertex[static_cast<size_t>(z + 1)]
                                             [static_cast<size_t>(y)]
                                             [static_cast<size_t>(x + 1)];
                uint32_t cx1y1f = cellVertex[static_cast<size_t>(z + 1)]
                                             [static_cast<size_t>(y + 1)]
                                             [static_cast<size_t>(x + 1)];
                uint32_t cx0y1f = cellVertex[static_cast<size_t>(z + 1)]
                                             [static_cast<size_t>(y + 1)]
                                             [static_cast<size_t>(x)];

                addQuad(cx0y1f, cx1y1f, cx1y0f, cx0y0f);  // far face
            }
        }
    }

    result.attributes().setPositions(positions);

    if (opts.computeNormals) {
        if (!result.computeVertexNormals()) return result;
    }

    return result;
}

} // namespace nexus::geometry
