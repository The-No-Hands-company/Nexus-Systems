#pragma once
#include <string>
#include <vector>
#include <unordered_map>
namespace nexus::utility::compiler {
class AbiCompatibilityChecker {
public:
    struct AbiVersion { std::string component; int version; bool compatible; std::string notes; };
    static AbiCompatibilityChecker& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void registerComponent(const std::string& name, int version);
    AbiVersion check(const std::string& name, int expectedVersion) const;
    std::vector<AbiVersion> checkAll(int expectedVersion) const;
    size_t componentCount() const;
    void clear();
private:
    AbiCompatibilityChecker()=default; ~AbiCompatibilityChecker()=default; bool enabled_=false;
    std::unordered_map<std::string,int> components_;
};
} // namespace nexus::utility::compiler