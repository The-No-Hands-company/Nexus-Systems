#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <cstddef>

namespace nexus::utility::formal {

/**
 * @brief Helper for symbolic execution: record execution paths and derive their
 * path conditions.
 *
 * A path is a named sequence of instructions. The path condition is the
 * conjunction of the branch conditions / assumptions found along the path.
 * Instructions of the form "assume(<cond>)" or containing a relational operator
 * contribute to the path condition.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Formal
 * @version 1.0.0
 */
class SymbolicExecutionHelper {
public:
    struct Path {
        std::vector<std::string> instructions;
        std::string condition;
    };

    /// @brief Get singleton instance
    static SymbolicExecutionHelper& instance() {
        static SymbolicExecutionHelper inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        paths_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        paths_.clear();
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

    /// @brief Execute (record) a named path made of a list of instructions. The
    /// path condition is derived from any branch/assume instructions.
    void executePath(const std::string& path_name,
                     const std::vector<std::string>& instructions) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled_) return;
        Path p;
        p.instructions = instructions;
        p.condition = deriveCondition(instructions);
        paths_[path_name] = std::move(p);
    }

    /// @brief Get the derived path condition for a named path (empty if unknown).
    std::string getPathCondition(const std::string& path_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = paths_.find(path_name);
        return it == paths_.end() ? std::string{} : it->second.condition;
    }

    /// @brief Get the instructions recorded for a named path.
    std::vector<std::string> getPathInstructions(const std::string& path_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = paths_.find(path_name);
        return it == paths_.end() ? std::vector<std::string>{} : it->second.instructions;
    }

    /// @brief Whether a path with the given name has been recorded.
    bool hasPath(const std::string& path_name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return paths_.find(path_name) != paths_.end();
    }

    /// @brief Number of recorded paths.
    std::size_t getPathCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return paths_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "SymbolicExecutionHelper[paths=" + std::to_string(paths_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        paths_.clear();
    }

private:
    SymbolicExecutionHelper() = default;
    ~SymbolicExecutionHelper() = default;

    SymbolicExecutionHelper(const SymbolicExecutionHelper&) = delete;
    SymbolicExecutionHelper& operator=(const SymbolicExecutionHelper&) = delete;
    SymbolicExecutionHelper(SymbolicExecutionHelper&&) = delete;
    SymbolicExecutionHelper& operator=(SymbolicExecutionHelper&&) = delete;

    static bool isCondition(const std::string& instr) {
        if (instr.rfind("assume", 0) == 0) return true;
        static const std::string ops[] = {"==", "!=", "<=", ">=", "<", ">"};
        for (const auto& op : ops) {
            if (instr.find(op) != std::string::npos) return true;
        }
        return false;
    }

    static std::string extractCondition(const std::string& instr) {
        if (instr.rfind("assume", 0) == 0) {
            auto open = instr.find('(');
            auto close = instr.rfind(')');
            if (open != std::string::npos && close != std::string::npos && close > open) {
                return instr.substr(open + 1, close - open - 1);
            }
        }
        return instr;
    }

    static std::string deriveCondition(const std::vector<std::string>& instructions) {
        std::string cond;
        for (const auto& instr : instructions) {
            if (!isCondition(instr)) continue;
            std::string part = extractCondition(instr);
            if (part.empty()) continue;
            if (!cond.empty()) cond += " && ";
            cond += part;
        }
        return cond.empty() ? "true" : cond;
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, Path> paths_;
};

} // namespace nexus::utility::formal
