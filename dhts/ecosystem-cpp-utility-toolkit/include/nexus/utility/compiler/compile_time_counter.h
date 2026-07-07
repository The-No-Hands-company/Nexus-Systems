#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::compiler {
class CompileTimeCounter {
public:
    struct CompileUnit { std::string file; std::chrono::milliseconds time{0}; size_t lines; size_t templates; };
    static CompileTimeCounter& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordUnit(const std::string& file, std::chrono::milliseconds time, size_t lines, size_t templates=0);
    CompileUnit getUnit(const std::string& file) const;
    std::vector<CompileUnit> getSlowest(size_t topN=10) const;
    std::chrono::milliseconds getTotalTime() const;
    size_t getUnitCount() const;
    void clear();
private:
    CompileTimeCounter()=default; ~CompileTimeCounter()=default; bool enabled_=false;
    std::vector<CompileUnit> units_;
};
} // namespace nexus::utility::compiler