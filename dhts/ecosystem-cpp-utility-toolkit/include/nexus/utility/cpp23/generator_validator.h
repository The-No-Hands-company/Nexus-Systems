#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace nexus::utility::cpp23 {

/// @brief Validate std::generator behavior: yielded values and completion.
class GeneratorValidator {
public:
    static GeneratorValidator& instance() {
        static GeneratorValidator inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        generators_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void recordYield(const std::string& generator, int value) {
        std::lock_guard<std::mutex> lock(mutex_);
        generators_[generator].values.push_back(value);
    }

    void recordDone(const std::string& generator) {
        std::lock_guard<std::mutex> lock(mutex_);
        generators_[generator].done = true;
    }

    /// All values yielded so far by a generator.
    std::vector<int> getValuesGenerated(const std::string& generator) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = generators_.find(generator);
        return it != generators_.end() ? it->second.values : std::vector<int>{};
    }

    /// Count of values yielded by a generator.
    size_t getCount(const std::string& generator) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = generators_.find(generator);
        return it != generators_.end() ? it->second.values.size() : 0;
    }

    bool isDone(const std::string& generator) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = generators_.find(generator);
        return it != generators_.end() && it->second.done;
    }

private:
    struct GenInfo {
        std::vector<int> values;
        bool done = false;
    };

    GeneratorValidator() = default;
    ~GeneratorValidator() = default;
    GeneratorValidator(const GeneratorValidator&) = delete;
    GeneratorValidator& operator=(const GeneratorValidator&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, GenInfo> generators_;
};

} // namespace nexus::utility::cpp23
