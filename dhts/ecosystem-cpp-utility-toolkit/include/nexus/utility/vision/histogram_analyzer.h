#pragma once

#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <iomanip>
#include <map>

namespace nexus::utility::vision {

/// Image histogram analysis, comparison, and quality metrics
class HistogramAnalyzer {
public:
    struct Histogram {
        std::vector<size_t> bins;
        double min_val = 0, max_val = 255;
        int bin_count = 256;

        double mean() const {
            double sum = 0, total = totalCount();
            for (int i = 0; i < bin_count; ++i) sum += i * bins[i];
            return total > 0 ? sum / total * ((max_val-min_val)/bin_count) + min_val : 0;
        }

        double median() const { return percentile(0.5); }

        double percentile(double p) const {
            size_t total = totalCount();
            size_t target = static_cast<size_t>(total * p);
            size_t cum = 0;
            for (int i = 0; i < bin_count; ++i) {
                cum += bins[i];
                if (cum >= target) return min_val + (max_val-min_val) * i / bin_count;
            }
            return max_val;
        }

        size_t totalCount() const {
            return std::accumulate(bins.begin(), bins.end(), size_t(0));
        }

        double entropy() const {
            double h = 0;
            size_t total = totalCount();
            for (auto b : bins) {
                if (b > 0) { double p = static_cast<double>(b)/total; h -= p * std::log2(p); }
            }
            return h;
        }
    };

    struct ComparisonResult {
        double chi_square = 0;
        double bhattacharyya = 0;
        double intersection = 0;       // 0=disjoint, 1=identical
        double earth_movers = 0;
        double kl_divergence = 0;
    };

    /// Build histogram from 8-bit image data
    static Histogram build(const std::vector<uint8_t>& pixels, int bins = 256) {
        Histogram h;
        h.bin_count = bins;
        h.min_val = 0; h.max_val = 255;
        h.bins.resize(bins, 0);
        for (auto p : pixels) {
            int b = std::min(bins - 1, static_cast<int>(p * bins / 256));
            h.bins[b]++;
        }
        return h;
    }

    /// Build histogram from float data
    static Histogram buildFloat(const std::vector<double>& data,
                                  double min_v, double max_v, int bins = 256) {
        Histogram h;
        h.bin_count = bins;
        h.min_val = min_v; h.max_val = max_v;
        h.bins.resize(bins, 0);
        double range = max_v - min_v;
        for (auto v : data) {
            int b = static_cast<int>((v - min_v) / range * bins);
            if (b < 0) b = 0; if (b >= bins) b = bins - 1;
            h.bins[b]++;
        }
        return h;
    }

    /// Compare two histograms
    static ComparisonResult compare(const Histogram& a, const Histogram& b) {
        ComparisonResult r;
        size_t total_a = a.totalCount(), total_b = b.totalCount();
        if (total_a == 0 || total_b == 0) return r;

        // Chi-square
        for (int i = 0; i < std::min(a.bin_count, b.bin_count); ++i) {
            double expected = static_cast<double>(a.bins[i] + b.bins[i]) / 2.0;
            if (expected > 0) {
                r.chi_square += (a.bins[i] - expected) * (a.bins[i] - expected) / expected;
                r.chi_square += (b.bins[i] - expected) * (b.bins[i] - expected) / expected;
            }
        }

        // Intersection
        double intersection_sum = 0, a_sum = 0, b_sum = 0;
        for (int i = 0; i < std::min(a.bin_count, b.bin_count); ++i) {
            intersection_sum += std::min(a.bins[i], b.bins[i]);
            a_sum += a.bins[i]; b_sum += b.bins[i];
        }
        r.intersection = (a_sum + b_sum > 0) ? 2.0 * intersection_sum / (a_sum + b_sum) : 0;

        // Bhattacharyya
        double bhat = 0;
        for (int i = 0; i < std::min(a.bin_count, b.bin_count); ++i) {
            if (total_a > 0 && total_b > 0)
                bhat += std::sqrt(static_cast<double>(a.bins[i])/total_a *
                                   static_cast<double>(b.bins[i])/total_b);
        }
        r.bhattacharyya = -std::log(std::max(1e-10, bhat));

        // KL divergence
        for (int i = 0; i < std::min(a.bin_count, b.bin_count); ++i) {
            double pa = static_cast<double>(a.bins[i]) / total_a;
            double pb = static_cast<double>(b.bins[i]) / total_b;
            if (pa > 0 && pb > 0) r.kl_divergence += pa * std::log(pa / pb);
        }

        // Earth mover's distance (1D)
        double emd = 0, running = 0;
        for (int i = 0; i < std::min(a.bin_count, b.bin_count); ++i) {
            running += static_cast<double>(a.bins[i])/total_a - static_cast<double>(b.bins[i])/total_b;
            emd += std::abs(running);
        }
        r.earth_movers = emd / a.bin_count;

        return r;
    }

    /// Check if image is over/under-exposed
    static std::string exposureCheck(const Histogram& h) {
        double median = h.median();
        auto total = h.totalCount();
        size_t dark = 0, bright = 0;
        for (int i = 0; i < h.bin_count / 8; ++i) dark += h.bins[i];
        for (int i = h.bin_count * 7 / 8; i < h.bin_count; ++i) bright += h.bins[i];

        double dark_pct = static_cast<double>(dark) / total;
        double bright_pct = static_cast<double>(bright) / total;

        if (dark_pct > 0.5) return "Underexposed";
        if (bright_pct > 0.5) return "Overexposed";
        if (h.entropy() < 3.0) return "Low contrast";
        if (h.entropy() > 7.0) return "High dynamic range";
        return "Well exposed";
    }

    static std::string report(const Histogram& h) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "═══ Histogram Analysis ═══\n";
        oss << "  Mean:      " << h.mean() << "\n";
        oss << "  Median:    " << h.median() << "\n";
        oss << "  Entropy:   " << h.entropy() << " bits\n";
        oss << "  Total Px:  " << h.totalCount() << "\n";
        oss << "  Exposure:  " << exposureCheck(h) << "\n";
        return oss.str();
    }
};

} // namespace nexus::utility::vision
