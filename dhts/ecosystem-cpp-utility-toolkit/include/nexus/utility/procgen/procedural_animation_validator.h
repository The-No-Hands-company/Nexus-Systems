#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <limits>

namespace nexus::utility::procgen {

/// Validates procedurally generated animations for jitter, popping, and discontinuities
class ProceduralAnimationValidator {
public:
    struct FrameData {
        std::vector<double> positions;  // concatenated: x0,y0,z0, x1,y1,z1, ...
        int joint_count = 0;
    };

    struct AnimationTrack {
        std::vector<FrameData> frames;
        double frame_rate = 30.0;
        std::string name;
    };

    struct JointAnalysis {
        int joint_index = 0;
        double max_displacement = 0;       // max movement between frames
        double avg_displacement = 0;
        double jitter_score = 0;           // high-frequency oscillation
        double pop_score = 0;              // sudden large displacements
        size_t pop_events = 0;             // count of sudden pops
        double smoothness = 0;             // 0 = jerky, 1 = smooth
        std::vector<size_t> anomaly_frames; // frames with suspicious movement
    };

    struct AnimationAnalysis {
        int frame_count = 0;
        int joint_count = 0;
        double duration = 0;
        std::vector<JointAnalysis> joints;
        double overall_smoothness = 0;
        double overall_jitter = 0;
        double temporal_coherence = 0;     // frame-to-frame consistency
        size_t total_anomalies = 0;
        bool has_freezing = false;         // joints stuck in place
        bool has_teleporting = false;      // joints jumping large distances
    };

    /// Analyze an animation track
    static AnimationAnalysis analyze(const AnimationTrack& track) {
        AnimationAnalysis result;
        result.frame_count = static_cast<int>(track.frames.size());
        if (track.frames.empty()) return result;

        result.joint_count = track.frames[0].joint_count;
        result.duration = track.frames.size() / track.frame_rate;

        // Per-joint analysis
        result.joints.resize(result.joint_count);
        for (int j = 0; j < result.joint_count; ++j) {
            result.joints[j] = analyzeJoint(track, j);
        }

        // Aggregate metrics
        double total_smooth = 0;
        double total_jitter = 0;
        for (auto& ja : result.joints) {
            total_smooth += ja.smoothness;
            total_jitter += ja.jitter_score;
            result.total_anomalies += ja.anomaly_frames.size();
            if (ja.pop_events > 0) result.has_teleporting = true;
        }
        result.overall_smoothness = result.joint_count > 0 ? total_smooth / result.joint_count : 0;
        result.overall_jitter = result.joint_count > 0 ? total_jitter / result.joint_count : 0;

        // Temporal coherence
        result.temporal_coherence = computeTemporalCoherence(track);

        // Freeze detection
        result.has_freezing = detectFreezing(track);

        return result;
    }

    /// Generate readable report
    static std::string report(const AnimationAnalysis& a) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(4);
        oss << "═══ Procedural Animation Validation ═══\n";
        oss << "  Frames:           " << a.frame_count << "\n";
        oss << "  Joints:           " << a.joint_count << "\n";
        oss << "  Duration:         " << a.duration << "s\n";
        oss << "  Overall Smooth:   " << a.overall_smoothness
            << (a.overall_smoothness < 0.5 ? " ⚠" : " ✓") << "\n";
        oss << "  Overall Jitter:   " << a.overall_jitter
            << (a.overall_jitter > 0.3 ? " ⚠" : " ✓") << "\n";
        oss << "  Temporal Coherence: " << a.temporal_coherence
            << (a.temporal_coherence < 0.7 ? " ⚠" : " ✓") << "\n";
        oss << "  Total Anomalies:  " << a.total_anomalies << "\n";
        oss << "  Freezing:         " << (a.has_freezing ? "⚠ YES" : "✓ none") << "\n";
        oss << "  Teleporting:      " << (a.has_teleporting ? "⚠ YES" : "✓ none") << "\n";

        // Per-joint details
        oss << "\n  Joint Details:\n";
        oss << "  Joint | Max Disp | Jitter | Pops | Smooth\n";
        oss << "  ------|----------|--------|------|-------\n";
        for (auto& j : a.joints) {
            oss << "  " << std::setw(5) << j.joint_index
                << " | " << std::setw(8) << j.max_displacement
                << " | " << std::setw(6) << j.jitter_score
                << " | " << std::setw(4) << j.pop_events
                << " | " << std::setw(5) << j.smoothness;
            if (j.jitter_score > 0.3 || j.pop_events > 0) oss << " ⚠";
            oss << "\n";
        }

