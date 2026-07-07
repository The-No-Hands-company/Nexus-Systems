#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace nexus::utility::quantum {

/**
 * @brief Track quantum error syndromes and correction statistics.
 */
class QuantumErrorCorrector {
public:
    enum class ErrorType { BitFlip, PhaseFlip, BitPhaseFlip, None };

    struct Syndrome {
        std::vector<int> measurements;   // stabilizer measurement results (0/1)
        ErrorType detectedError = ErrorType::None;
        std::size_t affectedQubit = 0;
        bool corrected = false;
    };

    static QuantumErrorCorrector& instance() {
        static QuantumErrorCorrector inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// A nonzero syndrome indicates an error was detected.
    static bool hasError(const std::vector<int>& measurements) {
        for (int m : measurements) if (m != 0) return true;
        return false;
    }

    /// Record a syndrome measurement and its decoded correction.
    Syndrome recordSyndrome(const std::vector<int>& measurements, ErrorType detected,
                            std::size_t affectedQubit, bool corrected) {
        Syndrome s{measurements, detected, affectedQubit, corrected};
        std::lock_guard<std::mutex> lk(mutex_);
        syndromes_.push_back(s);
        if (detected != ErrorType::None) {
            ++detected_;
            ++errorCounts_[detected];
            if (corrected) ++corrected_;
        }
        return s;
    }

    std::size_t detectedErrors() const { return detected_.load(); }
    std::size_t correctedErrors() const { return corrected_.load(); }

    /// Logical error rate: uncorrected errors over total detected.
    double logicalErrorRate() const {
        std::size_t d = detected_.load(), c = corrected_.load();
        return d == 0 ? 0.0 : static_cast<double>(d - c) / d;
    }

    std::size_t countByType(ErrorType type) const {
        std::lock_guard<std::mutex> lk(mutex_);
        auto it = errorCounts_.find(type);
        return it == errorCounts_.end() ? 0 : it->second;
    }

    std::vector<Syndrome> history() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return syndromes_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        syndromes_.clear();
        errorCounts_.clear();
        detected_ = 0;
        corrected_ = 0;
    }

private:
    QuantumErrorCorrector() = default;
    ~QuantumErrorCorrector() = default;
    QuantumErrorCorrector(const QuantumErrorCorrector&) = delete;
    QuantumErrorCorrector& operator=(const QuantumErrorCorrector&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<Syndrome> syndromes_;
    std::unordered_map<ErrorType, std::size_t> errorCounts_;
    std::atomic<std::size_t> detected_{0};
    std::atomic<std::size_t> corrected_{0};
};

} // namespace nexus::utility::quantum
