#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstddef>

namespace nexus::utility::audio {

/**
 * @brief Validate audio sample rates against an expected rate and known set.
 */
class SampleRateValidator {
public:
    static SampleRateValidator& instance() {
        static SampleRateValidator inst;
        return inst;
    }

    void initialize(const std::string& config = "") {
        std::lock_guard<std::mutex> lk(mutex_);
        config_ = config;
        enabled_ = true;
    }
    void shutdown() { enabled_ = false; }
    void enable() { enabled_ = true; }
    void disable() { enabled_ = false; }
    bool isEnabled() const { return enabled_.load(); }

    void setExpectedRate(int rate) {
        std::lock_guard<std::mutex> lk(mutex_);
        expectedRate_ = rate;
    }

    /// True if @p actual_rate matches the expected rate within tolerance.
    bool validateRate(int actual_rate) {
        std::lock_guard<std::mutex> lk(mutex_);
        ++validationCount_;
        bool ok = std::abs(actual_rate - expectedRate_) <= toleranceHz_;
        if (!ok) ++mismatchCount_;
        return ok;
    }

    /// Standard PCM sample rates supported by the validator.
    std::vector<int> getSupportedRates() const {
        return {8000, 11025, 16000, 22050, 44100, 48000, 96000};
    }

    std::size_t getValidationCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return validationCount_;
    }
    std::size_t getMismatchCount() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return mismatchCount_;
    }

    std::string getStatus() const {
        std::lock_guard<std::mutex> lk(mutex_);
        return "SampleRateValidator{expected=" + std::to_string(expectedRate_) +
               ", validations=" + std::to_string(validationCount_) +
               ", mismatches=" + std::to_string(mismatchCount_) + "}";
    }

    void reset() {
        std::lock_guard<std::mutex> lk(mutex_);
        validationCount_ = 0;
        mismatchCount_ = 0;
    }

private:
    SampleRateValidator() = default;
    ~SampleRateValidator() = default;
    SampleRateValidator(const SampleRateValidator&) = delete;
    SampleRateValidator& operator=(const SampleRateValidator&) = delete;

    mutable std::mutex mutex_;
    std::atomic<bool> enabled_{false};
    std::string config_;
    int expectedRate_ = 44100;
    int toleranceHz_ = 1;
    std::size_t validationCount_ = 0;
    std::size_t mismatchCount_ = 0;
};

} // namespace nexus::utility::audio
