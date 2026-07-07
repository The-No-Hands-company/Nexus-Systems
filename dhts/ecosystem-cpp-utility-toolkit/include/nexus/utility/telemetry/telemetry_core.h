#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <random>
#include <map>
#include <cstdint>

namespace nexus::utility::telemetry {

/// OpenTelemetry-compatible span builder
class SpanBuilder {
public:
    struct SpanContext {
        std::string trace_id;
        std::string span_id;
        std::string parent_span_id;
        bool sampled = true;
    };

    struct Span {
        std::string name;
        SpanContext context;
        std::chrono::steady_clock::time_point start_time;
        std::chrono::steady_clock::time_point end_time;
        std::map<std::string, std::string> attributes;
        std::vector<std::string> events;
        int status_code = 0; // 0=unset, 1=ok, 2=error
        std::string status_message;
    };

    static SpanContext generateContext() {
        SpanContext ctx;
        ctx.trace_id = randomHex(32);
        ctx.span_id = randomHex(16);
        ctx.parent_span_id = "";
        return ctx;
    }

    static Span createSpan(const std::string& name) {
        return createSpan(name, SpanContext{});
    }
    static Span createSpan(const std::string& name, const SpanContext& parent) {
        Span s;
        s.name = name;
        s.context = parent.trace_id.empty() ? generateContext() : SpanContext{parent.trace_id, randomHex(16), parent.span_id};
        s.start_time = std::chrono::steady_clock::now();
        return s;
    }

    static std::chrono::microseconds duration(const Span& s) {
        return std::chrono::duration_cast<std::chrono::microseconds>(s.end_time - s.start_time);
    }

    static std::string randomHex(int len) {
        static std::mt19937_64 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, 15);
        const char* h = "0123456789abcdef";
        std::string s; for (int i=0; i<len; ++i) s += h[dist(rng)];
        return s;
    }
};

/// Trace sampler with various strategies
class TraceSampler {
public:
    enum class Strategy { AlwaysOn, AlwaysOff, Probability, RateLimiting };

    struct Config {
        Strategy strategy = Strategy::AlwaysOn;
        double probability = 0.1;
        double max_spans_per_second = 100;
    };

    explicit TraceSampler(Config c) : config_(c) {}

    bool shouldSample() {
        switch (config_.strategy) {
            case Strategy::AlwaysOn: return true;
            case Strategy::AlwaysOff: return false;
            case Strategy::Probability: return dist_(rng_) < config_.probability;
            case Strategy::RateLimiting: {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration<double>(now - last_check_).count();
                if (elapsed > 1.0) { count_ = 0; last_check_ = now; }
                return count_++ < config_.max_spans_per_second;
            }
        }
        return true;
    }

    Config config() const { return config_; }

private:
    Config config_;
    std::mt19937_64 rng_{std::random_device{}()};
    std::uniform_real_distribution<double> dist_{0, 1};
    std::chrono::steady_clock::time_point last_check_{std::chrono::steady_clock::now()};
    double count_ = 0;
};

/// Metrics aggregator
class MetricsAggregator {
public:
    struct MetricPoint { std::string name; double value; std::map<std::string, std::string> tags; std::chrono::steady_clock::time_point timestamp; };

    struct AggregatedMetric {
        double min, max, sum, avg;
        size_t count;
        double p50, p95, p99;
        std::chrono::steady_clock::time_point window_start;
    };

    static AggregatedMetric aggregate(const std::vector<MetricPoint>& points) {
        AggregatedMetric am;
        if (points.empty()) return am;

        std::vector<double> vals;
        am.min = std::numeric_limits<double>::max();
        am.max = std::numeric_limits<double>::lowest();
        am.count = points.size();
        am.sum = 0;

        for (auto& p : points) {
            vals.push_back(p.value);
            am.min = std::min(am.min, p.value);
            am.max = std::max(am.max, p.value);
            am.sum += p.value;
        }
        am.avg = am.sum / am.count;

        std::sort(vals.begin(), vals.end());
        am.p50 = vals[vals.size()/2];
        am.p95 = vals[static_cast<size_t>(vals.size()*0.95)];
        am.p99 = vals[static_cast<size_t>(vals.size()*0.99)];
        am.window_start = points.front().timestamp;

        return am;
    }

    static std::string report(const AggregatedMetric& am) {
        std::ostringstream oss; oss << std::fixed << std::setprecision(3);
        oss << "═══ Metrics Aggregate ═══\n";
        oss << "  Count: " << am.count << " | Min: " << am.min << " | Max: " << am.max << "\n";
        oss << "  Avg: " << am.avg << " | P50: " << am.p50 << " | P95: " << am.p95 << " | P99: " << am.p99 << "\n";
        return oss.str();
    }
};

/// Log correlation helper
class LogCorrelator {
public:
    struct LogEntry { std::string trace_id; std::string span_id; std::string level; std::string message; std::chrono::steady_clock::time_point time; };

    static std::vector<LogEntry> findByTrace(const std::vector<LogEntry>& logs, const std::string& trace_id) {
        std::vector<LogEntry> result;
        for (auto& l : logs) if (l.trace_id == trace_id) result.push_back(l);
        std::sort(result.begin(), result.end(), [](auto& a, auto& b){ return a.time < b.time; });
        return result;
    }

    static std::map<std::string, size_t> traceSummary(const std::vector<LogEntry>& logs) {
        std::map<std::string, size_t> counts;
        for (auto& l : logs) counts[l.trace_id]++;
        return counts;
    }
};

} // namespace nexus::utility::telemetry
