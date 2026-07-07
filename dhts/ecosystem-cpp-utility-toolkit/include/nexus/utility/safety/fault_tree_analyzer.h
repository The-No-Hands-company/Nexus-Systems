#pragma once

#include <cstddef>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

namespace nexus::utility::safety {

/// @brief Analyzes fault trees composed of AND/OR gates over basic events, computing the
///        probability of the top event bottom-up.
class FaultTreeAnalyzer {
public:
    static FaultTreeAnalyzer& instance() {
        static FaultTreeAnalyzer inst;
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
        gates_.clear();
        basic_events_.clear();
    }

    bool isEnabled() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return enabled_;
    }

    /// @brief Add a logic gate. @p type should be "AND" or "OR".
    void addGate(const std::string& name, const std::string& type,
                 const std::vector<std::string>& inputs) {
        std::lock_guard<std::mutex> lock(mutex_);
        gates_[name] = {type, inputs};
    }

    void setBasicEvent(const std::string& name, double probability) {
        std::lock_guard<std::mutex> lock(mutex_);
        basic_events_[name] = probability;
    }

    /// @brief Compute the probability of the top event (the gate that feeds no other gate).
    double computeTopEventProbability() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (gates_.empty()) return 0.0;

        std::set<std::string> referenced;
        for (const auto& [_, gate] : gates_) {
            for (const auto& in : gate.inputs) referenced.insert(in);
        }
        std::string top;
        for (const auto& [name, _] : gates_) {
            if (referenced.find(name) == referenced.end()) {
                top = name;
                break;
            }
        }
        if (top.empty()) top = gates_.begin()->first;

        std::set<std::string> visiting;
        return evaluate(top, visiting);
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        gates_.clear();
        basic_events_.clear();
    }

private:
    struct Gate {
        std::string type;
        std::vector<std::string> inputs;
    };

    FaultTreeAnalyzer() = default;
    ~FaultTreeAnalyzer() = default;
    FaultTreeAnalyzer(const FaultTreeAnalyzer&) = delete;
    FaultTreeAnalyzer& operator=(const FaultTreeAnalyzer&) = delete;

    double evaluate(const std::string& node, std::set<std::string>& visiting) const {
        auto be = basic_events_.find(node);
        if (be != basic_events_.end()) return be->second;

        auto git = gates_.find(node);
        if (git == gates_.end()) return 0.0;
        if (!visiting.insert(node).second) return 0.0; // cycle guard

        const Gate& gate = git->second;
        double result;
        if (gate.type == "AND") {
            result = 1.0;
            for (const auto& in : gate.inputs) result *= evaluate(in, visiting);
        } else { // OR (default)
            double none = 1.0;
            for (const auto& in : gate.inputs) none *= (1.0 - evaluate(in, visiting));
            result = 1.0 - none;
        }
        visiting.erase(node);
        return result;
    }

    mutable std::mutex mutex_;
    bool enabled_ = false;
    std::string config_;
    std::map<std::string, Gate> gates_;
    std::map<std::string, double> basic_events_;
};

} // namespace nexus::utility::safety
