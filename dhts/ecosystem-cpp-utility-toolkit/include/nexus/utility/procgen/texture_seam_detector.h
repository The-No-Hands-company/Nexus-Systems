#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <string>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <iomanip>

namespace nexus::utility::procgen {

/// Detects visible seams and artifacts in tiled procedural textures
class TextureSeamDetector {
public:
    struct SeamAnalysis {
        double max_boundary_diff = 0;      // maximum pixel difference across tile boundary
        double avg_boundary_diff = 0;      // average boundary difference
        double edge_discontinuity = 0;     // gradient discontinuity at seams
        std::vector<size_t> seam_rows;     // rows with high seam visibility
        std::vector<size_t> seam_cols;     // cols with high seam visibility
        double overall_seam_score = 0;     // 0 = seamless, 1 = very visible seams
        bool is_tileable = false;

        // Frequency analysis
        double low_freq_energy = 0;
        double high_freq_energy = 0;
        bool has_low_freq_drift = false;   // gradual intensity change across tile
    };

    /// Analyze a single-channel texture for tiling artifacts
    static SeamAnalysis analyze(const std::vector<double>& pixels,
                                 int width, int height) {
        SeamAnalysis result;

        if (width < 2 || height < 2) {
            result.is_tileable = true;
            return result;
        }

        // Horizontal seam analysis (left ↔ right boundary)
        std::vector<double> h_diffs;
        for (int y = 0; y < height; ++y) {
            double left = pixels[y * width];
            double right = pixels[y * width + (width - 1)];
            double diff = std::abs(left - right);
            h_diffs.push_back(diff);

            if (diff > 0.1) {
                // Check if this row has a visible seam
                double local_avg = 0;
                for (int x = 1; x < width - 1; ++x) {
                    local_avg += std::abs(pixels[y * width + x] - pixels[y * width + x - 1]);
                }
                local_avg /= (width - 2);
                if (diff > local_avg * 3.0) {
                    result.seam_rows.push_back(y);
                }
            }
        }

        // Vertical seam analysis (top ↔ bottom boundary)
        std::vector<double> v_diffs;
        for (int x = 0; x < width; ++x) {
            double top = pixels[x];
            double bot = pixels[(height - 1) * width + x];
            double diff = std::abs(top - bot);
            v_diffs.push_back(diff);

            if (diff > 0.1) {
                double local_avg = 0;
                for (int y = 1; y < height - 1; ++y) {
                    local_avg += std::abs(pixels[y * width + x] - pixels[(y - 1) * width + x]);
                }
                local_avg /= (height - 2);
                if (diff > local_avg * 3.0) {
                    result.seam_cols.push_back(x);
                }
            }
        }

        result.max_boundary_diff = std::max(
            h_diffs.empty() ? 0 : *std::max_element(h_diffs.begin(), h_diffs.end()),
            v_diffs.empty() ? 0 : *std::max_element(v_diffs.begin(), v_diffs.end()));

        result.avg_boundary_diff = (std::accumulate(h_diffs.begin(), h_diffs.end(), 0.0) +
                                     std::accumulate(v_diffs.begin(), v_diffs.end(), 0.0)) /
                                    (h_diffs.size() + v_diffs.size());

        // Edge discontinuity: compare interior gradients vs boundary gradients
        double interior_grad = 0;
        int interior_count = 0;
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                double gx = std::abs(pixels[y * width + x] - pixels[y * width + x - 1]);
                double gy = std::abs(pixels[y * width + x] - pixels[(y - 1) * width + x]);
                interior_grad += gx + gy;
                interior_count += 2;
            }
        }
        interior_grad /= interior_count;
        result.edge_discontinuity = (interior_grad > 1e-8)
            ? result.avg_boundary_diff / interior_grad : 0;

        // Overall seam score (0 = seamless, 1 = very visible)
        double seam_count = result.seam_rows.size() + result.seam_cols.size();
        result.overall_seam_score = std::min(1.0,
            (seam_count / static_cast<double>(width + height)) * 10.0 +
            result.edge_discontinuity * 2.0);

        result.is_tileable = result.overall_seam_score < 0.3;

        // Low frequency drift detection
        double corner_bl = pixels[0];
        double corner_br = pixels[width - 1];
        double corner_tl = pixels[(height - 1) * width];
        double corner_tr = pixels[(height - 1) * width + width - 1];
        result.low_freq_energy = (std::abs(corner_bl - corner_tr) +
                                   std::abs(corner_br - corner_tl)) / 2.0;
        result.has_low_freq_drift = result.low_freq_energy > 0.3;

        return result;
    }

    /// Analyze an RGB texture (3 channels)
    static SeamAnalysis analyzeRGB(const std::vector<uint8_t>& pixels,
                                    int width, int height, int channels = 3) {
        // Convert to grayscale for analysis
        std::vector<double> gray(width * height);
        for (int i = 0; i < width * height; ++i) {
            double r = pixels[i * channels] / 255.0;
            double g = pixels[i * channels + 1] / 255.0;
            double b = pixels[i * channels + 2] / 255.0;
            gray[i] = 0.299 * r + 0.587 * g + 0.114 * b;
        }
        return analyze(gray, width, height);
    }

    /// Generate readable report
    static std::string report(const SeamAnalysis& r) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4);
        oss << "═══ Texture Seam Analysis ═══\n";
        oss << "  Max Boundary Diff:  " << r.max_boundary_diff << "\n";
        oss << "  Avg Boundary Diff:  " << r.avg_boundary_diff << "\n";
        oss << "  Edge Discontinuity: " << r.edge_discontinuity
            << (r.edge_discontinuity > 2.0 ? " ⚠" : " ✓") << "\n";
        oss << "  Seam Score:         " << r.overall_seam_score
            << (r.is_tileable ? " ✓ tileable" : " ⚠ visible seams") << "\n";
        oss << "  Seam Rows:          " << r.seam_rows.size() << "\n";
        oss << "  Seam Cols:          " << r.seam_cols.size() << "\n";
        oss << "  Low-Freq Drift:     " << (r.has_low_freq_drift ? "⚠ YES" : "✓ none") << "\n";
        return oss.str();
    }
};

} // namespace nexus::utility::procgen
