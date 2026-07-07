#pragma once

#include <string>
#include <vector>
#include <functional>
#include <source_location>
#include <cmath>
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

class PhysicsValidator {
public:
    static PhysicsValidator& instance() {
        static PhysicsValidator inst;
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
        penetration_count_ = 0;
        penetrations_.clear();
    }

    bool validateVelocity(double v, double max_v) const {
        return std::abs(v) <= max_v;
    }

    bool validatePosition(const Vector3& pos, const Vector3& bounds) const {
        return std::abs(pos.x) <= std::abs(bounds.x) &&
               std::abs(pos.y) <= std::abs(bounds.y) &&
               std::abs(pos.z) <= std::abs(bounds.z);
    }

    void recordPenetration(const std::string& objA, const std::string& objB) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++penetration_count_;
        std::string key = objA < objB ? objA + "|" + objB : objB + "|" + objA;
        ++penetrations_[key];
    }

    size_t getPenetrationCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return penetration_count_;
    }

    size_t getPenetrationCount(const std::string& objA, const std::string& objB) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string key = objA < objB ? objA + "|" + objB : objB + "|" + objA;
        auto it = penetrations_.find(key);
        return it != penetrations_.end() ? it->second : 0;
    }

private:
    PhysicsValidator() = default;
    ~PhysicsValidator() = default;

    PhysicsValidator(const PhysicsValidator&) = delete;
    PhysicsValidator& operator=(const PhysicsValidator&) = delete;
    PhysicsValidator(PhysicsValidator&&) = delete;
    PhysicsValidator& operator=(PhysicsValidator&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    size_t penetration_count_ = 0;
    std::map<std::string, size_t> penetrations_;
};

} // namespace nexus::utility::gamedev
