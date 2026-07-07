#pragma once
#include <string>
#include <vector>
namespace nexus::utility::compiler {
class InlineDecisionReporter {
public:
    struct InlineDecision { std::string function; bool inlined; std::string reason; size_t size; };
    static InlineDecisionReporter& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordDecision(const std::string& func, bool inlined, const std::string& reason, size_t size=0);
    std::vector<InlineDecision> getDecisions() const;
    size_t getInlinedCount() const;
    size_t getNotInlinedCount() const;
    double getInliningRate() const;
    void clear();
private:
    InlineDecisionReporter()=default; ~InlineDecisionReporter()=default; bool enabled_=false;
    std::vector<InlineDecision> decisions_;
    size_t inlined_=0,notInlined_=0;
};
} // namespace nexus::utility::compiler