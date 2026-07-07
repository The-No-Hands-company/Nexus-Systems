#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <string>
#include <vector>

namespace nexus::utility::datastructures {

/// @brief Validates directed graph structure using an adjacency list.
class GraphValidator {
public:
    static GraphValidator& instance() {
        static GraphValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
        config_ = config;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        adjacency_.clear();
        nodes_.clear();
        edge_count_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void addEdge(uint64_t from, uint64_t to) {
        std::lock_guard<std::mutex> lock(mutex_);
        adjacency_[from].push_back(to);
        nodes_.insert(from);
        nodes_.insert(to);
        adjacency_.try_emplace(to);
        ++edge_count_;
    }

    /// @brief Detect a cycle in the directed graph via DFS (white/gray/black coloring).
    bool hasCycle() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<uint64_t, int> color; // 0=white, 1=gray, 2=black
        for (uint64_t node : nodes_) {
            if (color[node] == 0 && dfsCycle(node, color)) {
                return true;
            }
        }
        return false;
    }

    /// @brief Check weak connectivity via BFS treating edges as undirected.
    bool isConnected() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (nodes_.empty()) return true;

        std::map<uint64_t, std::vector<uint64_t>> undirected;
        for (const auto& [from, tos] : adjacency_) {
            for (uint64_t to : tos) {
                undirected[from].push_back(to);
                undirected[to].push_back(from);
            }
        }

        std::set<uint64_t> visited;
        std::queue<uint64_t> frontier;
        uint64_t start = *nodes_.begin();
        frontier.push(start);
        visited.insert(start);
        while (!frontier.empty()) {
            uint64_t cur = frontier.front();
            frontier.pop();
            for (uint64_t next : undirected[cur]) {
                if (visited.insert(next).second) {
                    frontier.push(next);
                }
            }
        }
        return visited.size() == nodes_.size();
    }

    size_t getNodeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return nodes_.size();
    }

    size_t getEdgeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return edge_count_;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        adjacency_.clear();
        nodes_.clear();
        edge_count_ = 0;
    }

private:
    GraphValidator() = default;
    ~GraphValidator() = default;
    GraphValidator(const GraphValidator&) = delete;
    GraphValidator& operator=(const GraphValidator&) = delete;

    bool dfsCycle(uint64_t node, std::map<uint64_t, int>& color) const {
        color[node] = 1;
        auto it = adjacency_.find(node);
        if (it != adjacency_.end()) {
            for (uint64_t next : it->second) {
                if (color[next] == 1) return true;
                if (color[next] == 0 && dfsCycle(next, color)) return true;
            }
        }
        color[node] = 2;
        return false;
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<uint64_t, std::vector<uint64_t>> adjacency_;
    std::set<uint64_t> nodes_;
    size_t edge_count_ = 0;
};

} // namespace nexus::utility::datastructures
