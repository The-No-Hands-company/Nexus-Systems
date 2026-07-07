#!/usr/bin/env python3
"""Flesh out all 11 security TODO skeleton tools."""
import os

BASE = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/dhts/dht_nexus_debug"
INC = f"{BASE}/include/nexus/utility/security"
SRC = f"{BASE}/src/utility/security"
os.makedirs(SRC, exist_ok=True)

def w(path, content):
    with open(path, 'w') as f: f.write(content)

# 1. sql_injection_detector
w(f"{INC}/sql_injection_detector.h", '''#pragma once
#include <string>
#include <vector>
#include <regex>
#include <unordered_set>

namespace nexus::utility::security {

class SqlInjectionDetector {
public:
    struct DetectionResult { bool suspicious = false; std::string pattern; std::string input; size_t position; };

    static SqlInjectionDetector& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    DetectionResult scan(const std::string& input);
    void addCustomPattern(const std::string& pattern);
    size_t getDetectionCount() const;
    std::vector<DetectionResult> getHistory() const;
    void clear();

private:
    SqlInjectionDetector() = default;
    ~SqlInjectionDetector() = default;
    bool enabled_ = false;
    size_t detectionCount_ = 0;
    std::vector<std::regex> patterns_;
    std::vector<DetectionResult> history_;
    void buildDefaultPatterns();
};
} // namespace nexus::utility::security
''')

w(f"{SRC}/sql_injection_detector.cpp", '''#include "nexus/utility/security/sql_injection_detector.h"
namespace nexus::utility::security {
SqlInjectionDetector& SqlInjectionDetector::instance() { static SqlInjectionDetector i; return i; }
void SqlInjectionDetector::initialize() { enabled_ = true; buildDefaultPatterns(); }
void SqlInjectionDetector::shutdown() { enabled_ = false; patterns_.clear(); history_.clear(); }
bool SqlInjectionDetector::isEnabled() const { return enabled_; }

void SqlInjectionDetector::buildDefaultPatterns() {
    patterns_.push_back(std::regex(R"((\\bUNION\\b.*\\bSELECT\\b))", std::regex::icase));
    patterns_.push_back(std::regex(R"((\\bDROP\\s+TABLE\\b))", std::regex::icase));
    patterns_.push_back(std::regex(R"((\\bINSERT\\s+INTO\\b))", std::regex::icase));
    patterns_.push_back(std::regex(R"((\\bDELETE\\s+FROM\\b))", std::regex::icase));
    patterns_.push_back(std::regex(R"((\\bOR\\s+['\"]\\w+['\"]\\s*=\\s*['\"]\\w+['\"])", std::regex::icase));
    patterns_.push_back(std::regex(R"((--[^\\n]*$))", std::regex::icase));
    patterns_.push_back(std::regex(R"((/\\*.*\\*/))"));
    patterns_.push_back(std::regex(R"((\\bEXEC\\b.*\\bxp_cmdshell\\b))", std::regex::icase));
}

SqlInjectionDetector::DetectionResult SqlInjectionDetector::scan(const std::string& input) {
    DetectionResult r; r.input = input;
    for (const auto& pattern : patterns_) {
        std::smatch match;
        if (std::regex_search(input, match, pattern)) {
            r.suspicious = true; r.pattern = match.str(); r.position = match.position();
            detectionCount_++; history_.push_back(r); return r;
        }
    }
    return r;
}

void SqlInjectionDetector::addCustomPattern(const std::string& pat) { patterns_.push_back(std::regex(pat, std::regex::icase)); }
size_t SqlInjectionDetector::getDetectionCount() const { return detectionCount_; }
auto SqlInjectionDetector::getHistory() const -> std::vector<DetectionResult> { return history_; }
void SqlInjectionDetector::clear() { detectionCount_ = 0; history_.clear(); }
} // namespace nexus::utility::security
''')

# 2. format_string_validator
w(f"{INC}/format_string_validator.h", '''#pragma once
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
    std::regex specifierPattern_{R"(%[\\-+ #0]*(\\d+|\\*)?(\\.\\d+)?[hlLztj]?[diuoxXfFeEgGaAcspn])"};
    std::regex dangerousPattern_{R"(%[\\-+ #0]*[n%])"};
};
} // namespace nexus::utility::security
''')

