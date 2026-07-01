#pragma once

/**
 * @file EffectBase.h
 * @brief Base class for all audio effects
 * @version 1.0.0
 * 
 * Provides common interface and functionality for all audio effects
 * in the DAWG audio engine.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>

// Forward declarations
namespace dawg { namespace core { class AudioBuffer; } }

namespace dawg {
namespace dsp {

/**
 * @brief Base class for all audio effects
 * 
 * Defines the common interface that all effects must implement.
 * Provides standardized parameter control, state management,
 * and processing workflow for all effects.
 */
class Effect {
public:
    virtual ~Effect() = default;
    
    // ========================================================================
    // CORE PROCESSING
    // ========================================================================
    
    /**
     * @brief Process audio buffer in-place
     * @param buffer Audio buffer to process
     */
    virtual void process(core::AudioBuffer& buffer) = 0;
    
    /**
     * @brief Reset effect state (clear delays, etc.)
     */
    virtual void reset() = 0;
    
    // ========================================================================
    // PARAMETER CONTROL
    // ========================================================================
    
    /**
     * @brief Set effect parameter by name
     * @param name Parameter name
     * @param value Parameter value (normalized 0.0-1.0 or specific units)
     */
    virtual void setParameter(const std::string& name, double value) = 0;
    
    /**
     * @brief Get effect parameter by name
     * @param name Parameter name
     * @return Parameter value
     */
    virtual double getParameter(const std::string& name) const = 0;
    
    /**
     * @brief Get list of all parameter names
     * @return Vector of parameter name strings
     */
    virtual std::vector<std::string> getParameterNames() const = 0;
    
    // ========================================================================
    // STATE MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Set sample rate for effect processing
     * @param sampleRate Sample rate in Hz
     */
    virtual void setSampleRate(uint32_t sampleRate) = 0;
    
    /**
     * @brief Set buffer size for effect processing
     * @param bufferSize Buffer size in samples
     */
    virtual void setBufferSize(uint32_t bufferSize) = 0;
    
    // ========================================================================
    // BYPASS CONTROL
    // ========================================================================
    
    /**
     * @brief Enable or disable effect bypass
     * @param bypassed True to bypass effect
     */
    void setBypassed(bool bypassed) { this->bypassed = bypassed; }
    
    /**
     * @brief Check if effect is bypassed
     * @return True if effect is bypassed
     */
    bool isBypassed() const { return bypassed; }
    
    // ========================================================================
    // UTILITY METHODS
    // ========================================================================
    
    /**
     * @brief Get effect name/type
     * @return Effect name string
     */
    virtual std::string getName() const = 0;
    
    /**
     * @brief Get effect version
     * @return Version string
     */
    virtual std::string getVersion() const { return "1.0.0"; }
    
    /**
     * @brief Check if effect supports stereo processing
     * @return True if stereo capable
     */
    virtual bool isStereoCapable() const { return true; }
    
    /**
     * @brief Get processing latency in samples
     * @return Latency in samples
     */
    virtual uint32_t getLatency() const { return 0; }

protected:
    // Common state variables
    bool bypassed = false;
    uint32_t sampleRate = 48000;
    uint32_t bufferSize = 512;
    
    // ========================================================================
    // UTILITY FUNCTIONS FOR DERIVED CLASSES
    // ========================================================================
    
    /**
     * @brief Convert dB to linear gain
     * @param dB Value in decibels
     * @return Linear gain value
     */
    static double dbToLinear(double dB) {
        return std::pow(10.0, dB / 20.0);
    }
    
    /**
     * @brief Convert linear gain to dB
     * @param linear Linear gain value
     * @return Value in decibels
     */
    static double linearToDb(double linear) {
        return 20.0 * std::log10(std::max(linear, 1e-10));
    }
    
    /**
     * @brief Clamp value between min and max
     * @param value Value to clamp
     * @param min Minimum value
     * @param max Maximum value
     * @return Clamped value
     */
    static double clamp(double value, double min, double max) {
        return std::min(std::max(value, min), max);
    }
    
    /**
     * @brief Linear interpolation
     * @param a Start value
     * @param b End value  
     * @param t Interpolation factor (0.0-1.0)
     * @return Interpolated value
     */
    static double lerp(double a, double b, double t) {
        return a + t * (b - a);
    }
};

} // namespace dsp
} // namespace dawg
