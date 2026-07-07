#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <numeric>
#include <string>
#include <sstream>
#include <iomanip>
#include <map>
#include <random>

namespace nexus::utility::procgen {

/// Comprehensive noise quality analyzer for Perlin, Simplex, and Worley noise
class NoiseQualityAnalyzer {
public:
    struct NoiseStats {
        double min_val = 0, max_val = 0;
        double mean = 0, variance = 0, stddev = 0;
        double skewness = 0, kurtosis = 0;
        std::vector<double> histogram;
        double histogram_bin_width = 0.05;
        int histogram_bins = 40;
        double isotropy_score = 0;       // 0-1, directional uniformity
        double frequency_purity = 0;     // dominant frequency strength
        std::vector<double> frequency_spectrum;
    };

    /// Generate a 2D noise field and analyze it
    template<typename NoiseFunc>
    static NoiseStats analyze(NoiseFunc&& noise, int width, int height,
                               double frequency = 1.0, int seed = 42) {
        NoiseStats stats;

        // Generate samples
        std::vector<double> samples;
        samples.reserve(width * height);
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                double nx = static_cast<double>(x) / width * frequency;
                double ny = static_cast<double>(y) / height * frequency;
                samples.push_back(noise(nx, ny, seed));
            }
        }

        computeBasicStats(samples, stats);
        computeHistogram(samples, stats);
        computeIsotropy(samples, width, height, noise, frequency, seed, stats);
        computeFrequencySpectrum(samples, width, height, stats);

        return stats;
    }

    /// Generate a 3D noise field and analyze it
    template<typename NoiseFunc3D>
    static NoiseStats analyze3D(NoiseFunc3D&& noise, int width, int height, int depth,
                                 double frequency = 1.0, int seed = 42) {
        NoiseStats stats;

        std::vector<double> samples;
        samples.reserve(width * height * depth);
        for (int z = 0; z < depth; ++z) {
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < width; ++x) {
                    double nx = static_cast<double>(x) / width * frequency;
                    double ny = static_cast<double>(y) / height * frequency;
                    double nz = static_cast<double>(z) / depth * frequency;
                    samples.push_back(noise(nx, ny, nz, seed));
                }
            }
        }

        computeBasicStats(samples, stats);
        computeHistogram(samples, stats);

        return stats;
    }

    /// Reference Perlin noise implementation for comparison
    static double perlin2D(double x, double y, int seed = 42) {
        int x0 = static_cast<int>(std::floor(x));
        int y0 = static_cast<int>(std::floor(y));
        int x1 = x0 + 1, y1 = y0 + 1;
        double sx = x - x0, sy = y - y0;
        double u = fade(sx), v = fade(sy);

        double n00 = grad(hash(x0, y0, seed), sx, sy);
        double n10 = grad(hash(x1, y0, seed), sx - 1, sy);
        double n01 = grad(hash(x0, y1, seed), sx, sy - 1);
        double n11 = grad(hash(x1, y1, seed), sx - 1, sy - 1);

        double nx0 = lerp(n00, n10, u);
        double nx1 = lerp(n01, n11, u);
        return lerp(nx0, nx1, v);
    }

    /// Reference Simplex-like noise for comparison
    static double simplex2D(double x, double y, int seed = 42) {
        double skew = (x + y) * 0.3660254037844386; // F2 = (sqrt(3)-1)/2
        int i = static_cast<int>(std::floor(x + skew));
        int j = static_cast<int>(std::floor(y + skew));
        double unskew = (i + j) * 0.21132486540518713; // G2
        double x0 = x - (i - unskew);
        double y0 = y - (j - unskew);

        int i1, j1;
        if (x0 > y0) { i1 = 1; j1 = 0; }
        else         { i1 = 0; j1 = 1; }

        double x1 = x0 - i1 + 0.21132486540518713;
        double y1 = y0 - j1 + 0.21132486540518713;
        double x2 = x0 - 1.0 + 2.0 * 0.21132486540518713;
        double y2 = y0 - 1.0 + 2.0 * 0.21132486540518713;

        double n0 = contrib2D(x0, y0, hash(i, j, seed));
        double n1 = contrib2D(x1, y1, hash(i + i1, j + j1, seed));
        double n2 = contrib2D(x2, y2, hash(i + 1, j + 1, seed));

        return 70.0 * (n0 + n1 + n2);
    }

    /// Worley (Voronoi) noise
    static double worley2D(double x, double y, int seed = 42) {
        int cx = static_cast<int>(std::floor(x));
        int cy = static_cast<int>(std::floor(y));
        double fx = x - cx, fy = y - cy;

        double min_dist = std::numeric_limits<double>::max();
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                double px = fx - dx - (hash(cx + dx, cy + dy, seed) % 1000) / 1000.0;
                double py = fy - dy - (hash(cx + dx, cy + dy, seed + 1) % 1000) / 1000.0;
                double d = std::sqrt(px * px + py * py);
                if (d < min_dist) min_dist = d;
            }
        }
        return min_dist;
    }

    /// Generate readable report
    static std::string report(const NoiseStats& stats) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4);
        oss << "═══ Noise Quality Report ═══\n";
        oss << "  Range:        [" << stats.min_val << ", " << stats.max_val << "]\n";
        oss << "  Mean:         " << stats.mean << "\n";
        oss << "  StdDev:       " << stats.stddev << "\n";
        oss << "  Skewness:     " << stats.skewness
            << (std::abs(stats.skewness) > 0.5 ? " ⚠ (asymmetric)" : " ✓") << "\n";
        oss << "  Kurtosis:     " << stats.kurtosis
            << (stats.kurtosis > 4.0 ? " ⚠ (heavy-tailed)" : " ✓") << "\n";
        oss << "  Isotropy:     " << stats.isotropy_score
            << (stats.isotropy_score < 0.7 ? " ⚠ (directional bias)" : " ✓") << "\n";
        oss << "  Freq Purity:  " << stats.frequency_purity << "\n";
        return oss.str();
    }

