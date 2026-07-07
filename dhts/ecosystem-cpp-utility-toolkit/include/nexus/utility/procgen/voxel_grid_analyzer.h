#pragma once

#include <vector>
#include <cstdint>
#include <cmath>
#include <string>
#include <sstream>
#include <queue>
#include <algorithm>
#include <numeric>
#include <map>
#include <set>
#include <iomanip>

namespace nexus::utility::procgen {

/// Validates voxel grids for procedural generation artifacts
class VoxelGridAnalyzer {
public:
    struct VoxelGrid {
        int width = 0, height = 0, depth = 0;
        std::vector<uint8_t> data;  // 0 = empty, 1+ = material ID
        double voxel_size = 1.0;
    };

    struct VoxelAnalysis {
        size_t total_voxels = 0;
        size_t filled_voxels = 0;
        double fill_ratio = 0;

        size_t disconnected_components = 0;
        size_t largest_component_size = 0;
        std::vector<size_t> component_sizes;

        size_t floating_islands = 0;     // disconnected from ground
        size_t interior_voids = 0;       // empty regions fully enclosed
        size_t thin_shells = 0;          // regions only 1 voxel thick
        size_t single_voxel_connections = 0; // regions connected by 1 voxel

        bool has_boundary_leaks = false;
        double surface_area_estimate = 0;
        double compactness = 0;          // surface area / volume ratio

        struct MaterialStats {
            size_t count = 0;
            double percentage = 0;
            bool is_connected = false;
        };
        std::map<uint8_t, MaterialStats> material_stats;
    };

    /// Analyze a voxel grid
    static VoxelAnalysis analyze(const VoxelGrid& grid) {
        VoxelAnalysis result;
        result.total_voxels = grid.width * grid.height * grid.depth;

        // Count filled voxels
        for (auto v : grid.data) {
            if (v > 0) result.filled_voxels++;
            result.material_stats[v].count++;
        }
        result.fill_ratio = static_cast<double>(result.filled_voxels) / result.total_voxels;

        for (auto& [mat, stats] : result.material_stats) {
            stats.percentage = static_cast<double>(stats.count) / result.total_voxels;
        }

        // Find connected components
        findComponents(grid, result);

        // Detect floating islands (components not touching bottom Y=0)
        detectFloatingIslands(grid, result);

        // Detect interior voids
        detectVoids(grid, result);

        // Check thin structures
        detectThinStructures(grid, result);

        // Estimate surface area
        result.surface_area_estimate = estimateSurfaceArea(grid);

        // Compactness
        double volume = result.filled_voxels * grid.voxel_size * grid.voxel_size * grid.voxel_size;
        result.compactness = (volume > 0) ? result.surface_area_estimate /
            std::pow(volume, 2.0 / 3.0) : 0;

        return result;
    }

    /// Check if a voxel position is valid
    static bool inBounds(const VoxelGrid& g, int x, int y, int z) {
        return x >= 0 && x < g.width && y >= 0 && y < g.height && z >= 0 && z < g.depth;
    }

    /// Generate readable report
    static std::string report(const VoxelAnalysis& a) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(3);
        oss << "═══ Voxel Grid Analysis ═══\n";
        oss << "  Total Voxels:      " << a.total_voxels << "\n";
        oss << "  Filled Voxels:     " << a.filled_voxels
            << " (" << (a.fill_ratio * 100) << "%)\n";
        oss << "  Components:        " << a.disconnected_components
            << (a.disconnected_components > 10 ? " ⚠ (fragmented)" : " ✓") << "\n";
        oss << "  Largest Component: " << a.largest_component_size << "\n";
        oss << "  Floating Islands:  " << a.floating_islands
            << (a.floating_islands > 0 ? " ⚠" : " ✓") << "\n";
        oss << "  Interior Voids:    " << a.interior_voids
            << (a.interior_voids > 0 ? "" : " ✓ solid") << "\n";
        oss << "  Thin Shells:       " << a.thin_shells
            << (a.thin_shells > 0 ? " ⚠" : " ✓") << "\n";
        oss << "  1-Voxel Conns:     " << a.single_voxel_connections
            << (a.single_voxel_connections > 0 ? " ⚠ (fragile)" : " ✓") << "\n";
        oss << "  Surface Area Est:  " << a.surface_area_estimate << "\n";
        oss << "  Compactness:       " << a.compactness
            << (a.compactness > 10 ? " ⚠ (high surface/volume)" : "") << "\n";
        oss << "  Boundary Leaks:    "
            << (a.has_boundary_leaks ? "⚠ YES" : "✓ none") << "\n";

        return oss.str();
    }

private:
    static size_t index(const VoxelGrid& g, int x, int y, int z) {
        return (z * g.height + y) * g.width + x;
    }

