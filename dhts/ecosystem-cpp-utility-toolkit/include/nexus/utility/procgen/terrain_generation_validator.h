#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <iomanip>
#include <map>
#include <set>
#include <queue>
#include <cstdint>

namespace nexus::utility::procgen {

/// Validates procedurally generated terrain height maps
class TerrainGenerationValidator {
public:
    struct TerrainStats {
        int width = 0, height = 0;
        double min_height = 0, max_height = 0;
        double mean_height = 0, stddev_height = 0;
        double roughness = 0;           // RMS slope
        double max_slope = 0;           // steepest gradient
        double erosion_score = 0;       // 0=raw, 1=highly eroded look
        double fractal_dimension = 0;   // estimated fractal dimension
        size_t local_minima = 0;
        size_t local_maxima = 0;
        size_t basins = 0;             // number of drainage basins
        double hypsometric_integral = 0; // elevation distribution
        bool has_floating_regions = false;
        bool has_pits = false;         // single-cell depressions
        std::vector<double> slope_distribution; // histogram
        std::map<std::string, double> biome_coverage; // % of each biome
    };

    /// Analyze a heightmap
    static TerrainStats analyze(const std::vector<double>& heightmap,
                                 int width, int height,
                                 double max_world_height = 1.0) {
        TerrainStats stats;
        stats.width = width;
        stats.height = height;

        if (heightmap.empty()) return stats;

        // Basic statistics
        stats.min_height = *std::min_element(heightmap.begin(), heightmap.end());
        stats.max_height = *std::max_element(heightmap.begin(), heightmap.end());

        double sum = std::accumulate(heightmap.begin(), heightmap.end(), 0.0);
        stats.mean_height = sum / heightmap.size();

        double sq_sum = 0;
        for (auto h : heightmap) sq_sum += (h - stats.mean_height) * (h - stats.mean_height);
        stats.stddev_height = std::sqrt(sq_sum / heightmap.size());

        // Slope analysis
        computeSlopes(heightmap, width, height, stats);
        // Local extrema
        findExtrema(heightmap, width, height, stats);
        // Drainage basins
        findBasins(heightmap, width, height, stats);
        // Fractal dimension
        stats.fractal_dimension = estimateFractalDimension(heightmap, width, height);
        // Hypsometric integral
        stats.hypsometric_integral = computeHypsometric(heightmap, stats);
        // Biome classification
        classifyBiomes(heightmap, width, height, max_world_height, stats);
        // Pit detection
        detectPits(heightmap, width, height, stats);

        return stats;
    }

    /// Generate readable report
    static std::string report(const TerrainStats& s) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4);
        oss << "═══ Terrain Generation Validation ═══\n";
        oss << "  Resolution:       " << s.width << "×" << s.height << "\n";
        oss << "  Height Range:     [" << s.min_height << ", " << s.max_height << "]\n";
        oss << "  Mean/SD Height:   " << s.mean_height << " / " << s.stddev_height << "\n";
        oss << "  Roughness (RMS):  " << s.roughness << "\n";
        oss << "  Max Slope:        " << s.max_slope
            << (s.max_slope > 1.5 ? " ⚠ (cliffs)" : "") << "\n";
        oss << "  Erosion Score:    " << s.erosion_score
            << (s.erosion_score < 0.1 ? " (raw/uneroded)" : "") << "\n";
        oss << "  Fractal Dim:      " << s.fractal_dimension << "\n";
        oss << "  Local Minima:     " << s.local_minima << "\n";
        oss << "  Local Maxima:     " << s.local_maxima << "\n";
        oss << "  Drainage Basins:  " << s.basins << "\n";
        oss << "  Hypsometric Int:  " << s.hypsometric_integral << "\n";
        oss << "  Floating Regions: " << (s.has_floating_regions ? "⚠ YES" : "✓ no") << "\n";
        oss << "  Pits:             " << (s.has_pits ? "⚠ YES" : "✓ no") << "\n";
        if (!s.biome_coverage.empty()) {
            oss << "  Biome Coverage:\n";
            for (auto& [biome, pct] : s.biome_coverage) {
                oss << "    " << biome << ": " << (pct * 100) << "%\n";
            }
        }
        return oss.str();
    }