w(f"{SRC}/format_string_validator.cpp", '''#include "nexus/utility/security/format_string_validator.h"
namespace nexus::utility::security {
FormatStringValidator& FormatStringValidator::instance() { static FormatStringValidator i; return i; }
void FormatStringValidator::initialize() { enabled_ = true; totalValidated_ = 0; dangerousCount_ = 0; }
void FormatStringValidator::shutdown() { enabled_ = false; }
bool FormatStringValidator::isEnabled() const { return enabled_; }

FormatStringValidator::FmtResult FormatStringValidator::validate(const std::string& input) {
    FmtResult r; r.input = input; totalValidated_++;
    auto begin = std::sregex_iterator(input.begin(), input.end(), specifierPattern_);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        r.foundSpecifiers.push_back(it->str());
        if (std::regex_search(it->str(), dangerousPattern_)) { r.safe = false; r.dangerousCount++; }
    }
    if (!r.safe) dangerousCount_++;
    return r;
}
bool FormatStringValidator::isSafe(const std::string& input) { return validate(input).safe; }
size_t FormatStringValidator::getTotalValidated() const { return totalValidated_; }
size_t FormatStringValidator::getDangerousCount() const { return dangerousCount_; }
void FormatStringValidator::clear() { totalValidated_ = 0; dangerousCount_ = 0; }
} // namespace nexus::utility::security
''')

# 3. path_traversal_validator
w(f"{INC}/path_traversal_validator.h", '''#pragma once
#include <string>
#include <vector>
#include <filesystem>

namespace nexus::utility::security {

class PathTraversalValidator {
public:
    struct PathResult { bool safe = true; std::string input; std::string resolved; std::string error; };

    static PathTraversalValidator& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    PathResult validate(const std::string& path, const std::string& baseDir = "");
    bool containsTraversal(const std::string& path) const;
    void setBaseDirectory(const std::string& baseDir);
    std::string getBaseDirectory() const;
    size_t getBlockedCount() const;
    void clear();

private:
    PathTraversalValidator() = default;
    ~PathTraversalValidator() = default;
    bool enabled_ = false;
    std::string baseDir_;
    size_t blockedCount_ = 0;
    static bool hasDangerousPattern(const std::string& p);
};
} // namespace nexus::utility::security
''')

w(f"{SRC}/path_traversal_validator.cpp", '''#include "nexus/utility/security/path_traversal_validator.h"
#include <algorithm>
namespace nexus::utility::security {
PathTraversalValidator& PathTraversalValidator::instance() { static PathTraversalValidator i; return i; }
void PathTraversalValidator::initialize() { enabled_ = true; blockedCount_ = 0; }
void PathTraversalValidator::shutdown() { enabled_ = false; }
bool PathTraversalValidator::isEnabled() const { return enabled_; }

bool PathTraversalValidator::hasDangerousPattern(const std::string& p) {
    return p.find("..") != std::string::npos || p.find("~") != std::string::npos ||
           p.find("/etc/") != std::string::npos || p.find("\\\\") != std::string::npos;
}

PathTraversalValidator::PathResult PathTraversalValidator::validate(const std::string& path, const std::string& baseDir) {
    PathResult r; r.input = path;
    if (hasDangerousPattern(path)) { r.safe = false; r.error = "Path traversal detected"; blockedCount_++; return r; }
    try {
        std::filesystem::path p(path);
        if (!baseDir.empty()) p = std::filesystem::path(baseDir) / p;
        r.resolved = std::filesystem::weakly_canonical(p).string();
        if (!baseDir.empty() && r.resolved.find(baseDir) != 0) { r.safe = false; r.error = "Escapes base directory"; blockedCount_++; }
    } catch (...) { r.safe = false; r.error = "Invalid path"; blockedCount_++; }
    return r;
}

bool PathTraversalValidator::containsTraversal(const std::string& path) const { return hasDangerousPattern(path); }
void PathTraversalValidator::setBaseDirectory(const std::string& bd) { baseDir_ = bd; }
std::string PathTraversalValidator::getBaseDirectory() const { return baseDir_; }
size_t PathTraversalValidator::getBlockedCount() const { return blockedCount_; }
void PathTraversalValidator::clear() { blockedCount_ = 0; }
} // namespace nexus::utility::security
''')

# 4. input_sanitizer
w(f"{INC}/input_sanitizer.h", '''#pragma once
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
''')

