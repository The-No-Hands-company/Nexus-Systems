#pragma once

/**
 * @file ParametricEQ.h
 * @brief Professional 8-band parametric equalizer
 * @version 1.0.0
 * 
 * High-precision parametric EQ for mixing and mastering:
 * - 8 fully parametric bands
 * - High/low-pass filters with multiple slopes
 * - High/low shelf filters
 * - Bell filters with adjustable Q
 * - Real-time frequency response analysis
 * - Surgical precision for professional audio work
 */

#include "../EffectBase.h"
#include <memory>

namespace dawg {
namespace dsp {
namespace eq {

/**
 * @brief Professional 8-band parametric equalizer
 * 
 * Features:
 * - 8 fully parametric bands
 * - High/low-pass filters with 12/24/48dB slopes
 * - High/low shelf filters
 * - Bell filters with adjustable Q
 * - Real-time spectrum analysis
 * - Surgical precision for mixing and mastering
 */
class ParametricEQ : public Effect {
public:
    /**
     * @brief Filter type for each band
     */
    enum class FilterType {
        HighPass,   ///< High-pass filter (removes low frequencies)
        LowPass,    ///< Low-pass filter (removes high frequencies)
        HighShelf,  ///< High-frequency shelf
        LowShelf,   ///< Low-frequency shelf
        Bell,       ///< Bell-shaped boost/cut
        Notch       ///< Narrow notch filter
    };
    
    /**
     * @brief Filter slope options
     */
    enum class FilterSlope {
        Slope6dB,   ///< 6 dB/octave (1-pole)
        Slope12dB,  ///< 12 dB/octave (2-pole)
        Slope18dB,  ///< 18 dB/octave (3-pole)
        Slope24dB,  ///< 24 dB/octave (4-pole)
        Slope48dB   ///< 48 dB/octave (8-pole)
    };
    
    /**
     * @brief EQ band configuration
     */
    struct Band {
        FilterType type = FilterType::Bell;     ///< Filter type
        double frequency = 1000.0;              ///< Center frequency in Hz
        double gain = 0.0;                      ///< Gain in dB
        double q = 0.707;                       ///< Quality factor
        FilterSlope slope = FilterSlope::Slope12dB; ///< Filter slope
        bool enabled = false;                   ///< Band enable/disable
        
        Band() = default;
        Band(FilterType t, double f, double g = 0.0, double quality = 0.707) 
            : type(t), frequency(f), gain(g), q(quality) {}
    };
    
    // ========================================================================
    // CONSTRUCTION/DESTRUCTION
    // ========================================================================
    
    ParametricEQ();
    virtual ~ParametricEQ();
    
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
    
    std::string getName() const override { return "ParametricEQ"; }
    
    // ========================================================================
    // EQ-SPECIFIC CONTROL
    // ========================================================================
    
    /**
     * @brief Set configuration for a specific band
     * @param bandIndex Band index (0-7)
     * @param band Band configuration
     */
    void setBand(uint32_t bandIndex, const Band& band);
    
    /**
     * @brief Get configuration for a specific band
     * @param bandIndex Band index (0-7)
     * @return Band configuration
     */
    Band getBand(uint32_t bandIndex) const;
    
    /**
     * @brief Get number of EQ bands
     * @return Number of bands (always 8)
     */
    uint32_t getNumBands() const { return 8; }
    
    /**
     * @brief Enable or disable a specific band
     * @param bandIndex Band index (0-7)
     * @param enabled True to enable band
     */
    void setBandEnabled(uint32_t bandIndex, bool enabled);
    
    /**
     * @brief Check if a band is enabled
     * @param bandIndex Band index (0-7)
     * @return True if band is enabled
     */
    bool isBandEnabled(uint32_t bandIndex) const;
    
    /**
     * @brief Set global EQ gain
     * @param gain Global gain in dB
     */
    void setGlobalGain(double gain);
    
    /**
     * @brief Get global EQ gain
     * @return Global gain in dB
     */
    double getGlobalGain() const;
    
    // ========================================================================
    // ANALYSIS AND VISUALIZATION
    // ========================================================================
    
    /**
     * @brief Get frequency response at specified frequencies
     * @param frequencies Vector of frequencies to analyze
     * @return Vector of magnitude responses in dB
     */
    std::vector<double> getFrequencyResponse(const std::vector<double>& frequencies) const;
    
    /**
     * @brief Get phase response at specified frequencies
     * @param frequencies Vector of frequencies to analyze
     * @return Vector of phase responses in degrees
     */
    std::vector<double> getPhaseResponse(const std::vector<double>& frequencies) const;
    
    /**
     * @brief Get group delay at specified frequencies
     * @param frequencies Vector of frequencies to analyze
     * @return Vector of group delays in samples
     */
    std::vector<double> getGroupDelay(const std::vector<double>& frequencies) const;
    
    // ========================================================================
    // PRESETS AND UTILITY
    // ========================================================================
    
    /**
     * @brief Reset all bands to flat response
     */
    void resetToFlat();
    
    /**
     * @brief Load common EQ presets
     * @param presetName Preset name ("vocal", "bass", "master", etc.)
     */
    void loadPreset(const std::string& presetName);
    
    /**
     * @brief Set high-pass filter for rumble removal
     * @param frequency High-pass frequency in Hz (0 to disable)
     * @param slope Filter slope
     */
    void setHighPassFilter(double frequency, FilterSlope slope = FilterSlope::Slope12dB);
    
    /**
     * @brief Set low-pass filter for anti-aliasing
     * @param frequency Low-pass frequency in Hz (0 to disable)  
     * @param slope Filter slope
     */
    void setLowPassFilter(double frequency, FilterSlope slope = FilterSlope::Slope12dB);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace eq
} // namespace dsp
} // namespace dawg
