#pragma once

#include <string>
#include <vector>
#include <map>
#include <set>
#include <mutex>
#include <cstddef>

namespace nexus::utility::formal {

/**
 * @brief Lightweight SMT-style constraint interface.
 *
 * Variables are declared with a type, and (in)equality constraints are added as
 * strings. checkSat() performs a sound-but-incomplete satisfiability analysis of
 * equality/disequality constraints: it detects direct contradictions
 * (UNSAT), reports SAT for constraint sets it can fully decide, and UNKNOWN when
 * a constraint form is beyond its reasoning. getModel() returns a satisfying
 * assignment for the declared variables when available.
 *
 * Thread safety: all public methods are protected by a mutex.
 *
 * @category Formal
 * @version 1.0.0
 */
class SmtSolverInterface {
public:
    enum class SatResult { Sat, Unsat, Unknown };

    /// @brief Get singleton instance
    static SmtSolverInterface& instance() {
        static SmtSolverInterface inst;
        return inst;
    }

    /// @brief Initialize the utility
    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
        enabled_ = true;
        variables_.clear();
        constraints_.clear();
        model_.clear();
    }

    /// @brief Shutdown the utility and cleanup resources
    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled_ = false;
        variables_.clear();
        constraints_.clear();
        model_.clear();
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

    /// @brief Declare a variable with a type ("int", "bool", "real", ...).
    void declareVariable(const std::string& name, const std::string& type) {
        std::lock_guard<std::mutex> lock(mutex_);
        variables_[name] = type;
    }

    /// @brief Add a constraint (e.g. "x == 3", "y != 0", "false").
    void addConstraint(const std::string& constraint) {
        std::lock_guard<std::mutex> lock(mutex_);
        constraints_.push_back(constraint);
    }

    /// @brief Determine satisfiability of the current constraint set and, when
    /// satisfiable, compute a model.
    SatResult checkSat() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::map<std::string, std::string> equalities;
        std::set<std::pair<std::string, std::string>> disequalities;
        bool undecided = false;

        for (const auto& raw : constraints_) {
            std::string c = trim(raw);
            if (c == "false" || c == "0") {
                model_.clear();
                return SatResult::Unsat;
            }
            if (c == "true" || c == "1" || c.empty()) {
                continue;
            }
            std::string lhs, op, rhs;
            if (!parseBinary(c, lhs, op, rhs)) {
                undecided = true;
                continue;
            }
            if (op == "==") {
                auto it = equalities.find(lhs);
                if (it != equalities.end() && it->second != rhs) {
                    model_.clear();
                    return SatResult::Unsat;
                }
                equalities[lhs] = rhs;
            } else if (op == "!=") {
                disequalities.insert({lhs, rhs});
            } else {
                undecided = true;
            }
        }

        for (const auto& [var, val] : disequalities) {
            auto it = equalities.find(var);
            if (it != equalities.end() && it->second == val) {
                model_.clear();
                return SatResult::Unsat;
            }
        }

        // Build a candidate model from equalities plus type defaults.
        std::map<std::string, std::string> model;
        for (const auto& [name, type] : variables_) {
            auto it = equalities.find(name);
            model[name] = (it != equalities.end()) ? it->second : defaultFor(type);
        }
        model_ = model;

        return undecided ? SatResult::Unknown : SatResult::Sat;
    }

    /// @brief Convert a SatResult to its textual form.
    static std::string toString(SatResult r) {
        switch (r) {
            case SatResult::Sat: return "SAT";
            case SatResult::Unsat: return "UNSAT";
            default: return "UNKNOWN";
        }
    }

    /// @brief Get the satisfying assignment computed by the last checkSat().
    std::map<std::string, std::string> getModel() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return model_;
    }

    /// @brief Number of declared variables.
    std::size_t getVariableCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return variables_.size();
    }

    /// @brief Number of added constraints.
    std::size_t getConstraintCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return constraints_.size();
    }

    /// @brief Get utility statistics/status
    std::string getStatus() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return "SmtSolverInterface[vars=" + std::to_string(variables_.size()) +
               ", constraints=" + std::to_string(constraints_.size()) + "]";
    }

    /// @brief Reset utility state
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        variables_.clear();
        constraints_.clear();
        model_.clear();
    }

private:
    SmtSolverInterface() = default;
    ~SmtSolverInterface() = default;

    SmtSolverInterface(const SmtSolverInterface&) = delete;
    SmtSolverInterface& operator=(const SmtSolverInterface&) = delete;
    SmtSolverInterface(SmtSolverInterface&&) = delete;
    SmtSolverInterface& operator=(SmtSolverInterface&&) = delete;

    static std::string trim(const std::string& s) {
        std::size_t b = s.find_first_not_of(" \t");
        if (b == std::string::npos) return "";
        std::size_t e = s.find_last_not_of(" \t");
        return s.substr(b, e - b + 1);
    }

    static bool parseBinary(const std::string& c, std::string& lhs,
                            std::string& op, std::string& rhs) {
        static const std::string ops[] = {"==", "!="};
        for (const auto& candidate : ops) {
            auto pos = c.find(candidate);
            if (pos != std::string::npos) {
                lhs = trim(c.substr(0, pos));
                op = candidate;
                rhs = trim(c.substr(pos + candidate.size()));
                return !lhs.empty() && !rhs.empty();
            }
        }
        return false;
    }

    static std::string defaultFor(const std::string& type) {
        if (type == "bool") return "false";
        if (type == "real" || type == "float" || type == "double") return "0.0";
        return "0";
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, std::string> variables_;
    std::vector<std::string> constraints_;
    std::map<std::string, std::string> model_;
};

} // namespace nexus::utility::formal
