#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <complex>

namespace nexus::utility::procgen {

/// Fractal dimension and multifractal analysis for procedural content
class FractalDimensionCalculator {
public:
    struct FractalResult {
        double box_counting_dimension = 0;
        double correlation_dimension = 0;
        double information_dimension = 0;
        double lacunarity = 0;               // gappiness measure
        double sucolarity = 0;               // image complexity
        std::vector<double> multifractal_spectrum; // D(q) for various q
        std::string classification;           // "Brownian", "fBm", "multifractal", etc.
    };

    /// Compute box-counting dimension for 2D data
    static double boxCounting2D(const std::vector<double>& data,
                                 int width, int height) {
        int max_size = std::min(width, height);
        std::vector<std::pair<double, double>> points;

        for (int box_size = 1; box_size <= max_size / 2; box_size = std::max(1, box_size * 2)) {
            int count = 0;
            for (int y = 0; y < height; y += box_size) {
                for (int x = 0; x < width; x += box_size) {
                    double min_v = std::numeric_limits<double>::max();
                    double max_v = std::numeric_limits<double>::lowest();
                    for (int dy = 0; dy < box_size && (y + dy) < height; ++dy) {
                        for (int dx = 0; dx < box_size && (x + dx) < width; ++dx) {
                            double v = data[(y + dy) * width + (x + dx)];
                            min_v = std::min(min_v, v);
                            max_v = std::max(max_v, v);
                        }
                    }
                    count += static_cast<int>(std::ceil((max_v - min_v) / (1.0 / box_size)));
                }
            }
            if (count > 0) {
                points.push_back({std::log(static_cast<double>(box_size)), std::log(static_cast<double>(count))});
            }
        }

        if (points.size() < 2) return 2.0;
        return linearRegressionSlope(points);
    }

    /// Correlation dimension using Grassberger-Procaccia
    static double correlationDimension(const std::vector<double>& data,
                                        int width, int height,
                                        double radius = 0.1) {
        // Sample points for efficiency
        const int max_samples = 2000;
        std::vector<std::pair<double, double>> points;
        int step = std::max(1, (width * height) / max_samples);

        for (int y = 0; y < height; y += step) {
            for (int x = 0; x < width; x += step) {
                double v = data[y * width + x];
                points.push_back({static_cast<double>(x) / width, v});
            }
        }

        std::vector<std::pair<double, double>> corr_points;
        for (double r = 0.001; r <= 0.5; r *= 2.0) {
            double sum = 0;
            int pair_count = 0;
            for (size_t i = 0; i < points.size(); ++i) {
                for (size_t j = i + 1; j < points.size(); j += 5) { // sparse sampling
                    double dist = std::abs(points[i].second - points[j].second);
                    if (dist < r) sum++;
                    pair_count++;
                }
            }
            if (pair_count > 0 && sum > 0) {
                corr_points.push_back({std::log(r), std::log(sum / pair_count)});
            }
        }

        if (corr_points.size() < 2) return 0;
        return std::abs(linearRegressionSlope(corr_points));
    }

    /// Compute lacunarity (gappiness measure)
    static double lacunarity(const std::vector<double>& data,
                              int width, int height,
                              int box_size = 8) {
        double mean_mass = 0, mean_mass_sq = 0;
        int box_count = 0;

        for (int y = 0; y + box_size <= height; y += box_size) {
            for (int x = 0; x + box_size <= width; x += box_size) {
                double mass = 0;
                for (int dy = 0; dy < box_size; ++dy) {
                    for (int dx = 0; dx < box_size; ++dx) {
                        mass += data[(y + dy) * width + (x + dx)];
                    }
                }
                mean_mass += mass;
                mean_mass_sq += mass * mass;
                box_count++;
            }
        }

        if (box_count == 0) return 0;
        mean_mass /= box_count;
        mean_mass_sq /= box_count;
        double variance = mean_mass_sq - mean_mass * mean_mass;
        return (mean_mass > 1e-10) ? variance / (mean_mass * mean_mass) + 1.0 : 1.0;
    }

