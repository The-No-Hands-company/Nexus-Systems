#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <atomic>
#include <mutex>

namespace nexus::utility::orchestration {

/**
 * @brief Simple key-value query language for diagnostic data.
 *
 * Supports queries of the form: "key=value AND key2=value2" or "key!=value".
 */
class DiagnosticQueryLanguage {
public:
    struct Condition {
        std::string key;
        std::string op;   // "=" or "!="
        std::string value;
    };

    static DiagnosticQueryLanguage& instance() {
        static DiagnosticQueryLanguage inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Parse a query into a list of AND-combined conditions.
    static std::vector<Condition> parse(const std::string& query) {
        std::vector<Condition> conds;
        std::istringstream iss(query);
        std::string token;
        while (iss >> token) {
            if (token == "AND" || token == "and") continue;
            std::string op;
            std::size_t pos = std::string::npos;
            if ((pos = token.find("!=")) != std::string::npos) op = "!=";
            else if ((pos = token.find('=')) != std::string::npos) op = "=";
            if (pos == std::string::npos) continue;
            Condition c;
            c.key = token.substr(0, pos);
            c.op = op;
            c.value = token.substr(pos + op.size());
            conds.push_back(c);
        }
        return conds;
    }

    /// Evaluate a query against a set of key/value data.
    static bool evaluate(const std::string& query,
                         const std::unordered_map<std::string, std::string>& data) {
        for (const auto& c : parse(query)) {
            auto it = data.find(c.key);
            std::string actual = it == data.end() ? std::string{} : it->second;
            bool eq = (actual == c.value);
            if (c.op == "=" && !eq) return false;
            if (c.op == "!=" && eq) return false;
        }
        return true;
    }

    void reset() {}

private:
    DiagnosticQueryLanguage() = default;
    ~DiagnosticQueryLanguage() = default;
    DiagnosticQueryLanguage(const DiagnosticQueryLanguage&) = delete;
    DiagnosticQueryLanguage& operator=(const DiagnosticQueryLanguage&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
};

} // namespace nexus::utility::orchestration
