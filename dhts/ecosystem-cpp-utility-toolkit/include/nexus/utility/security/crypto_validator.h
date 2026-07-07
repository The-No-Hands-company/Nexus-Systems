#pragma once
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
