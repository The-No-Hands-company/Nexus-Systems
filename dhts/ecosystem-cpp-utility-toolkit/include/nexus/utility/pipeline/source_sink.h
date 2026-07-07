#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>

namespace nexus::utility::pipeline {

/// @brief Manages named data sources and sinks and transfers data between them.
class SourceSink {
public:
    using Source = std::function<std::string()>;
    using Sink = std::function<void(std::string)>;

    static SourceSink& instance() {
        static SourceSink inst;
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
        sources_.clear();
        sinks_.clear();
        transfer_count_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void registerSource(const std::string& name, Source source) {
        std::lock_guard<std::mutex> lock(mutex_);
        sources_[name] = std::move(source);
    }

    void registerSink(const std::string& name, Sink sink) {
        std::lock_guard<std::mutex> lock(mutex_);
        sinks_[name] = std::move(sink);
    }

    /// @brief Pull one datum from a source and push it to a sink.
    /// @return true if both endpoints existed and the transfer occurred.
    bool transfer(const std::string& source, const std::string& sink) {
        Source src;
        Sink snk;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto sit = sources_.find(source);
            auto kit = sinks_.find(sink);
            if (sit == sources_.end() || kit == sinks_.end() || !sit->second || !kit->second) {
                return false;
            }
            src = sit->second;
            snk = kit->second;
        }
        snk(src());
        {
            std::lock_guard<std::mutex> lock(mutex_);
            ++transfer_count_;
        }
        return true;
    }

    size_t getTransferCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return transfer_count_;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        sources_.clear();
        sinks_.clear();
        transfer_count_ = 0;
    }

private:
    SourceSink() = default;
    ~SourceSink() = default;
    SourceSink(const SourceSink&) = delete;
    SourceSink& operator=(const SourceSink&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::unordered_map<std::string, Source> sources_;
    std::unordered_map<std::string, Sink> sinks_;
    size_t transfer_count_ = 0;
};

} // namespace nexus::utility::pipeline
