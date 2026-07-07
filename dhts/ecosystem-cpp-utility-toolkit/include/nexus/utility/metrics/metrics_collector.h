#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <vector>

#include <nexus/utility/metrics/counter.h>
#include <nexus/utility/metrics/gauge.h>
#include <nexus/utility/metrics/histogram.h>

namespace nexus::utility::metrics {

class MetricsCollector {
public:
    [[nodiscard]] static MetricsCollector& instance() noexcept {
        static MetricsCollector collector;
        return collector;
    }

    // ── Counter Operations ──────────────────────────────────────────────────
    void incCounter(std::string_view name, int64_t delta = 1) noexcept {
        getOrCreateCounter(name).inc(delta);
    }

    [[nodiscard]] int64_t getCounter(std::string_view name) const noexcept {
        std::shared_lock lock(mutex_);
        auto it = counters_.find(std::string(name));
        return it != counters_.end() ? it->second->value() : 0;
    }

    // ── Gauge Operations ────────────────────────────────────────────────────
    void setGauge(std::string_view name, double value) noexcept {
        getOrCreateGauge(name).set(value);
    }

    [[nodiscard]] double getGauge(std::string_view name) const noexcept {
        std::shared_lock lock(mutex_);
        auto it = gauges_.find(std::string(name));
        return it != gauges_.end() ? it->second->value() : 0.0;
    }

    // ── Histogram Operations ────────────────────────────────────────────────
    void observeHistogram(std::string_view name, double value) noexcept {
        getOrCreateHistogram(name).observe(value);
    }

    [[nodiscard]] Histogram::Snapshot getHistogramSnapshot(std::string_view name) const {
        std::shared_lock lock(mutex_);
        auto it = histograms_.find(std::string(name));
        if (it != histograms_.end()) {
            return it->second->snapshot();
        }
        return {};
    }

    // ── Export ──────────────────────────────────────────────────────────────
    /// @brief Export all metrics in Prometheus text format
    [[nodiscard]] std::string exportPrometheus() const {
        std::shared_lock lock(mutex_);
        std::ostringstream oss;

        for (const auto& [name, counter] : counters_) {
            oss << "# TYPE " << name << " counter\n";
            oss << name << " " << counter->value() << "\n";
        }

        for (const auto& [name, gauge] : gauges_) {
            oss << "# TYPE " << name << " gauge\n";
            oss << name << " " << gauge->value() << "\n";
        }

        for (const auto& [name, hist] : histograms_) {
            auto snap = hist->snapshot();
            oss << "# TYPE " << name << " histogram\n";
            oss << name << "_count " << snap.count << "\n";
            oss << name << "_sum " << snap.sum << "\n";
            oss << name << "_min " << snap.min << "\n";
            oss << name << "_max " << snap.max << "\n";
            oss << name << "_mean " << snap.mean << "\n";
            oss << name << "_p50 " << snap.p50 << "\n";
            oss << name << "_p95 " << snap.p95 << "\n";
            oss << name << "_p99 " << snap.p99 << "\n";
            for (size_t i = 0; i < snap.bucket_bounds.size(); ++i) {
                oss << name << "_bucket{le=\"" << snap.bucket_bounds[i] << "\"} "
                    << snap.bucket_counts[i] << "\n";
            }
            uint64_t inf_count = snap.bucket_counts.size() > snap.bucket_bounds.size()
                                     ? snap.bucket_counts.back()
                                     : 0;
            oss << name << "_bucket{le=\"+Inf\"} " << inf_count << "\n";
        }

        return oss.str();
    }

    /// @brief Reset all metrics
    void reset() noexcept {
        std::unique_lock lock(mutex_);
        counters_.clear();
        gauges_.clear();
        histograms_.clear();
    }

private:
    MetricsCollector() = default;

    Counter& getOrCreateCounter(std::string_view name) {
        std::unique_lock lock(mutex_);
        auto& ptr = counters_[std::string(name)];
        if (!ptr) {
            ptr = std::make_unique<Counter>();
        }
        return *ptr;
    }

    Gauge& getOrCreateGauge(std::string_view name) {
        std::unique_lock lock(mutex_);
        auto& ptr = gauges_[std::string(name)];
        if (!ptr) {
            ptr = std::make_unique<Gauge>();
        }
        return *ptr;
    }

    Histogram& getOrCreateHistogram(std::string_view name) {
        std::unique_lock lock(mutex_);
        auto& ptr = histograms_[std::string(name)];
        if (!ptr) {
            ptr = std::make_unique<Histogram>();
            ptr->configureExponential(2.0, 0, 32);
        }
        return *ptr;
    }

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<Counter>> counters_;
    std::unordered_map<std::string, std::unique_ptr<Gauge>> gauges_;
    std::unordered_map<std::string, std::unique_ptr<Histogram>> histograms_;
};

} // namespace nexus::utility::metrics
