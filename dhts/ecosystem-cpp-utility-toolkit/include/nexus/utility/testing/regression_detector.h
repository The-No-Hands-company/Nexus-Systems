#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <unordered_map>
namespace nexus::utility::testing {
class RegressionDetector {
public:
    struct Benchmark { std::string name; std::chrono::microseconds baseline{0}; std::chrono::microseconds current{0}; double regressionPercent; };
    static RegressionDetector& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void setBaseline(const std::string& name, std::chrono::microseconds time);
    void recordResult(const std::string& name, std::chrono::microseconds time);
    Benchmark check(const std::string& name) const;
    std::vector<Benchmark> checkAll() const;
    std::vector<Benchmark> getRegressions(double thresholdPercent=10.0) const;
    size_t getRegressionCount() const;
    void clear();
private:
    RegressionDetector()=default; ~RegressionDetector()=default; bool enabled_=false;
    std::unordered_map<std::string,std::chrono::microseconds> baselines_;
    std::unordered_map<std::string,std::chrono::microseconds> results_;
};
} // namespace nexus::utility::testing