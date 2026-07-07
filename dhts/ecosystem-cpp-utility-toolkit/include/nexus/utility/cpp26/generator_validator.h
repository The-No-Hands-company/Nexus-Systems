#pragma once
#include <string>
#include <vector>
#include <chrono>
#include <source_location>
#include <unordered_map>

namespace nexus::utility::cpp26 {

/// Validate and trace C++26 std::generator usage (reference stability, recursion)
class GeneratorValidator {
public:
    struct GeneratorStats {
        std::string name;
        size_t valuesYielded = 0;
        size_t maxRecursionDepth = 0;
        std::chrono::microseconds totalYieldTime{0};
        std::chrono::microseconds maxYieldTime{0};
        bool finished = false;
        std::source_location creationSite;
    };

    size_t registerGenerator(const std::string& name,
                             std::source_location loc = std::source_location::current()) {
        size_t id = nextId_++;
        stats_[id] = {name, 0, 0, std::chrono::microseconds(0),
                      std::chrono::microseconds(0), false, loc};
        return id;
    }

    void recordYield(size_t id, std::chrono::microseconds yieldTime) {
        auto it = stats_.find(id);
        if (it == stats_.end()) return;
        it->second.valuesYielded++;
        it->second.totalYieldTime += yieldTime;
        it->second.maxYieldTime = std::max(it->second.maxYieldTime, yieldTime);
    }

    void recordRecursion(size_t id, size_t depth) {
        auto it = stats_.find(id);
        if (it != stats_.end())
            it->second.maxRecursionDepth = std::max(it->second.maxRecursionDepth, depth);
    }

    void recordFinished(size_t id) {
        auto it = stats_.find(id);
        if (it != stats_.end()) it->second.finished = true;
    }

    GeneratorStats getStats(size_t id) const {
        auto it = stats_.find(id);
        return it != stats_.end() ? it->second : GeneratorStats{};
    }

    std::vector<GeneratorStats> getAllStats() const {
        std::vector<GeneratorStats> r;
        for (auto& [id, s] : stats_) r.push_back(s);
        return r;
    }

    size_t activeGenerators() const {
        size_t c = 0;
        for (auto& [id, s] : stats_) if (!s.finished) c++;
        return c;
    }

    void clear() { stats_.clear(); nextId_ = 0; }

private:
    size_t nextId_ = 0;
    std::unordered_map<size_t, GeneratorStats> stats_;
};
} // namespace nexus::utility::cpp26
