#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <algorithm>
#include <numeric>
#include <iomanip>

namespace nexus::utility::robotics {

/// Sensor fusion quality analyzer (IMU + GPS + odometry)
class SensorFusionQuality {
public:
    struct SensorReading { double value; double variance; std::string source; };
    struct FusedEstimate { double value; double covariance; };

    struct FusionAnalysis {
        double kalman_gain_avg = 0;
        double filter_consistency = 0; // innovation whiteness
        double divergence_risk = 0;
        double estimation_error_reduction = 0; // % improvement over raw
        std::vector<double> innovation_sequence;
    };

    static FusionAnalysis analyze(const std::vector<SensorReading>& readings,
                                    const std::vector<FusedEstimate>& estimates,
                                    double ground_truth) {
        FusionAnalysis fa;
        if (readings.empty() || estimates.empty()) return fa;

        double raw_err_sum = 0, fused_err_sum = 0;
        for (auto& r : readings) raw_err_sum += std::abs(r.value - ground_truth);
        for (auto& e : estimates) fused_err_sum += std::abs(e.value - ground_truth);
        double raw_avg = raw_err_sum/readings.size(), fused_avg = fused_err_sum/estimates.size();
        fa.estimation_error_reduction = raw_avg>0 ? (1-fused_avg/raw_avg)*100 : 0;

        for (size_t i=1; i<estimates.size(); ++i) {
            double innov = estimates[i].value - estimates[i-1].value;
            fa.innovation_sequence.push_back(innov);
        }

        double innov_mean = std::accumulate(fa.innovation_sequence.begin(), fa.innovation_sequence.end(), 0.0) / fa.innovation_sequence.size();
        double innov_var = 0;
        for (auto x : fa.innovation_sequence) innov_var += (x-innov_mean)*(x-innov_mean);
        innov_var /= fa.innovation_sequence.size();
        fa.filter_consistency = 1.0 / (1.0 + std::sqrt(innov_var));
        fa.divergence_risk = innov_var > 1.0 ? std::min(1.0, (innov_var-1.0)/5.0) : 0;

        return fa;
    }

    static std::string report(const FusionAnalysis& fa) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(2);
        oss << "═══ Sensor Fusion Quality ═══\n";
        oss << "  Error Reduction: " << fa.estimation_error_reduction << "%\n";
        oss << "  Consistency:     " << fa.filter_consistency << (fa.filter_consistency<0.5?" ⚠":" ✓") << "\n";
        oss << "  Divergence Risk: " << fa.divergence_risk << (fa.divergence_risk>0.3?" ⚠":" ✓") << "\n";
        return oss.str();
    }
};

/// Motor PID tuner and analyzer
class MotorPidTuner {
public:
    struct PidGains { double kp, ki, kd; };
    struct StepResponse { double rise_time, overshoot_pct, settling_time, steady_state_error; };

    static StepResponse analyze(const std::vector<double>& response, double setpoint, double dt) {
        StepResponse sr;
        if (response.empty()) return sr;
        double final_val = response.back();
        sr.steady_state_error = std::abs(final_val - setpoint);

        double max_val = *std::max_element(response.begin(), response.end());
        sr.overshoot_pct = setpoint>0 ? std::max(0.0, (max_val-setpoint)/setpoint*100) : 0;

        // Rise time: 10% to 90%
        double lo = setpoint*0.1, hi = setpoint*0.9;
        size_t t10=0, t90=0;
        for (size_t i=0; i<response.size(); ++i) {
            if (response[i]>=lo && t10==0) t10=i;
            if (response[i]>=hi && t90==0) t90=i;
        }
        sr.rise_time = (t90-t10)*dt;

        // Settling time: within 2% of setpoint
        double band = setpoint*0.02;
        for (int i=response.size()-1; i>=0; --i) {
            if (std::abs(response[i]-setpoint) > band) { sr.settling_time = (i+1)*dt; break; }
        }

        return sr;
    }

    /// Ziegler-Nichols tuning
    static PidGains zieglerNichols(double ku, double tu) {
        return {0.6*ku, 1.2*ku/tu, 0.075*ku*tu};
    }

    static std::string report(const StepResponse& sr) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(2);
        oss << "═══ PID Step Response ═══\n";
        oss << "  Rise Time:    " << sr.rise_time << "s\n";
        oss << "  Overshoot:    " << sr.overshoot_pct << "%" << (sr.overshoot_pct>20?" ⚠":" ✓") << "\n";
        oss << "  Settling:     " << sr.settling_time << "s\n";
        oss << "  Steady-State: " << sr.steady_state_error << (sr.steady_state_error>0.05?" ⚠":" ✓") << "\n";
        return oss.str();
    }
};

/// Odometry drift analyzer
class OdometryDriftAnalyzer {
public:
    struct OdometryRecord { double x,y,theta; double timestamp; };

    struct DriftAnalysis {
        double total_distance = 0;
        double translation_drift = 0; // drift per meter
        double rotation_drift = 0;    // drift per radian
        double avg_error = 0;
        bool has_significant_drift = false;
    };

