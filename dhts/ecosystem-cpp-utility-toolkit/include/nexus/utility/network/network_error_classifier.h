#pragma once
#include <string>
#include <vector>
#include <unordered_map>
namespace nexus::utility::network {
class NetworkErrorClassifier {
public:
    enum class Category { Timeout, ConnectionRefused, DNSError, SSLHandshake, Reset, Unreachable, Unknown };
    struct ErrorRecord { Category category; int errorCode; std::string message; size_t count; };
    static NetworkErrorClassifier& instance(); void initialize(); void shutdown(); bool isEnabled() const;
    void recordError(int errorCode, const std::string& message);
    ErrorRecord getStats(Category cat) const;
    std::vector<ErrorRecord> getAllStats() const;
    size_t getTotalErrors() const;
    static Category classify(int errorCode, const std::string& message);
    void clear();
private:
    NetworkErrorClassifier() = default; ~NetworkErrorClassifier() = default; bool enabled_ = false;
    std::unordered_map<Category, ErrorRecord> stats_;
    static std::string categoryName(Category c);
};
} // namespace nexus::utility::network