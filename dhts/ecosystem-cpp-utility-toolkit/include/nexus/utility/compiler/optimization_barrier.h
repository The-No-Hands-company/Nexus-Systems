#pragma once
#include <string>
#include <vector>
namespace nexus::utility::compiler {
class OptimizationBarrier {
public:
    struct BarrierRecord { std::string variable; std::string location; std::string reason; };
    static OptimizationBarrier& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordBarrier(const std::string& var, const std::string& loc, const std::string& reason);
    std::vector<BarrierRecord> getBarriers() const;
    size_t getBarrierCount() const;
    void clear();
private:
    OptimizationBarrier()=default; ~OptimizationBarrier()=default; bool enabled_=false;
    std::vector<BarrierRecord> barriers_;
};
} // namespace nexus::utility::compiler