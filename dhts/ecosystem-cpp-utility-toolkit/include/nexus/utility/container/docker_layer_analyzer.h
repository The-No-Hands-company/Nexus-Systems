#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <cstddef>

namespace nexus::utility::container {

class DockerLayerAnalyzer {
public:
    struct Layer {
        std::string id;
        size_t size_bytes;
    };

    static DockerLayerAnalyzer& instance() {
        static DockerLayerAnalyzer inst;
        return inst;
    }

    void initialize(const std::string& config = "") { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; config_ = config; }
    void shutdown() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    bool isEnabled() const { return enabled_; }
    void enable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = true; }
    void disable() { std::lock_guard<std::mutex> lk(mutex_); enabled_ = false; }
    void reset() { std::lock_guard<std::mutex> lk(mutex_); layers_.clear(); }

    void addLayer(const std::string& id, size_t size_bytes) {
        std::lock_guard<std::mutex> lk(mutex_);
        layers_.push_back({id, size_bytes});
    }

    size_t getTotalSize() const {
        std::lock_guard<std::mutex> lk(mutex_);
        size_t total = 0;
        for (const auto& l : layers_) total += l.size_bytes;
        return total;
    }

    size_t getLayerCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return layers_.size();
    }

    Layer getLargestLayer() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (layers_.empty()) return {};
        const Layer* largest = &layers_[0];
        for (const auto& l : layers_) {
            if (l.size_bytes > largest->size_bytes) largest = &l;
        }
        return *largest;
    }

    size_t getLayerSize(const std::string& id) const {
        std::lock_guard<std::mutex> lk(mutex_);
        for (const auto& l : layers_) {
            if (l.id == id) return l.size_bytes;
        }
        return 0;
    }

private:
    DockerLayerAnalyzer() = default;
    ~DockerLayerAnalyzer() = default;
    DockerLayerAnalyzer(const DockerLayerAnalyzer&) = delete;
    DockerLayerAnalyzer& operator=(const DockerLayerAnalyzer&) = delete;
    DockerLayerAnalyzer(DockerLayerAnalyzer&&) = delete;
    DockerLayerAnalyzer& operator=(DockerLayerAnalyzer&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::vector<Layer> layers_;
};

} // namespace nexus::utility::container
