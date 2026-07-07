#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <stdexcept>

namespace nexus::utility::hardware {

class ActuatorMonitor {
public:
    struct ActuatorState {
        std::string name;
        std::string type;
        double position = 0.0;
        bool active = false;
    };

    static ActuatorMonitor& instance() {
        static ActuatorMonitor inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; actuators_.clear(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "ActuatorMonitor: " + std::to_string(actuators_.size()) + " actuators registered, enabled=" + (enabled_ ? "true" : "false");
    }
    void reset() { std::lock_guard<std::mutex> lock(mutex_); actuators_.clear(); }

    void registerActuator(uint64_t id, const std::string& name, const std::string& type) {
        std::lock_guard<std::mutex> lock(mutex_);
        actuators_[id] = ActuatorState{name, type, 0.0, false};
    }

    void setPosition(uint64_t id, double pos) {
        std::lock_guard<std::mutex> lock(mutex_);
        actuators_[id].position = pos;
    }

    double getPosition(uint64_t id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actuators_.find(id);
        return (it != actuators_.end()) ? it->second.position : 0.0;
    }

    void setState(uint64_t id, bool active) {
        std::lock_guard<std::mutex> lock(mutex_);
        actuators_[id].active = active;
    }

    bool getState(uint64_t id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = actuators_.find(id);
        return (it != actuators_.end()) ? it->second.active : false;
    }

private:
    ActuatorMonitor() = default;
    ~ActuatorMonitor() = default;
    ActuatorMonitor(const ActuatorMonitor&) = delete;
    ActuatorMonitor& operator=(const ActuatorMonitor&) = delete;
    ActuatorMonitor(ActuatorMonitor&&) = delete;
    ActuatorMonitor& operator=(ActuatorMonitor&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<uint64_t, ActuatorState> actuators_;
};

} // namespace nexus::utility::hardware
