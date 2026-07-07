#!/usr/bin/env python3
"""Flesh out security tools 6-11: timing, secret_leak, privilege, code_injection, constant_time, secure_wipe"""
import os
BASE = "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/dhts/dht_nexus_debug"
INC = f"{BASE}/include/nexus/utility/security"
SRC = f"{BASE}/src/utility/security"
os.makedirs(SRC, exist_ok=True)

def w(p,c):
    with open(p,'w') as f: f.write(c)

# 6. timing_attack_detector
w(f"{INC}/timing_attack_detector.h", '''#pragma once
#include <string>
#include <vector>
#include <chrono>

namespace nexus::utility::security {

class TimingAttackDetector {
public:
    struct TimingSample { std::string operation; std::chrono::nanoseconds elapsed; size_t inputSize; bool suspicious; };

    static TimingAttackDetector& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    void recordTiming(const std::string& op, std::chrono::nanoseconds elapsed, size_t inputSize);
    bool analyzeTimingVariance(const std::string& op, double maxVariancePercent = 20.0);
    std::vector<TimingSample> getSamples() const;
    size_t getSuspiciousCount() const;
    void setThreshold(double maxVariancePercent);
    void clear();

private:
    TimingAttackDetector() = default;
    ~TimingAttackDetector() = default;
    bool enabled_ = false;
    double maxVariance_ = 20.0;
    std::vector<TimingSample> samples_;
    size_t suspiciousCount_ = 0;
};
} // namespace nexus::utility::security
''')

w(f"{SRC}/timing_attack_detector.cpp", '''#include "nexus/utility/security/timing_attack_detector.h"
#include <numeric>
#include <cmath>
namespace nexus::utility::security {
TimingAttackDetector& TimingAttackDetector::instance() { static TimingAttackDetector i; return i; }
void TimingAttackDetector::initialize() { enabled_ = true; samples_.clear(); suspiciousCount_ = 0; }
void TimingAttackDetector::shutdown() { enabled_ = false; }
bool TimingAttackDetector::isEnabled() const { return enabled_; }

void TimingAttackDetector::recordTiming(const std::string& op, std::chrono::nanoseconds t, size_t sz) {
    if (!enabled_) return;
    bool susp = false;
    if (sz > 0) {
        double nsPerByte = (double)t.count() / sz;
        if (nsPerByte > 100.0) susp = true;
    }
    samples_.push_back({op, t, sz, susp});
    if (susp) suspiciousCount_++;
}

bool TimingAttackDetector::analyzeTimingVariance(const std::string& op, double maxVar) {
    std::vector<int64_t> times;
    for (auto& s : samples_) if (s.operation == op) times.push_back(s.elapsed.count());
    if (times.size() < 2) return true;
    double sum = std::accumulate(times.begin(), times.end(), 0LL);
    double mean = sum / times.size();
    double var = 0;
    for (auto t : times) { double d = t - mean; var += d*d; }
    var /= times.size();
    double stddev = std::sqrt(var);
    double cv = mean > 0 ? (stddev / mean) * 100.0 : 0.0;
    return cv <= maxVar;
}

auto TimingAttackDetector::getSamples() const -> std::vector<TimingSample> { return samples_; }
size_t TimingAttackDetector::getSuspiciousCount() const { return suspiciousCount_; }
void TimingAttackDetector::setThreshold(double v) { maxVariance_ = v; }
void TimingAttackDetector::clear() { samples_.clear(); suspiciousCount_ = 0; }
} // namespace nexus::utility::security
''')

# 7. secret_leak_detector
w(f"{INC}/secret_leak_detector.h", '''#pragma once
#include <string>
#include <vector>
#include <regex>
#include <unordered_set>

namespace nexus::utility::security {

class SecretLeakDetector {
public:
    struct LeakDetection { std::string pattern; std::string value; size_t position; std::string context; };

    static SecretLeakDetector& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    std::vector<LeakDetection> scan(const std::string& content);
    void addSecretPattern(const std::string& name, const std::regex& pattern);
    size_t getDetectionCount() const;
    std::vector<LeakDetection> getHistory() const;
    void clear();

private:
    SecretLeakDetector() = default;
    ~SecretLeakDetector() = default;
    bool enabled_ = false;
    size_t detectionCount_ = 0;
    std::vector<std::pair<std::string, std::regex>> patterns_;
    std::vector<LeakDetection> history_;
    void buildDefaultPatterns();
};
} // namespace nexus::utility::security
''')

