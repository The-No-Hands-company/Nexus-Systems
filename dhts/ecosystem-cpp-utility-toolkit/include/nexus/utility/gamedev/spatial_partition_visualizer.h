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

struct SpatialObject {
    uint64_t id = 0;
    Vector3 position;
};

class SpatialPartitionVisualizer {
public:
    static SpatialPartitionVisualizer& instance() {
        static SpatialPartitionVisualizer inst;
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
        objects_.clear();
    }

    void setBounds(const Vector3& min, const Vector3& max) {
        std::lock_guard<std::mutex> lock(mutex_);
        bounds_min_ = min;
        bounds_max_ = max;
    }

    void addObject(uint64_t id, const Vector3& pos) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& obj : objects_) {
            if (obj.id == id) {
                obj.position = pos;
                return;
            }
        }
        objects_.push_back({id, pos});
    }

    std::vector<uint64_t> queryRadius(const Vector3& center, double radius) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<uint64_t> result;
        double rsq = radius * radius;
        for (const auto& obj : objects_) {
            double dx = obj.position.x - center.x;
            double dy = obj.position.y - center.y;
            double dz = obj.position.z - center.z;
            if (dx * dx + dy * dy + dz * dz <= rsq) {
                result.push_back(obj.id);
            }
        }
        return result;
    }

    size_t getObjectCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return objects_.size();
    }

    Vector3 getBoundsMin() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return bounds_min_;
    }

    Vector3 getBoundsMax() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return bounds_max_;
    }

private:
    SpatialPartitionVisualizer() = default;
    ~SpatialPartitionVisualizer() = default;

    SpatialPartitionVisualizer(const SpatialPartitionVisualizer&) = delete;
    SpatialPartitionVisualizer& operator=(const SpatialPartitionVisualizer&) = delete;
    SpatialPartitionVisualizer(SpatialPartitionVisualizer&&) = delete;
    SpatialPartitionVisualizer& operator=(SpatialPartitionVisualizer&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    Vector3 bounds_min_;
    Vector3 bounds_max_;
    std::vector<SpatialObject> objects_;
};

} // namespace nexus::utility::gamedev
