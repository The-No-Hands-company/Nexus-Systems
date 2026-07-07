#pragma once
#include <string>
#include <vector>
#include <chrono>
namespace nexus::utility::graphics {
class ComputeKernelProfiler {
public:
    struct KernelStats { std::string name; size_t workGroupsX,workGroupsY,workGroupsZ; std::chrono::microseconds duration{0}; size_t invocations; };
    static ComputeKernelProfiler& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordKernel(const std::string& name, size_t gx,size_t gy,size_t gz,std::chrono::microseconds dur);
    std::vector<KernelStats> getKernels() const;
    std::chrono::microseconds getTotalTime() const;
    void clear();
private:
    ComputeKernelProfiler()=default; ~ComputeKernelProfiler()=default; bool enabled_=false;
    std::vector<KernelStats> kernels_;
};
} // namespace nexus::utility::graphics