    /// Full multifractal analysis
    static FractalResult fullAnalysis(const std::vector<double>& data,
                                       int width, int height) {
        FractalResult result;

        result.box_counting_dimension = boxCounting2D(data, width, height);
        result.correlation_dimension = correlationDimension(data, width, height);
        result.lacunarity = lacunarity(data, width, height);
        result.sucolarity = computeSucolarity(data, width, height);

        // Classification
        if (std::abs(result.box_counting_dimension - 2.5) < 0.3) {
            result.classification = "Brownian (white noise)";
        } else if (std::abs(result.box_counting_dimension - 2.2) < 0.3) {
            result.classification = "Fractional Brownian Motion (fBm)";
        } else if (result.lacunarity > 1.5) {
            result.classification = "Multifractal";
        } else {
            result.classification = "Monofractal";
        }

        // Multifractal spectrum D(q) for q = [-5, 5]
        for (int q = -5; q <= 5; ++q) {
            result.multifractal_spectrum.push_back(
                generalizedDimension(data, width, height, q));
        }

        return result;
    }

    /// Generate report
    static std::string report(const FractalResult& r) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4);
        oss << "═══ Fractal Analysis Report ═══\n";
        oss << "  Box-Counting Dim:  " << r.box_counting_dimension << "\n";
        oss << "  Correlation Dim:   " << r.correlation_dimension << "\n";
        oss << "  Lacunarity:        " << r.lacunarity
            << (r.lacunarity > 1.5 ? " (gappy)" : " (homogeneous)") << "\n";
        oss << "  Sucolarity:        " << r.sucolarity << "\n";
        oss << "  Classification:    " << r.classification << "\n";
        return oss.str();
    }

private:
    static double linearRegressionSlope(
        const std::vector<std::pair<double, double>>& pts) {
        double sx = 0, sy = 0, sxy = 0, sxx = 0;
        for (auto& [x, y] : pts) { sx += x; sy += y; sxy += x*y; sxx += x*x; }
        double n = pts.size();
        double denom = n * sxx - sx * sx;
        return (std::abs(denom) > 1e-10) ? (n * sxy - sx * sy) / denom : 0;
    }

    static double computeSucolarity(const std::vector<double>& data,
                                     int w, int h) {
        // Image complexity: std of local means
        double global_mean = std::accumulate(data.begin(), data.end(), 0.0) / data.size();
        double sum_sq = 0;
        int block_size = std::max(4, std::min(w, h) / 10);

        for (int y = 0; y < h; y += block_size) {
            for (int x = 0; x < w; x += block_size) {
                double local = 0; int cnt = 0;
                for (int dy = 0; dy < block_size && (y+dy) < h; ++dy)
                    for (int dx = 0; dx < block_size && (x+dx) < w; ++dx)
                        { local += data[(y+dy)*w + (x+dx)]; cnt++; }
                if (cnt > 0) {
                    double m = local / cnt;
                    sum_sq += (m - global_mean) * (m - global_mean);
                }
            }
        }
        return std::sqrt(sum_sq / ((w / block_size) * (h / block_size)));
    }

    static double generalizedDimension(const std::vector<double>& data,
                                        int w, int h, int q) {
        // Simplified: use histogram probabilities
        const int bins = 20;
        std::vector<double> hist(bins, 0);
        double min_v = *std::min_element(data.begin(), data.end());
        double max_v = *std::max_element(data.begin(), data.end());
        double range = max_v - min_v;
        if (range < 1e-10) return 0;

        for (auto v : data) {
            int b = std::min(bins - 1, static_cast<int>((v - min_v) / range * bins));
            hist[b]++;
        }
        for (auto& h : hist) h /= data.size();

        if (q == 1) {
            double sum = 0;
            for (auto p : hist) if (p > 0) sum += p * std::log(p);
            return -sum / std::log(bins);
        }

        double sum = 0;
        for (auto p : hist) if (p > 0) sum += std::pow(p, q);
        return (sum > 0) ? std::log(sum) / ((q - 1) * std::log(1.0 / bins)) : 0;
    }
};

} // namespace nexus::utility::procgen
