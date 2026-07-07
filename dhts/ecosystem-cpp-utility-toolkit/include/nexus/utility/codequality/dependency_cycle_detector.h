#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <mutex>
#include <atomic>

namespace nexus::utility::codequality {

/**
 * @brief Detect cycles in a dependency graph using depth-first search.
 */
class DependencyCycleDetector {
public:
    static DependencyCycleDetector& instance() {
        static DependencyCycleDetector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void addEdge(const std::string& from, const std::string& to) {
        std::lock_guard<std::mutex> lk(mutex_);
        graph_[from].insert(to);
        graph_[to];
    }

    bool hasCycle() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::unordered_set<std::string> visited, inStack;
        for (const auto& [node, _] : graph_) {
            if (!visited.count(node) && dfsCycle(node, visited, inStack, nullptr))
                return true;
        }
        return false;
    }

    std::vector<std::vector<std::string>> findCycles() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<std::vector<std::string>> cycles;
        std::unordered_set<std::string> visited, inStack;
        std::vector<std::string> path;
        for (const auto& [node, _] : graph_) {
            if (!visited.count(node))
                dfsCollect(node, visited, inStack, path, cycles);
        }
        return cycles;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        graph_.clear();
    }

private:
    DependencyCycleDetector() = default;
    ~DependencyCycleDetector() = default;
    DependencyCycleDetector(const DependencyCycleDetector&) = delete;
    DependencyCycleDetector& operator=(const DependencyCycleDetector&) = delete;

    bool dfsCycle(const std::string& node,
                  std::unordered_set<std::string>& visited,
                  std::unordered_set<std::string>& inStack,
                  std::vector<std::string>* /*unused*/) const {
        visited.insert(node);
        inStack.insert(node);
        auto it = graph_.find(node);
        if (it != graph_.end()) {
            for (const auto& next : it->second) {
                if (inStack.count(next)) return true;
                if (!visited.count(next) && dfsCycle(next, visited, inStack, nullptr))
                    return true;
            }
        }
        inStack.erase(node);
        return false;
    }

    void dfsCollect(const std::string& node,
                    std::unordered_set<std::string>& visited,
                    std::unordered_set<std::string>& inStack,
                    std::vector<std::string>& path,
                    std::vector<std::vector<std::string>>& cycles) const {
        visited.insert(node);
        inStack.insert(node);
        path.push_back(node);
        auto it = graph_.find(node);
        if (it != graph_.end()) {
            for (const auto& next : it->second) {
                if (inStack.count(next)) {
                    std::vector<std::string> cyc;
                    auto start = std::find(path.begin(), path.end(), next);
                    for (auto p = start; p != path.end(); ++p) cyc.push_back(*p);
                    cyc.push_back(next);
                    cycles.push_back(cyc);
                } else if (!visited.count(next)) {
                    dfsCollect(next, visited, inStack, path, cycles);
                }
            }
        }
        path.pop_back();
        inStack.erase(node);
    }

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, std::unordered_set<std::string>> graph_;
};

} // namespace nexus::utility::codequality
