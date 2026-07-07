#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

namespace nexus::utility::hardware {

class PcieErrorHandler {
public:
    using DeviceKey = std::tuple<uint8_t, uint8_t, uint8_t>;

    static PcieErrorHandler& instance() {
        static PcieErrorHandler inst;
        return inst;
    }

    void initialize(const std::string& = "") { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void shutdown() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; reset(); }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lock(mutex_); enabled_ = false; }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "PcieErrorHandler: " + std::to_string(device_errors_.size()) + " devices with errors, total=" +
               std::to_string(getTotalErrors()) + ", enabled=" + (enabled_ ? "true" : "false");
    }
    void reset() { std::lock_guard<std::mutex> lock(mutex_); device_errors_.clear(); }

    void recordError(uint8_t bus, uint8_t device, uint8_t function, const std::string& error_type) {
        std::lock_guard<std::mutex> lock(mutex_);
        device_errors_[{bus, device, function}].push_back(error_type);
    }

    std::vector<std::string> getDeviceErrors(uint8_t bus, uint8_t device) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> result;
        for (const auto& [key, errors] : device_errors_) {
            if (std::get<0>(key) == bus && std::get<1>(key) == device) {
                result.insert(result.end(), errors.begin(), errors.end());
            }
        }
        return result;
    }

    uint64_t getTotalErrors() const {
        std::lock_guard<std::mutex> lock(mutex_);
        uint64_t total = 0;
        for (const auto& [_, errors] : device_errors_) {
            total += errors.size();
        }
        return total;
    }

private:
    PcieErrorHandler() = default;
    ~PcieErrorHandler() = default;
    PcieErrorHandler(const PcieErrorHandler&) = delete;
    PcieErrorHandler& operator=(const PcieErrorHandler&) = delete;
    PcieErrorHandler(PcieErrorHandler&&) = delete;
    PcieErrorHandler& operator=(PcieErrorHandler&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::map<DeviceKey, std::vector<std::string>> device_errors_;
};

} // namespace nexus::utility::hardware
