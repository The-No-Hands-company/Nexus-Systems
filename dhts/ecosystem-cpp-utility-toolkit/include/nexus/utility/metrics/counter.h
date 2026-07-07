#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <vector>

namespace nexus::utility::metrics {

class Counter {
public:
    Counter() = default;

    void inc(int64_t delta = 1) noexcept { value_.fetch_add(delta, std::memory_order_relaxed); }
    void dec(int64_t delta = 1) noexcept { value_.fetch_sub(delta, std::memory_order_relaxed); }
    void set(int64_t value) noexcept { value_.store(value, std::memory_order_relaxed); }

    [[nodiscard]] int64_t value() const noexcept { return value_.load(std::memory_order_relaxed); }
    [[nodiscard]] int64_t operator++() noexcept { return value_.fetch_add(1, std::memory_order_relaxed) + 1; }
    [[nodiscard]] int64_t operator++(int) noexcept { return value_.fetch_add(1, std::memory_order_relaxed); }

    void reset() noexcept { value_.store(0, std::memory_order_relaxed); }

private:
    std::atomic<int64_t> value_{0};
};

class LabeledCounter {
public:
    [[nodiscard]] static LabeledCounter& instance() noexcept {
        static LabeledCounter c;
        return c;
    }

    void inc(std::string_view label, int64_t delta = 1) noexcept {
        std::unique_lock lock(mutex_);
        counters_[std::string(label)].inc(delta);
    }

    [[nodiscard]] int64_t get(std::string_view label) const noexcept {
        std::shared_lock lock(mutex_);
        auto it = counters_.find(std::string(label));
        return it != counters_.end() ? it->second.value() : 0;
    }

    [[nodiscard]] std::unordered_map<std::string, int64_t> snapshot() const {
        std::shared_lock lock(mutex_);
        std::unordered_map<std::string, int64_t> result;
        for (const auto& [k, v] : counters_) {
            result[k] = v.value();
        }
        return result;
    }

    void reset() noexcept {
        std::unique_lock lock(mutex_);
        counters_.clear();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Counter> counters_;
};

} // namespace nexus::utility::metrics
