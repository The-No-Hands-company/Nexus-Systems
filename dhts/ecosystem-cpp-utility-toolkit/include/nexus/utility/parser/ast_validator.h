#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace nexus::utility::parser {

/// @brief Validates AST nodes (child-count ranges) and overall tree acyclicity.
class AstValidator {
public:
    static AstValidator& instance() {
        static AstValidator inst;
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
        error_count_ = 0;
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Validate that a node's child count falls within [expected_min, expected_max].
    /// @return true if within range, false (and records an error) otherwise.
    bool validateNode(const std::string& node_type, int child_count, int expected_min, int expected_max) {
        std::lock_guard<std::mutex> lock(mutex_);
        (void)node_type;
        if (child_count < expected_min || child_count > expected_max) {
            ++error_count_;
            return false;
        }
        return true;
    }

    /// @brief Register a parent-child edge so the tree can be checked for cycles.
    void addEdge(uint64_t parent, uint64_t child) {
        std::lock_guard<std::mutex> lock(mutex_);
        adjacency_[parent].push_back(child);
        adjacency_.try_emplace(child);
        nodes_.insert(parent);
        nodes_.insert(child);
    }

    /// @brief Validate the registered tree contains no cycles.
    /// @return true if acyclic; false (and records an error) if a cycle exists.
    bool validateTree() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<uint64_t, int> color;
        for (uint64_t node : nodes_) {
            if (color[node] == 0 && dfsCycle(node, color)) {
                ++error_count_;
                return false;
            }
        }
        return true;
    }

    size_t getErrorCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return error_count_;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        adjacency_.clear();
        nodes_.clear();
        error_count_ = 0;
    }

private:
    AstValidator() = default;
    ~AstValidator() = default;
    AstValidator(const AstValidator&) = delete;
    AstValidator& operator=(const AstValidator&) = delete;

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
    size_t error_count_ = 0;
};

} // namespace nexus::utility::parser
