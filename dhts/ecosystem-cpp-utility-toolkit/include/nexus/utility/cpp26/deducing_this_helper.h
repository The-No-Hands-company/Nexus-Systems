#pragma once
#include <string>
#include <vector>
#include <typeinfo>
#include <type_traits>
#include <source_location>

namespace nexus::utility::cpp26 {

/// Debug C++26 explicit object parameters (deducing this refinements)
class DeducingThisHelper {
public:
    struct DeductionInfo {
        std::string functionName;
        std::string deducedType;
        bool isConst;
        bool isLValue;
        bool isRValue;
        std::source_location callSite;
    };

    template<typename Self, typename Func>
    void traceCall(const std::string& funcName, Self&& self, Func&&,
                   std::source_location loc = std::source_location::current()) {
        DeductionInfo info;
        info.functionName = funcName;
        info.deducedType = typeid(self).name();
        info.isConst = std::is_const_v<std::remove_reference_t<Self>>;
        info.isLValue = std::is_lvalue_reference_v<Self&&>;
        info.isRValue = std::is_rvalue_reference_v<Self&&>;
        info.callSite = loc;
        traces_.push_back(info);
    }

    void traceDeduction(const std::string& func, const std::string& type,
                        bool isConst, bool isLValue, bool isRValue,
                        std::source_location loc = std::source_location::current()) {
        traces_.push_back({func, type, isConst, isLValue, isRValue, loc});
    }

    std::vector<DeductionInfo> getTraces() const { return traces_; }
    size_t traceCount() const { return traces_.size(); }
    void clear() { traces_.clear(); }

private:
    std::vector<DeductionInfo> traces_;
};
} // namespace nexus::utility::cpp26
