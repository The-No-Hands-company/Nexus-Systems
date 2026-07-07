#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <mutex>
#include <atomic>

namespace nexus::utility::codequality {

/**
 * @brief Track function sizes (in lines) and report oversized functions.
 */
class FunctionSizeReporter {
public:
    struct FunctionSize {
        std::string function;
        std::size_t lines = 0;
        std::string file;
    };

    static FunctionSizeReporter& instance() {
        static FunctionSizeReporter inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void record(const std::string& function, std::size_t lines, const std::string& file = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        sizes_[function] = {function, lines, file};
    }

    std::size_t size(const std::string& function) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = sizes_.find(function);
        return it == sizes_.end() ? 0 : it->second.lines;
    }

    double averageSize() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (sizes_.empty()) return 0.0;
        std::size_t sum = 0;
        for (const auto& [f, s] : sizes_) sum += s.lines;
        return static_cast<double>(sum) / sizes_.size();
    }

    std::vector<FunctionSize> oversized(std::size_t threshold) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<FunctionSize> out;
        for (const auto& [f, s] : sizes_)
            if (s.lines > threshold) out.push_back(s);
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
            return a.lines > b.lines;
        });
        return out;
    }

    std::vector<FunctionSize> largest(std::size_t topN) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<FunctionSize> out;
        for (const auto& [f, s] : sizes_) out.push_back(s);
        std::sort(out.begin(), out.end(), [](const auto& a, const auto& b) {
            return a.lines > b.lines;
        });
        if (out.size() > topN) out.resize(topN);
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        sizes_.clear();
    }

private:
    FunctionSizeReporter() = default;
    ~FunctionSizeReporter() = default;
    FunctionSizeReporter(const FunctionSizeReporter&) = delete;
    FunctionSizeReporter& operator=(const FunctionSizeReporter&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, FunctionSize> sizes_;
};

} // namespace nexus::utility::codequality