w(f"{SRC}/secret_leak_detector.cpp", '''#include "nexus/utility/security/secret_leak_detector.h"
namespace nexus::utility::security {
SecretLeakDetector& SecretLeakDetector::instance() { static SecretLeakDetector i; return i; }
void SecretLeakDetector::initialize() { enabled_ = true; buildDefaultPatterns(); }
void SecretLeakDetector::shutdown() { enabled_ = false; patterns_.clear(); }
bool SecretLeakDetector::isEnabled() const { return enabled_; }

void SecretLeakDetector::buildDefaultPatterns() {
    patterns_.emplace_back("AWS Key", std::regex(R"((AKIA[0-9A-Z]{16}))"));
    patterns_.emplace_back("Private Key", std::regex(R"(-----BEGIN (RSA |EC )?PRIVATE KEY-----)"));
    patterns_.emplace_back("JWT", std::regex(R"(eyJ[a-zA-Z0-9_-]*\\.eyJ[a-zA-Z0-9_-]*\\.[a-zA-Z0-9_-]*)"));
    patterns_.emplace_back("API Key (generic)", std::regex(R"((api[_-]?key\\s*[:=]\\s*['\"]?[a-zA-Z0-9_-]{20,}['\"]?))", std::regex::icase));
    patterns_.emplace_back("Password assignment", std::regex(R"((password\\s*[:=]\\s*['\"]?[^'\"\\s]{4,}['\"]?))", std::regex::icase));
}

auto SecretLeakDetector::scan(const std::string& content) -> std::vector<LeakDetection> {
    std::vector<LeakDetection> results;
    if (!enabled_) return results;
    for (auto& [name, pattern] : patterns_) {
        auto begin = std::sregex_iterator(content.begin(), content.end(), pattern);
        for (auto it = begin; it != std::sregex_iterator(); ++it) {
            LeakDetection d{name, it->str(), (size_t)it->position(), ""};
            size_t start = it->position() > 20 ? it->position() - 20 : 0;
            size_t len = std::min((size_t)60, content.size() - start);
            d.context = content.substr(start, len);
            results.push_back(d); detectionCount_++; history_.push_back(d);
        }
    }
    return results;
}
void SecretLeakDetector::addSecretPattern(const std::string& name, const std::regex& p) { patterns_.emplace_back(name, p); }
size_t SecretLeakDetector::getDetectionCount() const { return detectionCount_; }
auto SecretLeakDetector::getHistory() const -> std::vector<LeakDetection> { return history_; }
void SecretLeakDetector::clear() { detectionCount_ = 0; history_.clear(); }
} // namespace nexus::utility::security
''')

# 8. privilege_escalation_guard
w(f"{INC}/privilege_escalation_guard.h", '''#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <functional>

namespace nexus::utility::security {

class PrivilegeEscalationGuard {
public:
    struct PrivilegeChange { std::string fromUser; std::string toUser; bool escalated; std::chrono::system_clock::time_point timestamp; };
    using EscalationCallback = std::function<void(const PrivilegeChange&)>;

    static PrivilegeEscalationGuard& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    bool recordUserSwitch(const std::string& from, const std::string& to);
    void onEscalation(EscalationCallback cb);
    std::vector<PrivilegeChange> getHistory() const;
    size_t getEscalationCount() const;
    bool hasEscalated() const;
    void clear();

private:
    PrivilegeEscalationGuard() = default;
    ~PrivilegeEscalationGuard() = default;
    bool enabled_ = false;
    std::vector<PrivilegeChange> history_;
    std::vector<EscalationCallback> callbacks_;
    size_t escalationCount_ = 0;
};
} // namespace nexus::utility::security
''')

w(f"{SRC}/privilege_escalation_guard.cpp", '''#include "nexus/utility/security/privilege_escalation_guard.h"
namespace nexus::utility::security {
PrivilegeEscalationGuard& PrivilegeEscalationGuard::instance() { static PrivilegeEscalationGuard i; return i; }
void PrivilegeEscalationGuard::initialize() { enabled_ = true; history_.clear(); escalationCount_ = 0; }
void PrivilegeEscalationGuard::shutdown() { enabled_ = false; }
bool PrivilegeEscalationGuard::isEnabled() const { return enabled_; }

bool PrivilegeEscalationGuard::recordUserSwitch(const std::string& from, const std::string& to) {
    if (!enabled_) return true;
    bool escalated = (to == "root" || to == "admin" || to == "Administrator") && from != to;
    PrivilegeChange pc{from, to, escalated, std::chrono::system_clock::now()};
    history_.push_back(pc);
    if (escalated) { escalationCount_++; for (auto& cb : callbacks_) if (cb) cb(pc); }
    return !escalated;
}
void PrivilegeEscalationGuard::onEscalation(EscalationCallback cb) { if (cb) callbacks_.push_back(cb); }
auto PrivilegeEscalationGuard::getHistory() const -> std::vector<PrivilegeChange> { return history_; }
size_t PrivilegeEscalationGuard::getEscalationCount() const { return escalationCount_; }
bool PrivilegeEscalationGuard::hasEscalated() const { return escalationCount_ > 0; }
void PrivilegeEscalationGuard::clear() { history_.clear(); escalationCount_ = 0; }
} // namespace nexus::utility::security
''')

