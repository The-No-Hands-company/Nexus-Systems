#pragma once
#include <string>
#include <vector>
#include <unordered_map>
namespace nexus::utility::compiler {
class MacroExpansionDebugger {
public:
    struct ExpansionStep { std::string macro; int depth; std::string result; };
    static MacroExpansionDebugger& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void startExpansion(const std::string& macro);
    void recordStep(const std::string& macro, int depth, const std::string& result);
    std::vector<ExpansionStep> getExpansion(const std::string& macro) const;
    size_t getMacroCount() const;
    size_t getMaxDepth(const std::string& macro) const;
    void clear();
private:
    MacroExpansionDebugger()=default; ~MacroExpansionDebugger()=default; bool enabled_=false;
    std::unordered_map<std::string,std::vector<ExpansionStep>> expansions_;
};
} // namespace nexus::utility::compiler