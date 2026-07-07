#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

namespace nexus::utility::cpp23 {

/// @brief Track C++20/23 module dependencies and detect circular dependencies.
class ModuleDependencyTracker {
public:
    static ModuleDependencyTracker& instance() {
        static ModuleDependencyTracker inst;
        return inst;
    }

    void initialize() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        graph_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    void addModule(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        graph_.try_emplace(name);
    }

    void addDependency(const std::string& module, const std::string& depends_on) {
        std::lock_guard<std::mutex> lock(mutex_);
        graph_[module].push_back(depends_on);
        graph_.try_emplace(depends_on);
    }

    std::vector<std::string> getDependencies(const std::string& module) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = graph_.find(module);
        return it != graph_.end() ? it->second : std::vector<std::string>{};
    }

    /// Returns true if a dependency cycle exists (DFS-based detection).
    bool detectCycle() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> in_stack;
        for (const auto& [node, deps] : graph_) {
            if (visited.find(node) == visited.end()) {
                if (dfs(node, visited, in_stack)) return true;
            }
        }
        return false;
    }

    size_t getModuleCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return graph_.size();
    }

private:
    ModuleDependencyTracker() = default;
    ~ModuleDependencyTracker() = default;
    ModuleDependencyTracker(const ModuleDependencyTracker&) = delete;
    ModuleDependencyTracker& operator=(const ModuleDependencyTracker&) = delete;

    bool dfs(const std::string& node,
             std::unordered_set<std::string>& visited,
             std::unordered_set<std::string>& in_stack) const {
        visited.insert(node);
        in_stack.insert(node);
        auto it = graph_.find(node);
        if (it != graph_.end()) {
            for (const auto& dep : it->second) {
                if (in_stack.count(dep)) return true;
                if (!visited.count(dep) && dfs(dep, visited, in_stack)) return true;
            }
        }
        in_stack.erase(node);
        return false;
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::unordered_map<std::string, std::vector<std::string>> graph_;
};

} // namespace nexus::utility::cpp23
