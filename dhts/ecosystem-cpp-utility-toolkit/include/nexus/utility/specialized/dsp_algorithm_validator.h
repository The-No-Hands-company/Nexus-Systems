#pragma once

#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <mutex>

namespace nexus::utility::specialized {

/**
 * @brief Validate digital signal processing (DSP) algorithm outputs.
 */
class DspAlgorithmValidator {
public:
    struct ValidationResult {
        bool valid = true;
        double snrDb = 0.0;          // signal-to-noise ratio
        double maxError = 0.0;
        double rmsError = 0.0;
        std::string note;
    };

    static DspAlgorithmValidator& instance() {
        static DspAlgorithmValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    /// Compare a processed signal against an expected reference.
    ValidationResult validate(const std::vector<double>& output,
                              const std::vector<double>& expected,
                              double tolerance = 1e-3) {
        ValidationResult r;
        if (output.size() != expected.size()) {
            r.valid = false;
            r.note = "size mismatch";
            recordResult(r);
            return r;
        }
        double sumSqErr = 0.0, sumSqSig = 0.0;
        for (std::size_t i = 0; i < output.size(); ++i) {
            double err = std::fabs(output[i] - expected[i]);
            r.maxError = std::max(r.maxError, err);
            sumSqErr += err * err;
            sumSqSig += expected[i] * expected[i];
        }
        std::size_t n = output.empty() ? 1 : output.size();
        r.rmsError = std::sqrt(sumSqErr / n);
        r.snrDb = sumSqErr > 0.0 ? 10.0 * std::log10(sumSqSig / sumSqErr) : 300.0;
        r.valid = r.maxError <= tolerance;
        recordResult(r);
        return r;
    }

    /// Check that no output sample exceeds full-scale (clipping detection).
    bool checkClipping(const std::vector<double>& output, double fullScale = 1.0) {
        for (double v : output)
            if (std::fabs(v) > fullScale) { ++clippingEvents_; return true; }
        return false;
    }

    std::size_t validations() const { return validations_.load(); }
    std::size_t failures() const { return failures_.load(); }
    std::size_t clippingEvents() const { return clippingEvents_.load(); }

    void reset() {
        validations_ = 0;
        failures_ = 0;
        clippingEvents_ = 0;
    }

private:
    DspAlgorithmValidator() = default;
    ~DspAlgorithmValidator() = default;
    DspAlgorithmValidator(const DspAlgorithmValidator&) = delete;
    DspAlgorithmValidator& operator=(const DspAlgorithmValidator&) = delete;

    void recordResult(const ValidationResult& r) {
        ++validations_;
        if (!r.valid) ++failures_;
    }

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    std::atomic<std::size_t> validations_{0};
    std::atomic<std::size_t> failures_{0};
    std::atomic<std::size_t> clippingEvents_{0};
};

} // namespace nexus::utility::specialized
