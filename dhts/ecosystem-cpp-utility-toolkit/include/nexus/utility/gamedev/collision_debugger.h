#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <map>
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

struct CollisionRecord {
    std::string object_a;
    std::string object_b;
    Vector3 point;
};

class CollisionDebugger {
public:
    static CollisionDebugger& instance() {
        static CollisionDebugger inst;
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
        collisions_.clear();
    }
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        collisions_.clear();
    }

    void recordCollision(const std::string& objA, const std::string& objB, const Vector3& point) {
        std::lock_guard<std::mutex> lock(mutex_);
        collisions_.push_back({objA, objB, point});
        collision_map_[objA].push_back(collisions_.size() - 1);
        collision_map_[objB].push_back(collisions_.size() - 1);
    }

    size_t getCollisionCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return collisions_.size();
    }

    std::vector<CollisionRecord> getCollisionsFor(const std::string& obj) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<CollisionRecord> result;
        auto it = collision_map_.find(obj);
        if (it == collision_map_.end()) return result;
        for (size_t idx : it->second) {
            result.push_back(collisions_[idx]);
        }
        return result;
    }

private:
    CollisionDebugger() = default;
    ~CollisionDebugger() = default;

    CollisionDebugger(const CollisionDebugger&) = delete;
    CollisionDebugger& operator=(const CollisionDebugger&) = delete;
    CollisionDebugger(CollisionDebugger&&) = delete;
    CollisionDebugger& operator=(CollisionDebugger&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<CollisionRecord> collisions_;
    std::map<std::string, std::vector<size_t>> collision_map_;
};

} // namespace nexus::utility::gamedev