w(f"{SRC}/input_sanitizer.cpp", '''#include "nexus/utility/security/input_sanitizer.h"
#include <algorithm>
#include <cctype>
namespace nexus::utility::security {
InputSanitizer& InputSanitizer::instance() { static InputSanitizer i; return i; }
void InputSanitizer::initialize() { enabled_ = true; sanitizedCount_ = 0; }
void InputSanitizer::shutdown() { enabled_ = false; sanitizers_.clear(); }
bool InputSanitizer::isEnabled() const { return enabled_; }

std::string InputSanitizer::sanitize(const std::string& input) {
    if (!enabled_) return input;
    std::string result = input;
    for (auto& [name, fn] : sanitizers_) result = fn(result);
    sanitizedCount_++;
    return result;
}
void InputSanitizer::addSanitizer(const std::string& name, SanitizerFn fn) { if (fn) sanitizers_.emplace_back(name, fn); }
size_t InputSanitizer::getSanitizedCount() const { return sanitizedCount_; }

std::string InputSanitizer::stripHtml(const std::string& input) {
    std::string r; bool inTag = false;
    for (char c : input) { if (c == '<') inTag = true; else if (c == '>') inTag = false; else if (!inTag) r += c; }
    return r;
}
std::string InputSanitizer::stripControlChars(const std::string& input) {
    std::string r;
    for (char c : input) if (c >= 32 || c == '\\n' || c == '\\r' || c == '\\t') r += c;
    return r;
}
std::string InputSanitizer::truncate(const std::string& input, size_t maxLen) {
    return input.size() <= maxLen ? input : input.substr(0, maxLen);
}
void InputSanitizer::clear() { sanitizedCount_ = 0; }
} // namespace nexus::utility::security
''')

# 5. crypto_validator
w(f"{INC}/crypto_validator.h", '''#pragma once
#include <string>
#include <vector>
#include <cstddef>

namespace nexus::utility::security {

class CryptoValidator {
public:
    struct CryptoCheck { std::string check; bool passed; std::string detail; };

    static CryptoValidator& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    bool validateKeyLength(size_t keyBits, size_t minBits = 128);
    bool validateIVLength(size_t ivBytes, size_t expectedBytes = 16);
    bool validateHashOutput(size_t hashBytes, size_t expectedBytes = 32);
    bool detectWeakAlgorithm(const std::string& algorithm);
    std::vector<CryptoCheck> getHistory() const;
    size_t getCheckCount() const;
    size_t getFailureCount() const;
    void clear();

private:
    CryptoValidator() = default;
    ~CryptoValidator() = default;
    bool enabled_ = false;
    std::vector<CryptoCheck> history_;
    size_t checkCount_ = 0;
    size_t failureCount_ = 0;
};
} // namespace nexus::utility::security
''')

w(f"{SRC}/crypto_validator.cpp", '''#include "nexus/utility/security/crypto_validator.h"
#include <unordered_set>
namespace nexus::utility::security {
CryptoValidator& CryptoValidator::instance() { static CryptoValidator i; return i; }
void CryptoValidator::initialize() { enabled_ = true; history_.clear(); checkCount_ = 0; failureCount_ = 0; }
void CryptoValidator::shutdown() { enabled_ = false; }
bool CryptoValidator::isEnabled() const { return enabled_; }

bool CryptoValidator::validateKeyLength(size_t bits, size_t minBits) {
    checkCount_++; bool ok = bits >= minBits;
    history_.push_back({"key_length", ok, std::to_string(bits) + " bits (min " + std::to_string(minBits) + ")"});
    if (!ok) failureCount_++; return ok;
}
bool CryptoValidator::validateIVLength(size_t bytes, size_t expected) {
    checkCount_++; bool ok = bytes >= expected;
    history_.push_back({"iv_length", ok, std::to_string(bytes) + " bytes (expected " + std::to_string(expected) + ")"});
    if (!ok) failureCount_++; return ok;
}
bool CryptoValidator::validateHashOutput(size_t bytes, size_t expected) {
    checkCount_++; bool ok = bytes >= expected;
    history_.push_back({"hash_output", ok, std::to_string(bytes) + " bytes"});
    if (!ok) failureCount_++; return ok;
}
bool CryptoValidator::detectWeakAlgorithm(const std::string& algo) {
    static const std::unordered_set<std::string> weak = {"MD5","SHA1","RC4","DES","3DES","MD4","RC2"};
    bool weak_ = weak.count(algo) > 0;
    checkCount_++; history_.push_back({"weak_algo", !weak_, algo}); if (weak_) failureCount_++; return weak_;
}
auto CryptoValidator::getHistory() const -> std::vector<CryptoCheck> { return history_; }
size_t CryptoValidator::getCheckCount() const { return checkCount_; }
size_t CryptoValidator::getFailureCount() const { return failureCount_; }
void CryptoValidator::clear() { history_.clear(); checkCount_ = 0; failureCount_ = 0; }
} // namespace nexus::utility::security
''')

print("Generated security tools 1-5")