    static void findComponents(const VoxelGrid& g, VoxelAnalysis& r) {
        std::vector<bool> visited(g.data.size(), false);
        const int dirs[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};

        uint8_t prev_mat = 0;
        for (size_t i = 0; i < g.data.size(); ++i) {
            if (g.data[i] > 0 && !visited[i]) {
                prev_mat = g.data[i];
                size_t comp_size = 0;
                std::queue<std::tuple<int,int,int>> q;

                int sx = i % g.width;
                int sy = (i / g.width) % g.height;
                int sz = i / (g.width * g.height);
                q.push({sx, sy, sz});
                visited[i] = true;

                while (!q.empty()) {
                    auto [x, y, z] = q.front(); q.pop();
                    comp_size++;

                    for (auto& d : dirs) {
                        int nx = x + d[0], ny = y + d[1], nz = z + d[2];
                        if (!inBounds(g, nx, ny, nz)) continue;
                        size_t ni = index(g, nx, ny, nz);
                        if (!visited[ni] && g.data[ni] > 0) {
                            visited[ni] = true;
                            q.push({nx, ny, nz});
                        }
                    }
                }

                r.component_sizes.push_back(comp_size);
                if (comp_size > r.largest_component_size)
                    r.largest_component_size = comp_size;
                r.disconnected_components++;
            }
        }
    }

    static void detectFloatingIslands(const VoxelGrid& g, VoxelAnalysis& r) {
        // A component is floating if no voxel touches y=0
        std::vector<bool> visited(g.data.size(), false);
        const int dirs[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};

        for (size_t i = 0; i < g.data.size(); ++i) {
            if (g.data[i] > 0 && !visited[i]) {
                bool touches_ground = false;
                std::queue<size_t> q;
                q.push(i);
                visited[i] = true;

                while (!q.empty()) {
                    auto idx = q.front(); q.pop();
                    int y = (idx / g.width) % g.height;
                    if (y == 0) touches_ground = true;

                    int x = idx % g.width;
                    int z = idx / (g.width * g.height);
                    for (auto& d : dirs) {
                        int nx = x + d[0], ny = y + d[1], nz = z + d[2];
                        if (!inBounds(g, nx, ny, nz)) continue;
                        size_t ni = index(g, nx, ny, nz);
                        if (!visited[ni] && g.data[ni] > 0) {
                            visited[ni] = true;
                            q.push(ni);
                        }
                    }
                }

                if (!touches_ground) r.floating_islands++;
            }
        }
    }

    static void detectVoids(const VoxelGrid& g, VoxelAnalysis& r) {
        // Find empty regions fully enclosed by filled voxels
        std::vector<bool> visited(g.data.size(), false);
        const int dirs[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};

        for (size_t i = 0; i < g.data.size(); ++i) {
            if (g.data[i] == 0 && !visited[i]) {
                bool touches_boundary = false;
                std::vector<size_t> region;
                std::queue<size_t> q;
                q.push(i);
                visited[i] = true;

                while (!q.empty()) {
                    auto idx = q.front(); q.pop();
                    region.push_back(idx);
                    int x = idx % g.width;
                    int y = (idx / g.width) % g.height;
                    int z = idx / (g.width * g.height);

                    if (x == 0 || x == g.width - 1 ||
                        y == 0 || y == g.height - 1 ||
                        z == 0 || z == g.depth - 1) {
                        touches_boundary = true;
                    }

                    for (auto& d : dirs) {
                        int nx = x + d[0], ny = y + d[1], nz = z + d[2];
                        if (!inBounds(g, nx, ny, nz)) continue;
                        size_t ni = index(g, nx, ny, nz);
                        if (!visited[ni] && g.data[ni] == 0) {
                            visited[ni] = true;
                            q.push(ni);
                        }
                    }
                }

                if (!touches_boundary && region.size() > 0) {
                    r.interior_voids++;
                }
            }
        }
    }

    static void detectThinStructures(const VoxelGrid& g, VoxelAnalysis& r) {
        const int dirs[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};

        for (size_t i = 0; i < g.data.size(); ++i) {
            if (g.data[i] == 0) continue;

            int filled_neighbors = 0;
            int x = i % g.width;
            int y = (i / g.width) % g.height;
            int z = i / (g.width * g.height);

            for (auto& d : dirs) {
                int nx = x + d[0], ny = y + d[1], nz = z + d[2];
                if (inBounds(g, nx, ny, nz) && g.data[index(g, nx, ny, nz)] > 0) {
                    filled_neighbors++;
                }
            }

            if (filled_neighbors <= 1) r.thin_shells++;
            if (filled_neighbors == 1) r.single_voxel_connections++;
        }
    }

    static double estimateSurfaceArea(const VoxelGrid& g) {
        const int dirs[6][3] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
        double area = 0;
        double face_area = g.voxel_size * g.voxel_size;

        for (int z = 0; z < g.depth; ++z) {
            for (int y = 0; y < g.height; ++y) {
                for (int x = 0; x < g.width; ++x) {
                    if (g.data[index(g, x, y, z)] == 0) continue;
                    for (auto& d : dirs) {
                        int nx = x + d[0], ny = y + d[1], nz = z + d[2];
                        if (!inBounds(g, nx, ny, nz) ||
                            g.data[index(g, nx, ny, nz)] == 0) {
                            area += face_area;
                        }
                    }
                }
            }
        }
        return area;
    }
};

} // namespace nexus::utility::procgen