# 9. code_injection_detector
w(f"{INC}/code_injection_detector.h", '''#pragma once
#include <string>
#include <vector>
#include <regex>

namespace nexus::utility::security {

class CodeInjectionDetector {
public:
    struct InjectionResult { bool detected = false; std::string type; std::string payload; size_t position; };

    static CodeInjectionDetector& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    InjectionResult scan(const std::string& input);
    size_t getDetectionCount() const;
    std::vector<InjectionResult> getHistory() const;
    void clear();

private:
    CodeInjectionDetector() = default;
    ~CodeInjectionDetector() = default;
    bool enabled_ = false;
    size_t detectionCount_ = 0;
    std::vector<std::pair<std::string, std::regex>> patterns_;
    std::vector<InjectionResult> history_;
    void buildPatterns();
};
} // namespace nexus::utility::security
''')

w(f"{SRC}/code_injection_detector.cpp", '''#include "nexus/utility/security/code_injection_detector.h"
namespace nexus::utility::security {
CodeInjectionDetector& CodeInjectionDetector::instance() { static CodeInjectionDetector i; return i; }
void CodeInjectionDetector::initialize() { enabled_ = true; buildPatterns(); }
void CodeInjectionDetector::shutdown() { enabled_ = false; patterns_.clear(); }
bool CodeInjectionDetector::isEnabled() const { return enabled_; }

void CodeInjectionDetector::buildPatterns() {
    patterns_.emplace_back("shell_cmd", std::regex(R"((\\bexec\\s*\\(|\\bsystem\\s*\\(|\\beval\\s*\\(|`[^`]+`)", std::regex::icase));
    patterns_.emplace_back("js_inline", std::regex(R"((<script[^>]*>.*?</script>|javascript\\s*:))", std::regex::icase));
    patterns_.emplace_back("php_tag", std::regex(R"((<\\?php.*\\?>))", std::regex::icase));
    patterns_.emplace_back("cmd_injection", std::regex(R"(([;&|]\\s*(cat|rm|wget|curl|nc|netcat)\\s+))", std::regex::icase));
    patterns_.emplace_back("template_injection", std::regex(R"((\\{\\{.*\\}\\}|\\$\\{.*\\}))"));
}

CodeInjectionDetector::InjectionResult CodeInjectionDetector::scan(const std::string& input) {
    InjectionResult r;
    if (!enabled_) return r;
    for (auto& [type, pattern] : patterns_) {
        std::smatch m;
        if (std::regex_search(input, m, pattern)) {
            r.detected = true; r.type = type; r.payload = m.str(); r.position = m.position();
            detectionCount_++; history_.push_back(r); return r;
        }
    }
    return r;
}
size_t CodeInjectionDetector::getDetectionCount() const { return detectionCount_; }
auto CodeInjectionDetector::getHistory() const -> std::vector<InjectionResult> { return history_; }
void CodeInjectionDetector::clear() { detectionCount_ = 0; history_.clear(); }
} // namespace nexus::utility::security
''')

# 10. constant_time_validator
w(f"{INC}/constant_time_validator.h", '''#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

namespace nexus::utility::security {

class ConstantTimeValidator {
public:
    struct TimingResult { std::string operation; std::chrono::nanoseconds minTime; std::chrono::nanoseconds maxTime; double variance; bool isConstantTime; };

    static ConstantTimeValidator& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    void startMeasurement(const std::string& op);
    void recordMeasurement(const std::string& op, std::chrono::nanoseconds time);
    TimingResult finishMeasurement(const std::string& op, double maxVariation = 0.10);
    static bool constantTimeCompare(const uint8_t* a, const uint8_t* b, size_t len);
    std::vector<TimingResult> getResults() const;
    void clear();

private:
    ConstantTimeValidator() = default;
    ~ConstantTimeValidator() = default;
    bool enabled_ = false;
    std::vector<TimingResult> results_;
};
} // namespace nexus::utility::security
''')

