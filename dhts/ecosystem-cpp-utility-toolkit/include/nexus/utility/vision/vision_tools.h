#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <algorithm>
#include <iomanip>

namespace nexus::utility::vision {

/// Template matching quality analysis
class TemplateMatchDebugger {
public:
    struct MatchResult {
        double x, y, score;
        double confidence;
    };

    struct Analysis {
        double peak_score = 0;
        double peak_to_secondary_ratio = 0;  // distinctiveness
        double match_localization = 0;       // peak sharpness
        double background_interference = 0;
        std::vector<double> score_map_stats;
        bool is_ambiguous = false;
    };

    static Analysis analyze(const std::vector<double>& score_map,
                             int map_w, int map_h,
                             const MatchResult& best_match) {
        Analysis r;
        r.peak_score = best_match.score;

        // Find secondary peak (outside suppression radius)
        int radius = 10;
        double secondary = 0;
        for (int y = 0; y < map_h; ++y) {
            for (int x = 0; x < map_w; ++x) {
                double dx = x - best_match.x, dy = y - best_match.y;
                if (std::sqrt(dx*dx+dy*dy) > radius) {
                    secondary = std::max(secondary, score_map[y*map_w+x]);
                }
            }
        }
        r.peak_to_secondary_ratio = (secondary > 0) ? r.peak_score / secondary : 999;

        // Localization sharpness
        double peak_sharpness = 0;
        int bx = static_cast<int>(best_match.x), by = static_cast<int>(best_match.y);
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                int nx = bx+dx, ny = by+dy;
                if (nx>=0 && nx<map_w && ny>=0 && ny<map_h) {
                    peak_sharpness += (r.peak_score - score_map[ny*map_w+nx]);
                }
            }
        }
        r.match_localization = peak_sharpness / 25.0;

        // Background interference
        double bg_sum = 0; int bg_count = 0;
        for (int y = 0; y < map_h; ++y) {
            for (int x = 0; x < map_w; ++x) {
                double dx = x - best_match.x, dy = y - best_match.y;
                if (std::sqrt(dx*dx+dy*dy) > 2*radius) {
                    bg_sum += score_map[y*map_w+x];
                    bg_count++;
                }
            }
        }
        r.background_interference = bg_count > 0 ? bg_sum / bg_count : 0;

        r.is_ambiguous = r.peak_to_secondary_ratio < 2.0;

        return r;
    }

    static std::string report(const Analysis& r) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(4);
        oss << "═══ Template Match Quality ═══\n";
        oss << "  Peak Score:           " << r.peak_score << "\n";
        oss << "  Peak/Secondary:       " << r.peak_to_secondary_ratio
            << (r.is_ambiguous ? " ⚠ ambiguous" : " ✓ distinct") << "\n";
        oss << "  Localization:         " << r.match_localization
            << (r.match_localization < 0.1 ? " ⚠ flat peak" : " ✓ sharp") << "\n";
        oss << "  Background Interf:    " << r.background_interference
            << (r.background_interference > 0.3 ? " ⚠ high" : " ✓") << "\n";
        return oss.str();
    }
};

/// Morphological operations validator
class MorphologyValidator {
public:
    struct MorphAnalysis {
        bool preserves_topology = true;
        size_t holes_created = 0;
        size_t holes_closed = 0;
        double size_change_ratio = 1.0;
        std::vector<std::string> warnings;
    };

    static MorphAnalysis analyze(const std::vector<uint8_t>& before,
                                   const std::vector<uint8_t>& after,
                                   int w, int h) {
        MorphAnalysis r;
        size_t before_fg = std::count(before.begin(), before.end(), uint8_t(1));
        size_t after_fg = std::count(after.begin(), after.end(), uint8_t(1));
        r.size_change_ratio = before_fg > 0 ? static_cast<double>(after_fg)/before_fg : 1;

        if (std::abs(r.size_change_ratio - 1.0) > 0.5)
            r.warnings.push_back("Large area change: " + std::to_string(r.size_change_ratio));

        // Hole detection simplified
        for (int y=1; y<h-1; ++y)
            for (int x=1; x<w-1; ++x)
                if (!before[y*w+x] && after[y*w+x]) r.holes_closed++;
                else if (before[y*w+x] && !after[y*w+x]) r.holes_created++;

        return r;
    }

    static std::string report(const MorphAnalysis& r) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(3);
        oss << "═══ Morphology Validation ═══\n";
        oss << "  Size Ratio:    " << r.size_change_ratio << "\n";
        oss << "  Holes Created: " << r.holes_created << "\n";
        oss << "  Holes Closed:  " << r.holes_closed << "\n";
        for (auto& w : r.warnings) oss << "  ⚠ " << w << "\n";
        return oss.str();
    }
};

/// Image difference/debug tool
class ImageDiff {
public:
    struct DiffResult {
        double mse = 0, psnr = 0, ssim = 0;
        size_t different_pixels = 0;
        double diff_percentage = 0;
        std::vector<size_t> diff_histogram;
    };

    static DiffResult compare(const std::vector<uint8_t>& a,
                               const std::vector<uint8_t>& b,
                               int w, int h, int channels = 1) {
        DiffResult r;
        r.diff_histogram.resize(256, 0);
        double mse_sum = 0;

        for (size_t i = 0; i < a.size(); i += channels) {
            double px_diff = 0;
            for (int c = 0; c < channels; ++c)
                px_diff += std::abs(static_cast<int>(a[i+c]) - static_cast<int>(b[i+c]));
            px_diff /= channels;
            mse_sum += px_diff * px_diff;
            if (px_diff > 1) r.different_pixels++;
            r.diff_histogram[std::min(255, static_cast<int>(px_diff))]++;
        }

        size_t px_count = a.size() / channels;
        r.mse = mse_sum / px_count;
        r.psnr = r.mse > 0 ? 20 * std::log10(255.0 / std::sqrt(r.mse)) : 100;
        r.diff_percentage = 100.0 * r.different_pixels / px_count;
        r.ssim = estimateSSIM(a, b, w, h, channels);

        return r;
    }

    static std::string report(const DiffResult& r) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(2);
        oss << "═══ Image Diff ═══\n";
        oss << "  MSE:       " << r.mse << "\n";
        oss << "  PSNR:      " << r.psnr << " dB"
            << (r.psnr < 30 ? " ⚠" : " ✓") << "\n";
        oss << "  SSIM:      " << r.ssim
            << (r.ssim < 0.9 ? " ⚠" : " ✓") << "\n";
        oss << "  Diff Px:   " << r.diff_percentage << "%\n";
        return oss.str();
    }

private:
    static double estimateSSIM(const std::vector<uint8_t>& a,
                                const std::vector<uint8_t>& b,
                                int w, int h, int ch) {
        double c1=6.5025, c2=58.5225; // (0.01*255)^2, (0.03*255)^2
        double mu_a=0, mu_b=0;
        for (size_t i=0; i<a.size(); i+=ch) { mu_a+=a[i]; mu_b+=b[i]; }
        double n=a.size()/ch;
        mu_a/=n; mu_b/=n;
        double va=0, vb=0, cov=0;
        for (size_t i=0; i<a.size(); i+=ch) {
            double da=a[i]-mu_a, db=b[i]-mu_b;
            va+=da*da; vb+=db*db; cov+=da*db;
        }
        va/=n; vb/=n; cov/=n;
        return (2*mu_a*mu_b+c1)*(2*cov+c2)/((mu_a*mu_a+mu_b*mu_b+c1)*(va+vb+c2));
    }
};

} // namespace nexus::utility::vision
