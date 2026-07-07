#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace nexus::utility::pipeline {

/// @brief Builds a linear processing pipeline of named string-to-string transform stages.
class PipelineBuilder {
public:
    using Transform = std::function<std::string(std::string)>;

    static PipelineBuilder& instance() {
        static PipelineBuilder inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        config_ = config;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        stages_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void addStage(const std::string& name, Transform transform) {
        std::lock_guard<std::mutex> lock(mutex_);
        stages_.emplace_back(name, std::move(transform));
    }

    /// @brief Pipe an input through every stage in registration order.
    std::string execute(const std::string& input) {
        std::vector<std::pair<std::string, Transform>> stages;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stages = stages_;
        }
        std::string value = input;
        for (auto& [name, transform] : stages) {
            (void)name;
            if (transform) value = transform(value);
        }
        return value;
    }

    size_t getStageCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return stages_.size();
    }

    std::vector<std::string> getStageNames() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        names.reserve(stages_.size());
        for (const auto& [name, _] : stages_) names.push_back(name);
        return names;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        stages_.clear();
    }

private:
    PipelineBuilder() = default;
    ~PipelineBuilder() = default;
    PipelineBuilder(const PipelineBuilder&) = delete;
    PipelineBuilder& operator=(const PipelineBuilder&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<std::pair<std::string, Transform>> stages_;
};

} // namespace nexus::utility::pipeline
