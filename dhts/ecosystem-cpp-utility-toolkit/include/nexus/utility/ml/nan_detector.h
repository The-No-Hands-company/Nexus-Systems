#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <atomic>
#include <mutex>

namespace nexus::utility::ml {

/**
 * @brief Detect NaN and Inf values in tensors.
 */
class NanDetector {
public:
    struct ScanResult {
        std::string tensor;
        std::size_t total = 0;
        std::size_t nanCount = 0;
        std::size_t infCount = 0;
        bool clean() const { return nanCount == 0 && infCount == 0; }
    };

    static NanDetector& instance() {
        static NanDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    template<typename T>
    ScanResult scan(const std::string& tensor, const T* data, std::size_t size) {
        ScanResult r;
        r.tensor = tensor;
        r.total = size;
        for (std::size_t i = 0; i < size; ++i) {
            if (std::isnan(static_cast<double>(data[i]))) ++r.nanCount;
            else if (std::isinf(static_cast<double>(data[i]))) ++r.infCount;
        }
        std::lock_guard<std::mutex> lk(mutex_);
        scanned_ += size;
        if (!r.clean()) { ++dirtyTensors_; lastDirty_ = r; }
        return r;
    }

    template<typename T>
    ScanResult scan(const std::string& tensor, const std::vector<T>& data) {
        return scan(tensor, data.data(), data.size());
    }

    std::size_t dirtyTensors() const { return dirtyTensors_.load(); }
    std::size_t elementsScanned() const { return scanned_.load(); }

    ScanResult lastDirty() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return lastDirty_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        scanned_ = 0;
        dirtyTensors_ = 0;
        lastDirty_ = {};
    }

private:
    NanDetector() = default;
    ~NanDetector() = default;
    NanDetector(const NanDetector&) = delete;
    NanDetector& operator=(const NanDetector&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::atomic<std::size_t> scanned_{0};
    std::atomic<std::size_t> dirtyTensors_{0};
    ScanResult lastDirty_;
};

} // namespace nexus::utility::ml