w(f"{SRC}/constant_time_validator.cpp", '''#include "nexus/utility/security/constant_time_validator.h"
#include <algorithm>
#include <numeric>
#include <cmath>
namespace nexus::utility::security {
ConstantTimeValidator& ConstantTimeValidator::instance() { static ConstantTimeValidator i; return i; }
void ConstantTimeValidator::initialize() { enabled_ = true; results_.clear(); }
void ConstantTimeValidator::shutdown() { enabled_ = false; }
bool ConstantTimeValidator::isEnabled() const { return enabled_; }

void ConstantTimeValidator::startMeasurement(const std::string& op) { (void)op; }
void ConstantTimeValidator::recordMeasurement(const std::string& op, std::chrono::nanoseconds t) {
    for (auto& r : results_) {
        if (r.operation == op) { r.minTime = std::min(r.minTime, t); r.maxTime = std::max(r.maxTime, t); return; }
    }
    results_.push_back({op, t, t, 0.0, true});
}

ConstantTimeValidator::TimingResult ConstantTimeValidator::finishMeasurement(const std::string& op, double maxVar) {
    for (auto& r : results_) {
        if (r.operation == op) {
            double range = (double)(r.maxTime - r.minTime).count();
            double avg = (double)(r.maxTime + r.minTime).count() / 2.0;
            r.variance = avg > 0 ? range / avg : 0.0;
            r.isConstantTime = r.variance <= maxVar;
            return r;
        }
    }
    return {};
}

bool ConstantTimeValidator::constantTimeCompare(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) diff |= a[i] ^ b[i];
    return diff == 0;
}

auto ConstantTimeValidator::getResults() const -> std::vector<TimingResult> { return results_; }
void ConstantTimeValidator::clear() { results_.clear(); }
} // namespace nexus::utility::security
''')

# 11. secure_wipe
w(f"{INC}/secure_wipe.h", '''#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace nexus::utility::security {

class SecureWipe {
public:
    struct WipeRecord { void* address; size_t size; std::string context; };

    static SecureWipe& instance();
    void initialize();
    void shutdown();
    bool isEnabled() const;

    static void wipe(void* ptr, size_t size);
    static void wipeString(std::string& str);
    template<typename T> static void wipeObject(T& obj) { wipe(&obj, sizeof(T)); }

    void recordWipe(void* addr, size_t size, const std::string& ctx = "");
    std::vector<WipeRecord> getHistory() const;
    size_t getWipeCount() const;
    size_t getTotalBytesWiped() const;
    void clear();

private:
    SecureWipe() = default;
    ~SecureWipe() = default;
    bool enabled_ = false;
    std::vector<WipeRecord> history_;
    size_t totalBytes_ = 0;
};
} // namespace nexus::utility::security
''')

w(f"{SRC}/secure_wipe.cpp", '''#include "nexus/utility/security/secure_wipe.h"
#include <algorithm>
namespace nexus::utility::security {
SecureWipe& SecureWipe::instance() { static SecureWipe i; return i; }
void SecureWipe::initialize() { enabled_ = true; history_.clear(); totalBytes_ = 0; }
void SecureWipe::shutdown() { enabled_ = false; }
bool SecureWipe::isEnabled() const { return enabled_; }

void SecureWipe::wipe(void* ptr, size_t size) {
    if (!ptr || size == 0) return;
    volatile uint8_t* p = static_cast<volatile uint8_t*>(ptr);
    for (size_t i = 0; i < size; i++) p[i] = 0x00;
    for (size_t i = 0; i < size; i++) p[i] = 0xFF;
    for (size_t i = 0; i < size; i++) p[i] = 0xAA;
    for (size_t i = 0; i < size; i++) p[i] = 0x00;
}

void SecureWipe::wipeString(std::string& str) {
    if (!str.empty()) { wipe(&str[0], str.size()); str.clear(); }
}

void SecureWipe::recordWipe(void* addr, size_t size, const std::string& ctx) {
    if (!enabled_) return;
    history_.push_back({addr, size, ctx}); totalBytes_ += size;
}

auto SecureWipe::getHistory() const -> std::vector<WipeRecord> { return history_; }
size_t SecureWipe::getWipeCount() const { return history_.size(); }
size_t SecureWipe::getTotalBytesWiped() const { return totalBytes_; }
void SecureWipe::clear() { history_.clear(); totalBytes_ = 0; }
} // namespace nexus::utility::security
''')

print("Generated security tools 6-11")