private:
    static void computeSlopes(const std::vector<double>& hm,
                               int w, int h, TerrainStats& s) {
        double max_slope = 0, rms_sum = 0;
        int count = 0;

        // 10-bin slope histogram
        s.slope_distribution.resize(10, 0);

        for (int y = 1; y < h; ++y) {
            for (int x = 1; x < w; ++x) {
                double dx = hm[y * w + x] - hm[y * w + (x - 1)];
                double dy = hm[y * w + x] - hm[(y - 1) * w + x];
                double slope = std::sqrt(dx * dx + dy * dy);
                if (slope > max_slope) max_slope = slope;
                rms_sum += slope * slope;
                count++;

                int bin = std::min(9, static_cast<int>(slope * 5));
                s.slope_distribution[bin]++;
            }
        }

        s.max_slope = max_slope;
        s.roughness = count > 0 ? std::sqrt(rms_sum / count) : 0;

        // Erosion score: ratio of smooth regions to steep regions
        double smooth = 0, steep = 0;
        for (int i = 0; i < 5; ++i) smooth += s.slope_distribution[i];
        for (int i = 5; i < 10; ++i) steep += s.slope_distribution[i];
        s.erosion_score = (smooth + steep > 0) ? smooth / (smooth + steep) : 0.5;
    }

    static void findExtrema(const std::vector<double>& hm,
                             int w, int h, TerrainStats& s) {
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                double center = hm[y * w + x];
                bool is_min = true, is_max = true;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        double neighbor = hm[(y + dy) * w + (x + dx)];
                        if (neighbor <= center) is_max = false;
                        if (neighbor >= center) is_min = false;
                    }
                }
                if (is_min) s.local_minima++;
                if (is_max) s.local_maxima++;
            }
        }
    }

    static void findBasins(const std::vector<double>& hm,
                            int w, int h, TerrainStats& s) {
        std::vector<bool> visited(w * h, false);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (visited[y * w + x]) continue;

                // Flood fill to find connected region below a threshold
                double threshold = s.mean_height + 0.3 * s.stddev_height;
                if (hm[y * w + x] > threshold) continue;

                std::queue<std::pair<int,int>> q;
                q.push({x, y});
                visited[y * w + x] = true;
                size_t region_size = 0;

                while (!q.empty()) {
                    auto [cx, cy] = q.front(); q.pop();
                    region_size++;
                    for (int dy = -1; dy <= 1; ++dy) {
                        for (int dx = -1; dx <= 1; ++dx) {
                            if (dx == 0 && dy == 0) continue;
                            int nx = cx + dx, ny = cy + dy;
                            if (nx < 0 || nx >= w || ny < 0 || ny >= h) continue;
                            if (!visited[ny * w + nx] && hm[ny * w + nx] <= threshold) {
                                visited[ny * w + nx] = true;
                                q.push({nx, ny});
                            }
                        }
                    }
                }
                if (region_size > 10) s.basins++;
            }
        }
    }

    static double estimateFractalDimension(const std::vector<double>& hm,
                                            int w, int h) {
        // Box-counting method
        int max_size = std::min(w, h) / 2;
        std::vector<std::pair<double, double>> points; // log(size), log(count)

        for (int size = 1; size <= max_size; size *= 2) {
            int boxes = 0;
            for (int y = 0; y < h; y += size) {
                for (int x = 0; x < w; x += size) {
                    double min_h = std::numeric_limits<double>::max();
                    double max_h = std::numeric_limits<double>::lowest();
                    for (int dy = 0; dy < size && (y + dy) < h; ++dy) {
                        for (int dx = 0; dx < size && (x + dx) < w; ++dx) {
                            double val = hm[(y + dy) * w + (x + dx)];
                            min_h = std::min(min_h, val);
                            max_h = std::max(max_h, val);
                        }
                    }
                    int h_boxes = static_cast<int>((max_h - min_h) / (1.0 / size)) + 1;
                    boxes += h_boxes;
                }
            }
            if (boxes > 0 && size > 0) {
                points.push_back({std::log(static_cast<double>(size)), std::log(static_cast<double>(boxes))});
            }
        }

        if (points.size() < 2) return 2.0;

        // Linear regression slope
        double sum_x = 0, sum_y = 0, sum_xy = 0, sum_xx = 0;
        for (auto& [x, y] : points) {
            sum_x += x; sum_y += y; sum_xy += x * y; sum_xx += x * x;
        }
        double n = points.size();
        double slope = (n * sum_xy - sum_x * sum_y) / (n * sum_xx - sum_x * sum_x);
        return -slope; // fractal dimension
    }

    static double computeHypsometric(const std::vector<double>& hm,
                                      const TerrainStats& s) {
        std::vector<double> sorted = hm;
        std::sort(sorted.begin(), sorted.end());
        double total_area = sorted.size();
        double area_above_mean = 0;
        double total_volume = 0;
        double min_h = s.min_height;
        double range = s.max_height - min_h;

        for (size_t i = 0; i < sorted.size(); ++i) {
            total_volume += (sorted[i] - min_h);
        }

        // Area above mean elevation
        for (auto v : sorted) {
            if (v > s.mean_height) area_above_mean++;
        }

        return area_above_mean / total_area;
    }

    static void classifyBiomes(const std::vector<double>& hm,
                                int w, int h, double max_height,
                                TerrainStats& s) {
        for (auto val : hm) {
            double norm = val / max_height;
            std::string biome;
            if (norm < 0.1) biome = "Deep Ocean";
            else if (norm < 0.3) biome = "Shallow Water";
            else if (norm < 0.4) biome = "Beach";
            else if (norm < 0.6) biome = "Grassland";
            else if (norm < 0.75) biome = "Forest";
            else if (norm < 0.9) biome = "Mountain";
            else biome = "Snow Peak";
            s.biome_coverage[biome]++;
        }

        double total = w * h;
        for (auto& [_, count] : s.biome_coverage) {
            count /= total;
        }
    }

    static void detectPits(const std::vector<double>& hm,
                            int w, int h, TerrainStats& s) {
        for (int y = 1; y < h - 1; ++y) {
            for (int x = 1; x < w - 1; ++x) {
                double center = hm[y * w + x];
                bool is_pit = true;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        if (hm[(y + dy) * w + (x + dx)] <= center) {
                            is_pit = false;
                            break;
                        }
                    }
                    if (!is_pit) break;
                }
                if (is_pit) { s.has_pits = true; return; }
            }
        }
    }
};

} // namespace nexus::utility::procgen
