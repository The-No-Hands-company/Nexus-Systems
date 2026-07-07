#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <map>
#include <iomanip>

namespace nexus::utility::vision {

/// Edge detection quality analysis for Canny, Sobel, and custom edge detectors
class EdgeDetectionQuality {
public:
    struct EdgeResult {
        double edge_density = 0;           // % pixels that are edges
        double avg_gradient_magnitude = 0;
        double edge_continuity = 0;        // 0=fragmented, 1=continuous
        double noise_response = 0;         // edges in low-variance regions
        size_t edge_pixels = 0;
        size_t junction_points = 0;        // edge intersections
        size_t endpoints = 0;              // dangling edge endpoints
        double thinness_score = 0;         // 0=thick, 1=single-pixel edges (for Canny)
        std::vector<double> orientation_histogram; // 8-bin edge direction
    };

    /// Analyze edge detection output given gradient data
    static EdgeResult analyze(const std::vector<uint8_t>& edge_map,
                               const std::vector<double>& gradient_mag,
                               const std::vector<double>& gradient_dir,
                               int width, int height) {
        EdgeResult r;
        r.orientation_histogram.resize(8, 0);

        double sum_mag = 0;
        int mag_count = 0;

        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                int idx = y * width + x;
                if (edge_map[idx] > 0) {
                    r.edge_pixels++;

                    // Gradient
                    sum_mag += gradient_mag[idx];
                    mag_count++;

                    // Orientation binning
                    double angle = gradient_dir[idx];
                    if (angle < 0) angle += M_PI;
                    int bin = std::min(7, static_cast<int>(angle / M_PI * 8));
                    r.orientation_histogram[bin]++;

                    // Thinness: check if neighbor pixels are also edges
                    int neighbors = 0;
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            if ((dx || dy) && edge_map[(y+dy)*width+(x+dx)] > 0) neighbors++;
                    r.thinness_score += (8 - neighbors) / 8.0;

                    // Junctions and endpoints
                    if (neighbors >= 3) r.junction_points++;
                    if (neighbors == 1) r.endpoints++;

                    // Noise response: edge in flat region
                    double local_var = computeLocalVariance(gradient_mag, width, height, x, y);
                    if (local_var < 0.01 && gradient_mag[idx] > 0.1)
                        r.noise_response++;
                }
            }
        }

        r.edge_density = static_cast<double>(r.edge_pixels) / (width * height);
        r.avg_gradient_magnitude = mag_count > 0 ? sum_mag / mag_count : 0;
        r.thinness_score = r.edge_pixels > 0 ? r.thinness_score / r.edge_pixels : 0;

        // Edge continuity: ratio of connected to total
        r.edge_continuity = r.edge_pixels > 0 ?
            1.0 - static_cast<double>(r.endpoints) / (2.0 * r.edge_pixels) : 0;

        r.noise_response /= std::max(1.0, static_cast<double>(r.edge_pixels));

        // Normalize orientation histogram
        double total_orient = std::accumulate(r.orientation_histogram.begin(),
                                               r.orientation_histogram.end(), 0.0);
        if (total_orient > 0)
            for (auto& v : r.orientation_histogram) v /= total_orient;

        return r;
    }

    static std::string report(const EdgeResult& r) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4);
        oss << "═══ Edge Detection Quality ═══\n";
        oss << "  Edge Density:      " << (r.edge_density * 100) << "%\n";
        oss << "  Avg Gradient:      " << r.avg_gradient_magnitude << "\n";
        oss << "  Continuity:        " << r.edge_continuity
            << (r.edge_continuity < 0.6 ? " ⚠ fragmented" : " ✓") << "\n";
        oss << "  Noise Response:    " << r.noise_response
            << (r.noise_response > 0.1 ? " ⚠ noisy" : " ✓") << "\n";
        oss << "  Endpoints:         " << r.endpoints << "\n";
        oss << "  Junctions:         " << r.junction_points << "\n";
        oss << "  Thinness:          " << r.thinness_score
            << (r.thinness_score < 0.5 ? " ⚠ thick edges" : " ✓") << "\n";
        return oss.str();
    }

private:
    static double computeLocalVariance(const std::vector<double>& data,
                                        int w, int h, int cx, int cy) {
        double sum = 0, sq = 0; int n = 0;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
                if (cx+dx >= 0 && cx+dx < w && cy+dy >= 0 && cy+dy < h)
                    { double v = data[(cy+dy)*w+(cx+dx)]; sum += v; sq += v*v; n++; }
        return n > 1 ? (sq - sum*sum/n) / n : 0;
    }
};

} // namespace nexus::utility::vision
