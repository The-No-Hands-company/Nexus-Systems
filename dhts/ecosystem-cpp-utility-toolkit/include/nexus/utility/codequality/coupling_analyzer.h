#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>

namespace nexus::utility::codequality {

/**
 * @brief Analyze coupling (dependencies) between modules.
 */
class CouplingAnalyzer {
public:
    struct CouplingMetrics {
        std::string module;
        std::size_t afferent = 0;   // Ca: modules depending on this one
        std::size_t efferent = 0;   // Ce: modules this one depends on
        double instability() const {
            std::size_t sum = afferent + efferent;
            return sum == 0 ? 0.0 : static_cast<double>(efferent) / sum;
        }
    };

    static CouplingAnalyzer& instance() {
        static CouplingAnalyzer inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void addDependency(const std::string& from, const std::string& to) {
        if (from == to) return;
        std::lock_guard<std::mutex> lk(mutex_);
        deps_[from].insert(to);
        modules_.insert(from);
        modules_.insert(to);
    }

    std::size_t efferentCoupling(const std::string& module) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = deps_.find(module);
        return it == deps_.end() ? 0 : it->second.size();
    }

    std::size_t afferentCoupling(const std::string& module) const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::size_t count = 0;
        for (const auto& [from, tos] : deps_)
            if (tos.count(module)) ++count;
        return count;
    }

    CouplingMetrics metrics(const std::string& module) const {
        CouplingMetrics m;
        m.module = module;
        m.efferent = efferentCoupling(module);
        m.afferent = afferentCoupling(module);
        return m;
    }

    std::vector<CouplingMetrics> allMetrics() const {
        std::lock_guard<std::mutex> lk(mutex_);
        std::vector<CouplingMetrics> out;
        for (const auto& mod : modules_) {
            CouplingMetrics m;
            m.module = mod;
            auto it = deps_.find(mod);
            m.efferent = it == deps_.end() ? 0 : it->second.size();
            for (const auto& [from, tos] : deps_)
                if (tos.count(mod)) ++m.afferent;
            out.push_back(m);
        }
        return out;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        deps_.clear();
        modules_.clear();
    }

private:
    CouplingAnalyzer() = default;
    ~CouplingAnalyzer() = default;
    CouplingAnalyzer(const CouplingAnalyzer&) = delete;
    CouplingAnalyzer& operator=(const CouplingAnalyzer&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::unordered_map<std::string, std::unordered_set<std::string>> deps_;
    std::unordered_set<std::string> modules_;
};

} // namespace nexus::utility::codequality
