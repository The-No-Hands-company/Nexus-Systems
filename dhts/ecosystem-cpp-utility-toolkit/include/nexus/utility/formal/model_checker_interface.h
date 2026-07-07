#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <mutex>
#include <algorithm>
#include <cstddef>

namespace nexus::utility::formal {

/**
 * @brief Minimal explicit-state model checker: states and guarded transitions.
 *
 * Build a state graph via addState()/addTransition() then compute the set of
 * states reachable from a given start state via breadth-first search.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Formal
 * @version 1.0.0
 */
class ModelCheckerInterface {
public:
    struct Transition {
        std::string from;
        std::string to;
        std::string condition;
    };

    /// @brief Get singleton instance
    static ModelCheckerInterface& instance() {
        static ModelCheckerInterface inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        states_.clear();
        adjacency_.clear();
        transitions_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        states_.clear();
        adjacency_.clear();
        transitions_.clear();
    }

    /// @brief Check if the utility is currently enabled
    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Enable the utility
    void enable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = true;
    }

    /// @brief Disable the utility
    void disable() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
    }

    // ── Domain methods ──────────────────────────────────────────────────────

    /// @brief Add a state to the model.
    void addState(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);
        states_.insert(name);
    }

    /// @brief Add a guarded transition between two states (states are created if
    /// they do not yet exist).
    void addTransition(const std::string& from, const std::string& to,
                       const std::string& condition) {
        std::lock_guard<std::mutex> lock(mutex_);
        states_.insert(from);
        states_.insert(to);
        adjacency_[from].push_back(to);
        transitions_.push_back({from, to, condition});
    }

    /// @brief Compute all states reachable from `from` via one or more
    /// transitions, using breadth-first search. The start state is included only
    /// if it is reachable from itself (i.e. lies on a cycle).
    std::vector<std::string> reachableStates(const std::string& from) const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::set<std::string> visited;
        std::queue<std::string> q;
        auto it = adjacency_.find(from);
        if (it != adjacency_.end()) {
            for (const auto& next : it->second) {
                if (visited.insert(next).second) q.push(next);
            }
        }
        while (!q.empty()) {
            std::string cur = q.front();
            q.pop();
            auto ai = adjacency_.find(cur);
            if (ai == adjacency_.end()) continue;
            for (const auto& next : ai->second) {
                if (visited.insert(next).second) q.push(next);
            }
        }
        return {visited.begin(), visited.end()};
    }

    /// @brief Whether `to` is reachable from `from`.
    bool isReachable(const std::string& from, const std::string& to) const {
        auto reachable = reachableStates(from);
        return std::find(reachable.begin(), reachable.end(), to) != reachable.end();
    }

    /// @brief Number of states in the model.
    std::size_t getStateCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return states_.size();
    }

    /// @brief Number of transitions in the model.
    std::size_t getTransitionCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return transitions_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "ModelCheckerInterface[states=" + std::to_string(states_.size()) +
               ", transitions=" + std::to_string(transitions_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        states_.clear();
        adjacency_.clear();
        transitions_.clear();
    }

private:
    ModelCheckerInterface() = default;
    ~ModelCheckerInterface() = default;

    ModelCheckerInterface(const ModelCheckerInterface&) = delete;
    ModelCheckerInterface& operator=(const ModelCheckerInterface&) = delete;
    ModelCheckerInterface(ModelCheckerInterface&&) = delete;
    ModelCheckerInterface& operator=(ModelCheckerInterface&&) = delete;

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::set<std::string> states_;
    std::map<std::string, std::vector<std::string>> adjacency_;
    std::vector<Transition> transitions_;
};

} // namespace nexus::utility::formal
