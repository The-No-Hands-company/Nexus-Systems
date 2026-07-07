#pragma once

#include <atomic>
#include <string>
#include <string_view>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <cmath>
#include <limits>

namespace nexus::utility::metrics {

class Gauge {
public:
    Gauge() = default;

    void set(double value) noexcept { value_.store(value, std::memory_order_relaxed); }
    void inc(double delta = 1.0) noexcept {
        double expected = value_.load(std::memory_order_relaxed);
        while (!value_.compare_exchange_weak(expected, expected + delta,
                                              std::memory_order_relaxed,
                                              std::memory_order_relaxed)) {}
    }
    void dec(double delta = 1.0) noexcept { inc(-delta); }

    [[nodiscard]] double value() const noexcept { return value_.load(std::memory_order_relaxed); }
    [[nodiscard]] explicit operator double() const noexcept { return value(); }

    void reset() noexcept { value_.store(0.0, std::memory_order_relaxed); }

private:
    std::atomic<double> value_{0.0};
};

class LabeledGauge {
public:
    [[nodiscard]] static LabeledGauge& instance() noexcept {
        static LabeledGauge g;
        return g;
    }

    void set(std::string_view label, double value) noexcept {
        std::unique_lock lock(mutex_);
        gauges_[std::string(label)].set(value);
    }

    void inc(std::string_view label, double delta = 1.0) noexcept {
        std::unique_lock lock(mutex_);
        gauges_[std::string(label)].inc(delta);
    }

    [[nodiscard]] double get(std::string_view label) const noexcept {
        std::shared_lock lock(mutex_);
        auto it = gauges_.find(std::string(label));
        return it != gauges_.end() ? it->second.value() : 0.0;
    }

    [[nodiscard]] std::unordered_map<std::string, double> snapshot() const {
        std::shared_lock lock(mutex_);
        std::unordered_map<std::string, double> result;
        for (const auto& [k, v] : gauges_) {
            result[k] = v.value();
        }
        return result;
    }

    void reset() noexcept {
        std::unique_lock lock(mutex_);
        gauges_.clear();
    }

private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Gauge> gauges_;
};

} // namespace nexus::utility::metrics
