#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <source_location>

namespace nexus::utility::cpp26 {

class ContractsChecker {
public:
    enum class ContractKind { Pre, Post, Assert };

    struct ContractViolation {
        ContractKind kind;
        std::string message;
        std::source_location location;
        std::chrono::system_clock::time_point timestamp;
    };

    void recordViolation(ContractKind kind, const std::string& msg,
                         std::source_location loc = std::source_location::current()) {
        violations_.push_back({kind, msg, loc, std::chrono::system_clock::now()});
    }

    std::vector<ContractViolation> getViolations() const { return violations_; }

    std::vector<ContractViolation> getViolationsByKind(ContractKind kind) const {
        std::vector<ContractViolation> r;
        for (const auto& v : violations_)
            if (v.kind == kind) r.push_back(v);
        return r;
    }

    size_t violationCount() const { return violations_.size(); }
    size_t preViolations() const { return countByKind(ContractKind::Pre); }
    size_t postViolations() const { return countByKind(ContractKind::Post); }
    size_t assertViolations() const { return countByKind(ContractKind::Assert); }

    void clear() { violations_.clear(); }

private:
    size_t countByKind(ContractKind kind) const {
        size_t c = 0;
        for (const auto& v : violations_) if (v.kind == kind) c++;
        return c;
    }
    std::vector<ContractViolation> violations_;
};

} // namespace nexus::utility::cpp26