    static DriftAnalysis analyze(const std::vector<OdometryRecord>& odom,
                                   const std::vector<OdometryRecord>& ground_truth) {
        DriftAnalysis da;
        auto n = std::min(odom.size(), ground_truth.size());
        if (n < 2) return da;

        double err_sum = 0, dist_sum = 0, rot_sum = 0;
        for (size_t i=1; i<n; ++i) {
            double dx = odom[i].x - ground_truth[i].x, dy = odom[i].y - ground_truth[i].y;
            err_sum += std::sqrt(dx*dx+dy*dy);
            dist_sum += std::sqrt((ground_truth[i].x-ground_truth[i-1].x)*(ground_truth[i].x-ground_truth[i-1].x) + (ground_truth[i].y-ground_truth[i-1].y)*(ground_truth[i].y-ground_truth[i-1].y));
            rot_sum += std::abs(ground_truth[i].theta - ground_truth[i-1].theta);
        }
        da.total_distance = dist_sum;
        da.avg_error = err_sum / (n-1);
        da.translation_drift = dist_sum>0 ? err_sum/dist_sum : 0;
        da.rotation_drift = rot_sum>0 ? err_sum/rot_sum : 0;
        da.has_significant_drift = da.translation_drift > 0.05;
        return da;
    }

    static std::string report(const DriftAnalysis& da) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(4);
        oss << "═══ Odometry Drift ═══\n";
        oss << "  Distance:       " << da.total_distance << "m\n";
        oss << "  Trans Drift:    " << da.translation_drift << "/m" << (da.has_significant_drift?" ⚠":" ✓") << "\n";
        oss << "  Rot Drift:      " << da.rotation_drift << "/rad\n";
        oss << "  Avg Error:      " << da.avg_error << "m\n";
        return oss.str();
    }
};

/// Collision detector validator
class CollisionDetectorValidator {
public:
    struct BBox { double min_x,min_y,min_z, max_x,max_y,max_z; };

    struct CollisionResult {
        size_t false_positives = 0, false_negatives = 0;
        double precision = 1, recall = 1, f1 = 1;
    };

    static CollisionResult validate(const std::vector<std::pair<BBox,BBox>>& predicted_collisions,
                                      const std::vector<std::pair<BBox,BBox>>& actual_collisions) {
        CollisionResult cr;
        size_t tp=0, fp=0, fn=0;
        for (auto& p : predicted_collisions) {
            bool is_actual = false;
            for (auto& a : actual_collisions)
                if (boxesEqual(p,a)) { is_actual=true; tp++; break; }
            if (!is_actual) fp++;
        }
        fn = actual_collisions.size() - tp;
        cr.false_positives=fp; cr.false_negatives=fn;
        cr.precision = (tp+fp)>0 ? (double)tp/(tp+fp) : 0;
        cr.recall = (tp+fn)>0 ? (double)tp/(tp+fn) : 0;
        cr.f1 = (cr.precision+cr.recall)>0 ? 2*cr.precision*cr.recall/(cr.precision+cr.recall) : 0;
        return cr;
    }

    static std::string report(const CollisionResult& cr) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(2);
        oss << "═══ Collision Detection ═══\n";
        oss << "  Precision: " << cr.precision << " | Recall: " << cr.recall << " | F1: " << cr.f1 << "\n";
        oss << "  FP: " << cr.false_positives << " | FN: " << cr.false_negatives << "\n";
        return oss.str();
    }

private:
    static bool boxesEqual(const std::pair<BBox,BBox>& a, const std::pair<BBox,BBox>& b, double eps=0.01) {
        auto eq=[eps](const BBox& x,const BBox& y){ return std::abs(x.min_x-y.min_x)<eps&&std::abs(x.min_y-y.min_y)<eps&&std::abs(x.min_z-y.min_z)<eps&&std::abs(x.max_x-y.max_x)<eps&&std::abs(x.max_y-y.max_y)<eps&&std::abs(x.max_z-y.max_z)<eps; };
        return (eq(a.first,b.first)&&eq(a.second,b.second))||(eq(a.first,b.second)&&eq(a.second,b.first));
    }
};

/// Joint limit monitor
class JointLimitMonitor {
public:
    struct JointState { double position, velocity, torque; };

    struct LimitStats {
        double position_margin = 1, velocity_margin = 1, torque_margin = 1;
        bool position_approaching = false, velocity_exceeded = false, torque_exceeded = false;
    };

    static LimitStats check(const JointState& state, double max_pos, double max_vel, double max_torque) {
        LimitStats ls;
        ls.position_margin = max_pos>0 ? 1-std::abs(state.position)/max_pos : 0;
        ls.velocity_margin = max_vel>0 ? 1-std::abs(state.velocity)/max_vel : 0;
        ls.torque_margin = max_torque>0 ? 1-std::abs(state.torque)/max_torque : 0;
        ls.position_approaching = ls.position_margin < 0.1;
        ls.velocity_exceeded = ls.velocity_margin < 0;
        ls.torque_exceeded = ls.torque_margin < 0;
        return ls;
    }

    static std::string report(const LimitStats& ls) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(2);
        oss << "═══ Joint Limits ═══\n";
        oss << "  Position: " << (ls.position_margin*100) << "%" << (ls.position_approaching?" ⚠":" ✓") << "\n";
        oss << "  Velocity: " << (ls.velocity_margin*100) << "%" << (ls.velocity_exceeded?" ⚠":" ✓") << "\n";
        oss << "  Torque:   " << (ls.torque_margin*100) << "%" << (ls.torque_exceeded?" ⚠":" ✓") << "\n";
        return oss.str();
    }
};

} // namespace nexus::utility::robotics
