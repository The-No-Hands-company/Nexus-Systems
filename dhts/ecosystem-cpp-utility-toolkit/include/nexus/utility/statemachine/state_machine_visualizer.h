#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include <functional>
#include <ostream>
#include <sstream>

namespace nexus::utility::statemachine {

/// @brief State transition information
template<typename StateType>
struct StateTransition {
    StateType from_state;
    StateType to_state;
    std::string event_name;
    std::chrono::steady_clock::time_point timestamp;
    std::string context;
};

/// @brief Track state machine transitions
template<typename StateType>
class StateMachineVisualizer {
public:
    void recordTransition(StateType from, StateType to,
                         const std::string& event,
                         const std::string& context = "");

    std::vector<StateTransition<StateType>> getHistory() const;
    void clearHistory();

    /// Generate GraphViz DOT format
    std::string generateDOT(const std::function<std::string(StateType)>& state_to_string) const;

    void printHistory(std::ostream& os,
                     const std::function<std::string(StateType)>& state_to_string) const;

private:
    mutable std::mutex mutex_;
    std::vector<StateTransition<StateType>> history_;
};

/// @brief Validate state transitions
template<typename StateType>
class TransitionValidator {
public:
    using TransitionPredicate = std::function<bool(StateType from, StateType to)>;

    void registerValidTransition(StateType from, StateType to);
    void registerTransitionRule(TransitionPredicate predicate);

    bool isValidTransition(StateType from, StateType to) const;
    void validateOrThrow(StateType from, StateType to) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<StateType, std::vector<StateType>> valid_transitions_;
    std::vector<TransitionPredicate> rules_;
};

// Template implementations

template<typename StateType>
void StateMachineVisualizer<StateType>::recordTransition(
    StateType from, StateType to,
    const std::string& event,
    const std::string& context) {

    std::lock_guard<std::mutex> lock(mutex_);
    history_.push_back({
        from, to, event,
        std::chrono::steady_clock::now(),
        context
    });
}

template<typename StateType>
std::vector<StateTransition<StateType>>
StateMachineVisualizer<StateType>::getHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return history_;
}

template<typename StateType>
void StateMachineVisualizer<StateType>::clearHistory() {
    std::lock_guard<std::mutex> lock(mutex_);
    history_.clear();
}

template<typename StateType>
std::string StateMachineVisualizer<StateType>::generateDOT(
    const std::function<std::string(StateType)>& state_to_string) const {

    std::lock_guard<std::mutex> lock(mutex_);
    std::ostringstream oss;

    oss << "digraph StateMachine {\n";
    oss << "  rankdir=LR;\n";
    oss << "  node [shape=circle];\n\n";

    for (const auto& trans : history_) {
        oss << "  \"" << state_to_string(trans.from_state) << "\" -> \""
            << state_to_string(trans.to_state) << "\" [label=\""
            << trans.event_name << "\"];\n";
    }

    oss << "}\n";
    return oss.str();
}

template<typename StateType>
void StateMachineVisualizer<StateType>::printHistory(
    std::ostream& os,
    const std::function<std::string(StateType)>& state_to_string) const {

    std::lock_guard<std::mutex> lock(mutex_);

    os << "\n=== State Machine History ===\n";
    for (const auto& trans : history_) {
        os << state_to_string(trans.from_state) << " --["
           << trans.event_name << "]--> "
           << state_to_string(trans.to_state);
        if (!trans.context.empty()) {
            os << " (" << trans.context << ")";
        }
        os << "\n";
    }
    os << "=============================\n";
}

template<typename StateType>
void TransitionValidator<StateType>::registerValidTransition(StateType from, StateType to) {
    std::lock_guard<std::mutex> lock(mutex_);
    valid_transitions_[from].push_back(to);
}

template<typename StateType>
void TransitionValidator<StateType>::registerTransitionRule(TransitionPredicate predicate) {
    std::lock_guard<std::mutex> lock(mutex_);
    rules_.push_back(predicate);
}

template<typename StateType>
bool TransitionValidator<StateType>::isValidTransition(StateType from, StateType to) const {
    std::lock_guard<std::mutex> lock(mutex_);

    // Check explicit valid transitions
    auto it = valid_transitions_.find(from);
    if (it != valid_transitions_.end()) {
        const auto& valid = it->second;
        if (std::find(valid.begin(), valid.end(), to) != valid.end()) {
            return true;
        }
    }

    // Check rules
    for (const auto& rule : rules_) {
        if (rule(from, to)) {
            return true;
        }
    }

    return false;
}

template<typename StateType>
void TransitionValidator<StateType>::validateOrThrow(StateType from, StateType to) const {
    if (!isValidTransition(from, to)) {
        throw std::logic_error("Invalid state transition");
    }
}

} // namespace nexus::utility::statemachine
