#pragma once
#include <string>
#include <vector>
#include <source_location>
#include <chrono>

namespace nexus::utility::cpp26 {

/// Debug C++26 extended constexpr evaluation (void constexpr functions, constexpr allocations)
class ConstexprDebugger {
public:
    struct ConstexprTrace {
        std::string functionName;
        std::source_location location;
        bool evaluatedAtCompileTime;
        std::chrono::microseconds runtimeFallback{0};
    };

    static bool isConstantEvaluated() {
#if defined(__cpp_if_consteval) || defined(__cpp_consteval)
        return std::is_constant_evaluated();
#else
        return false;
#endif
    }

    void traceEvaluation(const std::string& func,
                         std::source_location loc = std::source_location::current()) {
        traces_.push_back({func, loc, isConstantEvaluated(), std::chrono::microseconds(0)});
    }

    void traceRuntimeFallback(const std::string& func, std::chrono::microseconds time) {
        for (auto& t : traces_) {
            if (t.functionName == func && !t.evaluatedAtCompileTime) {
                t.runtimeFallback = time; break;
            }
        }
    }

    std::vector<ConstexprTrace> getTraces() const { return traces_; }
    size_t compileTimeCount() const {
        size_t c = 0;
        for (auto& t : traces_) if (t.evaluatedAtCompileTime) c++;
        return c;
    }
    size_t runtimeFallbackCount() const { return traces_.size() - compileTimeCount(); }
    void clear() { traces_.clear(); }

private:
    std::vector<ConstexprTrace> traces_;
};
} // namespace nexus::utility::cpp26
