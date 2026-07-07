#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace nexus::utility::scientific {

/**
 * @brief Track loss of numerical precision across a computation.
 */
class NumericalPrecisionTracker {
public:
    struct PrecisionSample {
        std::string label;
        double computed = 0.0;
        double reference = 0.0;
        double absoluteError = 0.0;
        double relativeError = 0.0;
        int significantDigits = 0;
    };

    static NumericalPrecisionTracker& instance() {
        static NumericalPrecisionTracker inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    PrecisionSample record(const std::string& label, double computed, double reference) {
        PrecisionSample s;
        s.label = label;
        s.computed = computed;
        s.reference = reference;
        s.absoluteError = std::fabs(computed - reference);
        s.relativeError = reference != 0.0 ? s.absoluteError / std::fabs(reference) : s.absoluteError;
        s.significantDigits = s.relativeError > 0.0
            ? static_cast<int>(-std::log10(s.relativeError))
            : 15;
        if (s.significantDigits < 0) s.significantDigits = 0;
        std::lock_guard<std::mutex> lk(mutex_);
        samples_.push_back(s);
        return s;
    }

    double worstRelativeError() const {
        std::lock_guard<std::mutex> lk(mutex_);
        double worst = 0.0;
        for (const auto& s : samples_) worst = std::max(worst, s.relativeError);
        return worst;
    }

    int minSignificantDigits() const {
        std::lock_guard<std::mutex> lk(mutex_);
        if (samples_.empty()) return 0;
        int m = samples_.front().significantDigits;
        for (const auto& s : samples_) m = std::min(m, s.significantDigits);
        return m;
    }

    std::vector<PrecisionSample> samples() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return samples_;
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        samples_.clear();
    }

private:
    NumericalPrecisionTracker() = default;
    ~NumericalPrecisionTracker() = default;
    NumericalPrecisionTracker(const NumericalPrecisionTracker&) = delete;
    NumericalPrecisionTracker& operator=(const NumericalPrecisionTracker&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::vector<PrecisionSample> samples_;
};

} // namespace nexus::utility::scientific
