#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::documentation {

/**
 * @brief Validate completeness of API documentation.
 */
class ApiDocumentationValidator {
public:
    struct SymbolDoc {
        std::string symbol;
        bool hasBrief = false;
        bool hasParams = false;
        bool hasReturn = false;
        bool isFunction = true;
        bool hasReturnValue = true;   // false for void functions
    };

    struct Report {
        std::string symbol;
        std::vector<std::string> missing;
        bool complete() const { return missing.empty(); }
    };

    static ApiDocumentationValidator& instance() {
        static ApiDocumentationValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    Report validate(const SymbolDoc& doc) {
        Report r;
        r.symbol = doc.symbol;
        if (!doc.hasBrief) r.missing.push_back("brief description");
        if (doc.isFunction && !doc.hasParams) r.missing.push_back("parameter docs");
        if (doc.isFunction && doc.hasReturnValue && !doc.hasReturn)
            r.missing.push_back("return docs");
        std::lock_guard<std::mutex> lk(mutex_);
        reports_[doc.symbol] = r;
        ++total_;
        if (r.complete()) ++documented_;
        return r;
    }

    double coveragePercentage() const {
        std::size_t t = total_.load(), d = documented_.load();
        return t == 0 ? 100.0 : 100.0 * d / t;
    }

    std::vector<Report> incompleteSymbols() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Report> out;
        for (const auto& [s, r] : reports_) if (!r.complete()) out.push_back(r);
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        reports_.clear();
        total_ = 0;
        documented_ = 0;
    }

private:
    ApiDocumentationValidator() = default;
    ~ApiDocumentationValidator() = default;
    ApiDocumentationValidator(const ApiDocumentationValidator&) = delete;
    ApiDocumentationValidator& operator=(const ApiDocumentationValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, Report> reports_;
    std::atomic<std::size_t> total_{0};
    std::atomic<std::size_t> documented_{0};
};

} // namespace nexus::utility::documentation
