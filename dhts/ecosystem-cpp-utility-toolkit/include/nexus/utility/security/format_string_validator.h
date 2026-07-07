#pragma once
#include <string>
#include <vector>
#include <regex>

namespace nexus::utility::security {

class FormatStringValidator {
public:
    struct FmtResult { bool safe = true; std::string input; std::vector<std::string> foundSpecifiers; size_t dangerousCount = 0; };

    static FormatStringValidator& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    FmtResult validate(const std::string& input);
    bool isSafe(const std::string& input);
    size_t getTotalValidated() const;
    size_t getDangerousCount() const;
    void clear();

private:
    FormatStringValidator() = default;
    ~FormatStringValidator() = default;
    bool enabled_ = false;
    size_t totalValidated_ = 0;
    size_t dangerousCount_ = 0;
    std::regex specifierPattern_{R"(%[\-+ #0]*(\d+|\*)?(\.\d+)?[hlLztj]?[diuoxXfFeEgGaAcspn])"};
    std::regex dangerousPattern_{R"(%[\-+ #0]*[n%])"};
};
} // namespace nexus::utility::security
