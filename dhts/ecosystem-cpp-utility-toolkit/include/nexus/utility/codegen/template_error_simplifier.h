#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace nexus::utility::codegen {

/// @brief Simplify and aggregate template instantiation errors by template.
class TemplateErrorSimplifier {
public:
    static TemplateErrorSimplifier& instance() {
        static TemplateErrorSimplifier inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        errors_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordError(const std::string& template_name, const std::string& error) {
        std::lock_guard<std::mutex> lock(mutex_);
        errors_[template_name].push_back(error);
    }

    size_t getErrorCount(const std::string& template_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = errors_.find(template_name);
        return it != errors_.end() ? it->second.size() : 0;
    }

    /// Name of the template with the most recorded errors ("" if none).
    std::string getMostErrorProne() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::string worst;
        size_t worst_count = 0;
        for (const auto& [name, errs] : errors_) {
            if (errs.size() > worst_count) {
                worst_count = errs.size();
                worst = name;
            }
        }
        return worst;
    }

private:
    TemplateErrorSimplifier() = default;
    ~TemplateErrorSimplifier() = default;
    TemplateErrorSimplifier(const TemplateErrorSimplifier&) = delete;
    TemplateErrorSimplifier& operator=(const TemplateErrorSimplifier&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, std::vector<std::string>> errors_;
};

} // namespace nexus::utility::codegen
