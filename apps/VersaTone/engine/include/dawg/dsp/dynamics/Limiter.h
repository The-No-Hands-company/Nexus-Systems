#pragma once

/**
 * @file Limiter.h
 * @brief Professional brick-wall limiter for mastering
 * @version 1.0.0
 * 
 * Features:
 * - True peak limiting with oversampling
 * - ISP (Inter-Sample Peak) detection
 * - Multiple release algorithms (Auto/Manual/Adaptive)
 * - Look-ahead processing for transparent limiting
 * - Advanced metering (gain reduction, true peak levels)
 * - Zero-latency and low-latency modes
 */

#include "../EffectBase.h"
#include <memory>

namespace dawg {
namespace dsp {
namespace dynamics {

/**
 * @brief Professional brick-wall limiter for mastering
 * 
 * Advanced features:
 * - True peak detection with oversampling (2x, 4x, 8x)
 * - ISP (Inter-Sample Peak) detection for digital peak limiting
 * - Program-dependent release timing
 * - Look-ahead buffer for transparent limiting
 * - Advanced gain reduction metering
 * - Multiple release modes for different material
 */
class Limiter : public Effect {
public:
    /**
     * @brief Release timing modes
     */
    enum class ReleaseMode {
        Auto,       ///< Program-dependent release (analyzes incoming audio)
        Manual,     ///< Fixed release time set by user
        Adaptive    ///< Dynamic release adaptation based on signal characteristics
    };
    
    // ========================================================================
    // CONSTRUCTION/DESTRUCTION
    // ========================================================================
    
    Limiter();
    virtual ~Limiter();
    
    // ========================================================================
    // EFFECT INTERFACE
    // ========================================================================
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    std::string getName() const override { return "Limiter"; }
    uint32_t getLatency() const override;
    
    // ========================================================================
    // LIMITER-SPECIFIC CONTROL
    // ========================================================================
    
    /**
     * @brief Set output ceiling level
     * @param ceiling Maximum output level in dB (typically -0.1 to -3.0 dB)
     */
    void setCeiling(double ceiling);
    
    /**
     * @brief Set release time for manual mode
     * @param release Release time in milliseconds (1-1000ms)
     */
    void setRelease(double release);
    
    /**
     * @brief Set release timing mode
     * @param mode Release algorithm type
     */
    void setReleaseMode(ReleaseMode mode);
    
    /**
     * @brief Set look-ahead time
     * @param time Look-ahead time in milliseconds (0-10ms)
     */
    void setLookAhead(double time);
    
    /**
     * @brief Set oversampling factor
     * @param factor Oversampling factor (1x, 2x, 4x, 8x)
     */
    void setOversampling(uint32_t factor);
    
    /**
     * @brief Enable/disable ISP detection
     * @param enabled True to enable inter-sample peak detection
     */
    void setISPDetection(bool enabled);
    
    /**
     * @brief Set input gain
     * @param gain Input gain in dB (-20 to +20 dB)
     */
    void setInputGain(double gain);
    
    /**
     * @brief Set output gain
     * @param gain Output gain in dB (-20 to +20 dB)
     */
    void setOutputGain(double gain);
    
    // ========================================================================
    // METERING AND ANALYSIS
    // ========================================================================
    
    /**
     * @brief Get current gain reduction
     * @return Gain reduction in dB (negative values)
     */
    double getGainReduction() const;
    
    /**
     * @brief Get true peak level
     * @return True peak level in dBTP
     */
    double getTruePeakLevel() const;
    
    /**
     * @brief Get RMS level
     * @return RMS level in dB
     */
    double getRMSLevel() const;
    
    /**
     * @brief Get current release time being used
     * @return Effective release time in ms (useful for auto/adaptive modes)
     */
    double getEffectiveRelease() const;
    
    /**
     * @brief Check if limiter is currently active
     * @return True if gain reduction is being applied
     */
    bool isLimiting() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace dynamics
} // namespace dsp
} // namespace dawg
