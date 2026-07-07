#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace nexus::utility::codegen {

/// @brief Trace preprocessor activity: defines, undefs, includes.
class PreprocessorTracer {
public:
    static PreprocessorTracer& instance() {
        static PreprocessorTracer inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        defines_.clear();
        include_count_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordDefine(const std::string& name, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        defines_[name] = value;
    }

    void recordUndef(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        defines_.erase(name);
    }

    void recordInclude(const std::string& file, bool found) {
        std::lock_guard<std::mutex> lock(mutex_);
        ++include_count_;
        if (found) ++found_includes_;
    }

    /// Names currently defined (after applying undefs).
    std::vector<std::string> getDefinedNames() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(defines_.size());
        for (const auto& [name, value] : defines_) names.push_back(name);
        return names;
    }

    size_t getIncludeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return include_count_;
    }

    size_t getFoundIncludeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return found_includes_;
    }

private:
    PreprocessorTracer() = default;
    ~PreprocessorTracer() = default;
    PreprocessorTracer(const PreprocessorTracer&) = delete;
    PreprocessorTracer& operator=(const PreprocessorTracer&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, std::string> defines_;
    size_t include_count_ = 0;
    size_t found_includes_ = 0;
};

} // namespace nexus::utility::codegen
