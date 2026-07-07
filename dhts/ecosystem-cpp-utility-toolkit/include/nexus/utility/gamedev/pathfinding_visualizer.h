#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <cmath>
#include <mutex>

namespace nexus::utility::gamedev {

#ifndef NEXUS_GAMEDEV_VECTOR3_DEFINED
#define NEXUS_GAMEDEV_VECTOR3_DEFINED
struct Vector3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};
#endif

struct Obstacle {
    Vector3 position;
    double radius = 0.0;
};

class PathfindingVisualizer {
public:
    static PathfindingVisualizer& instance() {
        static PathfindingVisualizer inst;
        return inst;
    }

    void initialize(const std::string& config = "") { config_ = config; enabled_ = true; }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    std::string getStatus() const { return enabled_ ? "active" : "inactive"; }
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        waypoints_.clear();
        obstacles_.clear();
    }

    void setPath(const std::vector<Vector3>& waypoints) {
        std::lock_guard<std::mutex> lock(mutex_);
        waypoints_ = waypoints;
    }

    double getPathLength() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (waypoints_.size() < 2) return 0.0;
        double total = 0.0;
        for (size_t i = 1; i < waypoints_.size(); ++i) {
            total += distance(waypoints_[i - 1], waypoints_[i]);
        }
        return total;
    }

    size_t getWaypointCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return waypoints_.size();
    }

    void addObstacle(const Vector3& pos, double radius) {
        std::lock_guard<std::mutex> lock(mutex_);
        obstacles_.push_back({pos, radius});
    }

    bool isObstructed(const Vector3& pos) const {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& obs : obstacles_) {
            if (distance(pos, obs.position) <= obs.radius) return true;
        }
        return false;
    }

    const std::vector<Vector3>& getWaypoints() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return waypoints_;
    }

private:
    PathfindingVisualizer() = default;
    ~PathfindingVisualizer() = default;

    PathfindingVisualizer(const PathfindingVisualizer&) = delete;
    PathfindingVisualizer& operator=(const PathfindingVisualizer&) = delete;
    PathfindingVisualizer(PathfindingVisualizer&&) = delete;
    PathfindingVisualizer& operator=(PathfindingVisualizer&&) = delete;

    double distance(const Vector3& a, const Vector3& b) const {
        double dx = a.x - b.x;
        double dy = a.y - b.y;
        double dz = a.z - b.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<Vector3> waypoints_;
    std::vector<Obstacle> obstacles_;
};

} // namespace nexus::utility::gamedev
