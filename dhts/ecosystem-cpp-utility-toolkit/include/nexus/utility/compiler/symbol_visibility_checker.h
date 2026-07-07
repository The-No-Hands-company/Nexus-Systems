#pragma once
#include <string>
#include <vector>
#include <unordered_map>
namespace nexus::utility::compiler {
class SymbolVisibilityChecker {
public:
    enum class Visibility { Default, Hidden, Internal, Protected };
    struct SymbolInfo { std::string name; Visibility visibility; std::string library; };
    static SymbolVisibilityChecker& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerSymbol(const std::string& name, Visibility vis, const std::string& lib);
    SymbolInfo getSymbol(const std::string& name) const;
    std::vector<SymbolInfo> getSymbolsByLib(const std::string& lib) const;
    std::vector<SymbolInfo> getByVisibility(Visibility vis) const;
    size_t getSymbolCount() const;
    void clear();
private:
    SymbolVisibilityChecker()=default; ~SymbolVisibilityChecker()=default; bool enabled_=false;
    std::unordered_map<std::string,SymbolInfo> symbols_;
};
} // namespace nexus::utility::compiler