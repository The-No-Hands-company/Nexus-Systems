#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <cstdint>

namespace nexus::utility::orchestration {

/**
 * @brief Correlate events across systems by timestamp proximity and shared tags.
 */
class CrossSystemCorrelator {
public:
    struct Event {
        std::string system;
        std::string name;
        std::uint64_t timestampNs = 0;
        std::vector<std::string> tags;
    };

    static CrossSystemCorrelator& instance() {
        static CrossSystemCorrelator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void recordEvent(const Event& e) {
        std::lock_guard<std::mutex> lk(mutex_);
        events_.push_back(e);
    }

    /// Return events within +/- windowNs of the reference timestamp that share a tag.
    std::vector<Event> correlate(std::uint64_t referenceTimestampNs,
                                 std::uint64_t windowNs,
                                 const std::string& tag) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Event> out;
        for (const auto& e : events_) {
            std::uint64_t diff = e.timestampNs > referenceTimestampNs
                ? e.timestampNs - referenceTimestampNs
                : referenceTimestampNs - e.timestampNs;
            bool tagMatch = tag.empty() ||
                std::find(e.tags.begin(), e.tags.end(), tag) != e.tags.end();
            if (diff <= windowNs && tagMatch) out.push_back(e);
        }
        return out;
    }

    /// Group all events into time-ordered clusters no wider than windowNs.
    std::vector<std::vector<Event>> cluster(std::uint64_t windowNs) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<Event> sorted = events_;
        std::sort(sorted.begin(), sorted.end(),
                  [](const Event& a, const Event& b) { return a.timestampNs < b.timestampNs; });
        std::vector<std::vector<Event>> clusters;
        for (const auto& e : sorted) {
            if (clusters.empty() ||
                e.timestampNs - clusters.back().front().timestampNs > windowNs) {
                clusters.push_back({e});
            } else {
                clusters.back().push_back(e);
            }
        }
        return clusters;
    }

    std::size_t eventCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return events_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        events_.clear();
    }

private:
    CrossSystemCorrelator() = default;
    ~CrossSystemCorrelator() = default;
    CrossSystemCorrelator(const CrossSystemCorrelator&) = delete;
    CrossSystemCorrelator& operator=(const CrossSystemCorrelator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<Event> events_;
};

} // namespace nexus::utility::orchestration
