#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace nexus::utility::security {

class TimingAttackDetector {
public:
    struct TimingSample { std::string operation; std::chrono::nanoseconds elapsed; size_t inputSize; bool suspicious; };

    static TimingAttackDetector& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    void recordTiming(const std::string& op, std::chrono::nanoseconds elapsed, size_t inputSize);
    bool analyzeTimingVariance(const std::string& op, double maxVariancePercent = 20.0);
    std::vector<TimingSample> getSamples() const;
    size_t getSuspiciousCount() const;
    void setThreshold(double maxVariancePercent);
    void clear();

private:
    TimingAttackDetector() = default;
    ~TimingAttackDetector() = default;
    bool enabled_ = false;
    double maxVariance_ = 20.0;
    std::vector<TimingSample> samples_;
    size_t suspiciousCount_ = 0;
};
} // namespace nexus::utility::security
