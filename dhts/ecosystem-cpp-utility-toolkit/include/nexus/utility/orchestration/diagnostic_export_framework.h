#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <sstream>
#include <atomic>
#include <mutex>

namespace nexus::utility::orchestration {

/**
 * @brief Collect diagnostic key/value data and export it as JSON or plain text.
 */
class DiagnosticExportFramework {
public:
    enum class Format { Json, Text };

    static DiagnosticExportFramework& instance() {
        static DiagnosticExportFramework inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lk(mutex_);
        if (!data_.count(key)) order_.push_back(key);
        data_[key] = value;
    }

    std::string exportAs(Format format) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::ostringstream os;
        if (format == Format::Json) {
            os << "{";
            bool first = true;
            for (const auto& key : order_) {
                if (!first) os << ",";
                first = false;
                os << "\"" << escape(key) << "\":\"" << escape(data_.at(key)) << "\"";
            }
            os << "}";
        } else {
            for (const auto& key : order_)
                os << key << ": " << data_.at(key) << "\n";
        }
        return os.str();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        data_.clear();
        order_.clear();
    }

private:
    DiagnosticExportFramework() = default;
    ~DiagnosticExportFramework() = default;
    DiagnosticExportFramework(const DiagnosticExportFramework&) = delete;
    DiagnosticExportFramework& operator=(const DiagnosticExportFramework&) = delete;

    static std::string escape(const std::string& s) {
        std::string out;
        for (char c : s) {
            if (c == '"' || c == '\\') out += '\\';
            out += c;
        }
        return out;
    }

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, std::string> data_;
    std::vector<std::string> order_;
};

} // namespace nexus::utility::orchestration
