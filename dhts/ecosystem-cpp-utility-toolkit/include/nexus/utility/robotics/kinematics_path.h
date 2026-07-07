#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <limits>

namespace nexus::utility::robotics {

class KinematicsSolverValidator {
public:
    struct JointConfig { std::vector<double> angles; bool valid; };
    struct Pose { double x,y,z,roll,pitch,yaw; };

    struct ValidationResult {
        size_t singularities = 0;
        size_t out_of_bounds = 0;
        size_t unreachable = 0;
        double avg_position_error = 0;
        double max_position_error = 0;
        std::vector<double> joint_range_usage; // % of joint limits used
        bool is_well_conditioned = true;
    };

    static ValidationResult validate(
        const std::vector<JointConfig>& joint_path,
        const std::vector<Pose>& computed_poses,
        const std::vector<Pose>& desired_poses,
        const std::vector<std::pair<double,double>>& joint_limits) {

        ValidationResult vr;
        vr.joint_range_usage.resize(joint_limits.size(), 0);

        double pos_err_sum = 0;
        for (size_t i = 0; i < std::min(computed_poses.size(), desired_poses.size()); ++i) {
            const auto& cp = computed_poses[i];
            const auto& dp = desired_poses[i];
            double err = std::sqrt((cp.x-dp.x)*(cp.x-dp.x)+(cp.y-dp.y)*(cp.y-dp.y)+(cp.z-dp.z)*(cp.z-dp.z));
            pos_err_sum += err;
            vr.max_position_error = std::max(vr.max_position_error, err);
        }
        vr.avg_position_error = pos_err_sum / std::max(size_t(1), desired_poses.size());

        for (auto& jc : joint_path) {
            if (!jc.valid) vr.singularities++;
            for (size_t j = 0; j < std::min(jc.angles.size(), joint_limits.size()); ++j) {
                if (jc.angles[j] < joint_limits[j].first || jc.angles[j] > joint_limits[j].second)
                    vr.out_of_bounds++;
                double range = joint_limits[j].second - joint_limits[j].first;
                double usage = range>0 ? std::abs(jc.angles[j])/range : 0;
                vr.joint_range_usage[j] = std::max(vr.joint_range_usage[j], usage);
            }
        }
        vr.is_well_conditioned = vr.singularities == 0;
        return vr;
    }

    static std::string report(const ValidationResult& vr) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(3);
        oss << "═══ Kinematics Validation ═══\n";
        oss << "  Singularities:  " << vr.singularities << (vr.singularities>0?" ⚠":" ✓") << "\n";
        oss << "  Out-of-bounds:  " << vr.out_of_bounds << (vr.out_of_bounds>0?" ⚠":" ✓") << "\n";
        oss << "  Pos Error Avg:  " << vr.avg_position_error << "\n";
        oss << "  Pos Error Max:  " << vr.max_position_error << "\n";
        oss << "  Conditioned:    " << (vr.is_well_conditioned?"✓":"⚠") << "\n";
        return oss.str();
    }
};

/// Path planner debugger (A*/RRT validation)
class PathPlannerDebugger {
public:
    struct PathPoint { double x,y; };
    struct Obstacle { double x,y,radius; };

    struct PathAnalysis {
        double total_length = 0;
        double straightness = 0; // path_len / euclidean_dist
        size_t collision_count = 0;
        size_t sharp_turns = 0; // > 120 degrees
        double min_clearance = std::numeric_limits<double>::max();
        double avg_step_size = 0;
        size_t path_points = 0;
    };

    static PathAnalysis analyze(const std::vector<PathPoint>& path,
                                 const std::vector<Obstacle>& obstacles) {
        PathAnalysis pa;
        pa.path_points = path.size();
        if (path.size() < 2) return pa;

        double total = 0, step_sum = 0;
        for (size_t i = 1; i < path.size(); ++i) {
            double dx = path[i].x - path[i-1].x, dy = path[i].y - path[i-1].y;
            double step = std::sqrt(dx*dx+dy*dy);
            total += step;
            step_sum += step;

            // Sharp turn detection
            if (i >= 2) {
                double dx1 = path[i-1].x-path[i-2].x, dy1 = path[i-1].y-path[i-2].y;
                double dx2 = dx, dy2 = dy;
                double dot = dx1*dx2 + dy1*dy2;
                double l1 = std::sqrt(dx1*dx1+dy1*dy1), l2 = step;
                double angle = (l1*l2>0) ? std::acos(dot/(l1*l2))*180/M_PI : 0;
                if (angle > 120) pa.sharp_turns++;
            }
        }
        pa.total_length = total;
        pa.avg_step_size = step_sum/(path.size()-1);

        double start_end = std::sqrt(
            (path.back().x-path[0].x)*(path.back().x-path[0].x) +
            (path.back().y-path[0].y)*(path.back().y-path[0].y));
        pa.straightness = total > 0 ? start_end/total : 1;

        // Collision check
        for (auto& p : path) {
            for (auto& obs : obstacles) {
                double d = std::sqrt((p.x-obs.x)*(p.x-obs.x)+(p.y-obs.y)*(p.y-obs.y));
                pa.min_clearance = std::min(pa.min_clearance, d);
                if (d < obs.radius) pa.collision_count++;
            }
        }

        return pa;
    }

    static std::string report(const PathAnalysis& pa) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(2);
        oss << "═══ Path Planner Debug ═══\n";
        oss << "  Length:        " << pa.total_length << "\n";
        oss << "  Straightness:  " << pa.straightness << (pa.straightness<0.5?" ⚠ winding":" ✓") << "\n";
        oss << "  Collisions:    " << pa.collision_count << (pa.collision_count>0?" ⚠":" ✓") << "\n";
        oss << "  Sharp Turns:   " << pa.sharp_turns << (pa.sharp_turns>2?" ⚠":" ✓") << "\n";
        oss << "  Min Clearance: " << pa.min_clearance << "\n";
        return oss.str();
    }
};

} // namespace nexus::utility::robotics