private:
    // Basic statistics
    static void computeBasicStats(const std::vector<double>& samples, NoiseStats& stats) {
        if (samples.empty()) return;

        stats.min_val = *std::min_element(samples.begin(), samples.end());
        stats.max_val = *std::max_element(samples.begin(), samples.end());

        stats.mean = std::accumulate(samples.begin(), samples.end(), 0.0) / samples.size();

        double m2 = 0, m3 = 0, m4 = 0;
        for (auto s : samples) {
            double d = s - stats.mean;
            m2 += d * d;
            m3 += d * d * d;
            m4 += d * d * d * d;
        }
        double n = static_cast<double>(samples.size());
        stats.variance = m2 / n;
        stats.stddev = std::sqrt(stats.variance);
        stats.skewness = (stats.stddev > 1e-10) ? (m3 / n) / (stats.stddev * stats.stddev * stats.stddev) : 0;
        stats.kurtosis = (stats.variance > 1e-10) ? (m4 / n) / (stats.variance * stats.variance) : 0;
    }

    // Histogram analysis
    static void computeHistogram(const std::vector<double>& samples, NoiseStats& stats) {
        stats.histogram.resize(stats.histogram_bins, 0);
        for (auto s : samples) {
            int bin = static_cast<int>((s - stats.min_val) / stats.histogram_bin_width);
            if (bin < 0) bin = 0;
            if (bin >= stats.histogram_bins) bin = stats.histogram_bins - 1;
            stats.histogram[bin]++;
        }
    }

    // Isotropy check: compare variance in different directions
    template<typename NoiseFunc>
    static void computeIsotropy(const std::vector<double>& samples, int w, int h,
                                 NoiseFunc&& noise, double freq, int seed, NoiseStats& stats) {
        // Sample along 8 directional lines and compare variance
        const int num_dirs = 8;
        std::vector<double> dir_vars;
        for (int d = 0; d < num_dirs; ++d) {
            double angle = d * M_PI / num_dirs;
            double dx = std::cos(angle), dy = std::sin(angle);
            std::vector<double> line;
            for (int i = 0; i < 100; ++i) {
                line.push_back(noise(dx * i * 0.1, dy * i * 0.1, seed));
            }
            double m = std::accumulate(line.begin(), line.end(), 0.0) / line.size();
            double var = 0;
            for (auto v : line) var += (v - m) * (v - m);
            dir_vars.push_back(var / line.size());
        }

        double max_var = *std::max_element(dir_vars.begin(), dir_vars.end());
        double min_var = *std::min_element(dir_vars.begin(), dir_vars.end());
        stats.isotropy_score = (max_var > 1e-10) ? min_var / max_var : 1.0;
    }

    // Simple frequency analysis
    static void computeFrequencySpectrum(const std::vector<double>& samples,
                                          int w, int h, NoiseStats& stats) {
        // Simplified: compute variance at multiple scales
        stats.frequency_spectrum.resize(8, 0);
        for (int octave = 0; octave < 8; ++octave) {
            int step = 1 << octave;
            double sum = 0;
            int count = 0;
            for (int y = 0; y < h; y += step) {
                for (int x = 0; x < w; x += step) {
                    double v = samples[y * w + x];
                    sum += v;
                    count++;
                }
            }
            stats.frequency_spectrum[octave] = (count > 0) ? sum / count : 0;
        }

        // Frequency purity: ratio of dominant to total energy
        double total = 0, dominant = 0;
        for (auto v : stats.frequency_spectrum) {
            total += std::abs(v);
            if (std::abs(v) > dominant) dominant = std::abs(v);
        }
        stats.frequency_purity = (total > 1e-10) ? dominant / total : 0;
    }

    // Perlin utility functions
    static double fade(double t) { return t * t * t * (t * (t * 6 - 15) + 10); }
    static double lerp(double a, double b, double t) { return a + t * (b - a); }

    static double grad(int hash, double x, double y) {
        int h = hash & 3;
        double u = (h < 2) ? x : y;
        double v = (h < 2) ? y : x;
        return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
    }

    static int hash(int x, int y, int seed) {
        int h = seed + x * 374761393 + y * 668265263;
        h = (h ^ (h >> 13)) * 1274126177;
        return h ^ (h >> 16);
    }

    static double contrib2D(double x, double y, int hash_val) {
        double t = 0.5 - x * x - y * y;
        if (t < 0) return 0;
        t *= t;
        return t * t * grad(hash_val, x, y);
    }
};

} // namespace nexus::utility::procgen
