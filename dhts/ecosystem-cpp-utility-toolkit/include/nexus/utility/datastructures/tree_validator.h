#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace nexus::utility::datastructures {

/// @brief Validates tree structures (single root, no cycles) using a parent/child adjacency.
class TreeValidator {
public:
    static TreeValidator& instance() {
        static TreeValidator inst;
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
        children_.clear();
        nodes_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void insertNode(uint64_t parent, uint64_t child) {
        std::lock_guard<std::mutex> lock(mutex_);
        children_[parent].push_back(child);
        children_.try_emplace(child);
        nodes_.insert(parent);
        nodes_.insert(child);
    }

    void removeNode(uint64_t node) {
        std::lock_guard<std::mutex> lock(mutex_);
        children_.erase(node);
        for (auto& [_, kids] : children_) {
            for (auto it = kids.begin(); it != kids.end();) {
                if (*it == node) {
                    it = kids.erase(it);
                } else {
                    ++it;
                }
            }
        }
        nodes_.erase(node);
    }

    /// @brief A valid tree has exactly one root, every node at most one parent, and no cycles.
    bool isTree() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (nodes_.empty()) return true;

        std::map<uint64_t, int> indegree;
        for (uint64_t node : nodes_) indegree[node] = 0;
        for (const auto& [_, kids] : children_) {
            for (uint64_t child : kids) {
                if (++indegree[child] > 1) {
                    return false; // multiple parents
                }
            }
        }

        int roots = 0;
        uint64_t root = 0;
        for (uint64_t node : nodes_) {
            if (indegree[node] == 0) {
                ++roots;
                root = node;
            }
        }
        if (roots != 1) return false;

        std::set<uint64_t> visited;
        return !dfsHasCycle(root, visited) && visited.size() == nodes_.size();
    }

    size_t getHeight(uint64_t node) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::set<uint64_t> visiting;
        return heightOf(node, visiting);
    }

    size_t getNodeCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return nodes_.size();
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        children_.clear();
        nodes_.clear();
    }

private:
    TreeValidator() = default;
    ~TreeValidator() = default;
    TreeValidator(const TreeValidator&) = delete;
    TreeValidator& operator=(const TreeValidator&) = delete;

    bool dfsHasCycle(uint64_t node, std::set<uint64_t>& visited) const {
        if (!visited.insert(node).second) return true;
        auto it = children_.find(node);
        if (it != children_.end()) {
            for (uint64_t child : it->second) {
                if (dfsHasCycle(child, visited)) return true;
            }
        }
        return false;
    }

    size_t heightOf(uint64_t node, std::set<uint64_t>& visiting) const {
        auto it = children_.find(node);
        if (it == children_.end() || it->second.empty()) return 0;
        if (!visiting.insert(node).second) return 0; // guard against cycles
        size_t best = 0;
        for (uint64_t child : it->second) {
            size_t h = heightOf(child, visiting) + 1;
            if (h > best) best = h;
        }
        visiting.erase(node);
        return best;
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<uint64_t, std::vector<uint64_t>> children_;
    std::set<uint64_t> nodes_;
};

} // namespace nexus::utility::datastructures
