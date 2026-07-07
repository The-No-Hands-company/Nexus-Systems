#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace nexus::utility::security {

class ConstantTimeValidator {
public:
    struct TimingResult { std::string operation; std::chrono::nanoseconds minTime; std::chrono::nanoseconds maxTime; double variance; bool isConstantTime; };

    static ConstantTimeValidator& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    void startMeasurement(const std::string& op);
    void recordMeasurement(const std::string& op, std::chrono::nanoseconds time);
    TimingResult finishMeasurement(const std::string& op, double maxVariation = 0.10);
    static bool constantTimeCompare(const uint8_t* a, const uint8_t* b, size_t len);
    std::vector<TimingResult> getResults() const;
    void clear();

private:
    ConstantTimeValidator() = default;
    ~ConstantTimeValidator() = default;
    bool enabled_ = false;
    std::vector<TimingResult> results_;
};
} // namespace nexus::utility::security
