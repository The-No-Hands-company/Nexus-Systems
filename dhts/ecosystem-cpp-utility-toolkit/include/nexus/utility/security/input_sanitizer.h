#pragma once
#include <string>
#include <vector>
#include <functional>

namespace nexus::utility::security {

class InputSanitizer {
public:
    using SanitizerFn = std::function<std::string(const std::string&)>;

    static InputSanitizer& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    std::string sanitize(const std::string& input);
    void addSanitizer(const std::string& name, SanitizerFn fn);
    size_t getSanitizedCount() const;
    static std::string stripHtml(const std::string& input);
    static std::string stripControlChars(const std::string& input);
    static std::string truncate(const std::string& input, size_t maxLen = 1024);
    void clear();

private:
    InputSanitizer() = default;
    ~InputSanitizer() = default;
    bool enabled_ = false;
    size_t sanitizedCount_ = 0;
    std::vector<std::pair<std::string, SanitizerFn>> sanitizers_;
};
} // namespace nexus::utility::security
