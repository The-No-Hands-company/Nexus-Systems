#pragma once
#include <string>
#include <vector>
#include <functional>
#include <chrono>
#include <unordered_map>
namespace nexus::utility::testing {
class FuzzerHarness {
public:
    using FuzzFn = std::function<void(const std::vector<uint8_t>&)>;
    struct FuzzResult { std::string target; size_t iterations; size_t crashes; size_t hangs; std::chrono::milliseconds duration{0}; };
    static FuzzerHarness& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerTarget(const std::string& name, FuzzFn fn);
    FuzzResult runFuzzer(const std::string& name, size_t iterations, std::chrono::milliseconds timeout=std::chrono::milliseconds(5000));
    std::vector<FuzzResult> getResults() const;
    size_t getTotalCrashes() const;
    void clear();
private:
    FuzzerHarness()=default; ~FuzzerHarness()=default; bool enabled_=false;
    std::unordered_map<std::string,FuzzFn> targets_;
    std::vector<FuzzResult> results_;
};
} // namespace nexus::utility::testing