        return oss.str();
    }

private:
    static JointAnalysis analyzeJoint(const AnimationTrack& track, int joint_idx) {
        JointAnalysis ja;
        ja.joint_index = joint_idx;

        if (track.frames.size() < 2) return ja;

        // Extract joint position across frames
        int stride = track.frames[0].joint_count * 3;
        std::vector<double> jx, jy, jz;
        for (auto& frame : track.frames) {
            jx.push_back(frame.positions[joint_idx * 3]);
            jy.push_back(frame.positions[joint_idx * 3 + 1]);
            jz.push_back(frame.positions[joint_idx * 3 + 2]);
        }

        // Frame-to-frame displacements
        std::vector<double> displacements;
        double max_disp = 0, sum_disp = 0;
        for (size_t f = 1; f < track.frames.size(); ++f) {
            double dx = jx[f] - jx[f - 1];
            double dy = jy[f] - jy[f - 1];
            double dz = jz[f] - jz[f - 1];
            double disp = std::sqrt(dx*dx + dy*dy + dz*dz);
            displacements.push_back(disp);
            if (disp > max_disp) max_disp = disp;
            sum_disp += disp;
        }
        ja.max_displacement = max_disp;
        ja.avg_displacement = displacements.empty() ? 0 :
            sum_disp / displacements.size();

        // Jitter: high-frequency oscillation = acceleration magnitude
        if (displacements.size() >= 3) {
            double jitter_sum = 0;
            for (size_t f = 2; f < displacements.size(); ++f) {
                double accel = std::abs(displacements[f] - displacements[f - 1]);
                jitter_sum += accel;
            }
            ja.jitter_score = displacements.size() > 1 ?
                jitter_sum / (displacements.size() - 2) / (ja.avg_displacement + 1e-10) : 0;
        }

        // Pop detection: displacements > 3x median
        if (!displacements.empty()) {
            auto sorted = displacements;
            std::sort(sorted.begin(), sorted.end());
            double median = sorted[sorted.size() / 2];
            double threshold = std::max(median * 3.0, 0.01);

            for (size_t f = 0; f < displacements.size(); ++f) {
                if (displacements[f] > threshold) {
                    ja.pop_events++;
                    ja.anomaly_frames.push_back(f);
                }
            }
        }
        ja.pop_score = displacements.empty() ? 0 :
            static_cast<double>(ja.pop_events) / displacements.size();

        // Smoothness
        ja.smoothness = 1.0 - std::min(1.0, ja.jitter_score + ja.pop_score);

        return ja;
    }

    static double computeTemporalCoherence(const AnimationTrack& track) {
        if (track.frames.size() < 2) return 1.0;

        // Compare whole-frame displacement variance
        std::vector<double> frame_diffs;
        for (size_t f = 1; f < track.frames.size(); ++f) {
            double diff = 0;
            for (size_t i = 0; i < track.frames[f].positions.size(); ++i) {
                double d = track.frames[f].positions[i] - track.frames[f - 1].positions[i];
                diff += d * d;
            }
            frame_diffs.push_back(std::sqrt(diff));
        }

        double mean = std::accumulate(frame_diffs.begin(), frame_diffs.end(), 0.0) / frame_diffs.size();
        double variance = 0;
        for (auto d : frame_diffs) variance += (d - mean) * (d - mean);
        variance /= frame_diffs.size();

        // Coherence = consistency of frame-to-frame differences
        double cv = mean > 1e-10 ? std::sqrt(variance) / mean : 0;
        return 1.0 / (1.0 + cv);
    }

    static bool detectFreezing(const AnimationTrack& track) {
        if (track.frames.size() < 10) return false;

        // Check if any joint has zero displacement for many consecutive frames
        for (int j = 0; j < track.frames[0].joint_count; ++j) {
            int freeze_count = 0;
            for (size_t f = 1; f < track.frames.size(); ++f) {
                double dx = track.frames[f].positions[j * 3] - track.frames[f - 1].positions[j * 3];
                double dy = track.frames[f].positions[j * 3 + 1] - track.frames[f - 1].positions[j * 3 + 1];
                double dz = track.frames[f].positions[j * 3 + 2] - track.frames[f - 1].positions[j * 3 + 2];

                if (dx*dx + dy*dy + dz*dz < 1e-12) {
                    freeze_count++;
                    if (freeze_count > 5) return true; // frozen for >5 frames
                } else {
                    freeze_count = 0;
                }
            }
        }
        return false;
    }
};

} // namespace nexus::utility::procgen
