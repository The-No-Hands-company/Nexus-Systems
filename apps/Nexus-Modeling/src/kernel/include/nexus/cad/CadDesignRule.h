#pragma once
#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadDataTables.h>
#include <cstdint>
#include <string>
#include <vector>

namespace nexus::cad {

enum class DesignRuleSeverity : uint8_t { Info, Warning, Error, Critical };
struct DesignRule { std::string id,description; DesignRuleSeverity severity=DesignRuleSeverity::Warning; bool enabled=true; };
struct DesignRuleViolation { std::string ruleId,message; DesignRuleSeverity severity; std::vector<parametric::FeatureId> relatedFeatures; };

class CadDesignChecker {
public:
    void addRule(const DesignRule& r);
    [[nodiscard]] std::vector<DesignRuleViolation> check(const CadDocument& doc) const noexcept;
    [[nodiscard]] static std::vector<DesignRule> builtinRules() noexcept;
    [[nodiscard]] const std::vector<DesignRule>& rules() const noexcept;
    [[nodiscard]] size_t errorCount(const std::vector<DesignRuleViolation>& v) const noexcept;
    [[nodiscard]] size_t warningCount(const std::vector<DesignRuleViolation>& v) const noexcept;
private: std::vector<DesignRule> m_rules;
};

}
