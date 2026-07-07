#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <iomanip>

namespace nexus::utility::vision {

/// Feature matching quality analysis (SIFT/ORB/AKAZE-style descriptors)
class FeatureMatchDebugger {
public:
    struct Keypoint {
        double x, y;
        double scale = 1.0;
        double orientation = 0;
        double response = 0;
    };

    struct Match {
        size_t query_idx, train_idx;
        double distance;
    };

    struct MatchAnalysis {
        size_t total_matches = 0;
        size_t inlier_count = 0;
        double inlier_ratio = 0;
        double avg_distance = 0;
        double min_distance = 0;
        double distance_ratio_mean = 0;   // Lowe's ratio test average
        double spatial_consistency = 0;   // 0=random, 1=consistent motion
        std::vector<double> distance_distribution;
    };

    /// Analyze feature matches for quality
    static MatchAnalysis analyze(const std::vector<Match>& matches,
                                   const std::vector<Keypoint>& query_kps,
                                   const std::vector<Keypoint>& train_kps,
                                   double ransac_threshold = 3.0) {
        MatchAnalysis r;
        r.total_matches = matches.size();
        if (matches.empty()) return r;

        // Distance statistics
        std::vector<double> distances;
        for (auto& m : matches) distances.push_back(m.distance);
        std::sort(distances.begin(), distances.end());

        r.min_distance = distances.front();
        r.avg_distance = std::accumulate(distances.begin(), distances.end(), 0.0) / distances.size();

        // Distance distribution
        r.distance_distribution.resize(10, 0);
        double max_d = distances.back();
        for (auto d : distances) {
            int bin = max_d > 0 ? std::min(9, static_cast<int>(d / max_d * 10)) : 0;
            r.distance_distribution[bin]++;
        }

        // Lowe's ratio: check if second-best is much worse
        double ratio_sum = 0;
        int ratio_count = 0;
        for (size_t i = 0; i + 1 < distances.size(); i += 2) {
            if (distances[i] > 0) ratio_sum += distances[i] / distances[i+1];
            ratio_count++;
        }
        r.distance_ratio_mean = ratio_count > 0 ? ratio_sum / ratio_count : 0;

        // Spatial consistency: check if matched points have similar displacement
        double disp_sum = 0, disp_sq = 0;
        int disp_count = 0;
        for (auto& m : matches) {
            if (m.query_idx < query_kps.size() && m.train_idx < train_kps.size()) {
                double dx = query_kps[m.query_idx].x - train_kps[m.train_idx].x;
                double dy = query_kps[m.query_idx].y - train_kps[m.train_idx].y;
                disp_sum += dx + dy;
                disp_sq += dx*dx + dy*dy;
                disp_count++;
            }
        }
        if (disp_count > 1) {
            double mean_disp = disp_sum / disp_count;
            double var = disp_sq/disp_count - mean_disp*mean_disp;
            r.spatial_consistency = 1.0 / (1.0 + std::sqrt(std::max(0.0, var)));
        }

        // Inlier estimation: if distance < 2 * min_distance
        for (auto& m : matches)
            if (m.distance < 2.0 * r.min_distance) r.inlier_count++;
        r.inlier_ratio = static_cast<double>(r.inlier_count) / r.total_matches;

        return r;
    }

    static std::string report(const MatchAnalysis& r) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4);
        oss << "═══ Feature Match Quality ═══\n";
        oss << "  Total Matches:     " << r.total_matches << "\n";
        oss << "  Inlier Ratio:      " << r.inlier_ratio
            << (r.inlier_ratio < 0.3 ? " ⚠ low" : " ✓") << "\n";
        oss << "  Avg Distance:      " << r.avg_distance << "\n";
        oss << "  Min Distance:      " << r.min_distance << "\n";
        oss << "  Distance Ratio:    " << r.distance_ratio_mean
            << (r.distance_ratio_mean > 0.8 ? " ⚠ ambiguous" : " ✓") << "\n";
        oss << "  Spatial Consist:   " << r.spatial_consistency
            << (r.spatial_consistency < 0.5 ? " ⚠" : " ✓") << "\n";
        return oss.str();
    }
};

} // namespace nexus::utility::vision
