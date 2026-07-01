#pragma once

/**
 * @file Chorus.h
 * @brief Professional chorus effect for lush, wide sound
 * @version 1.0.0
 * 
 * Features:
 * - Multiple chorus voices with independent LFO phases
 * - Variable chorus modes (Classic, Ensemble, Wide, Shimmer)
 * - Advanced modulation with multiple LFO waveforms
 * - High-quality interpolation for smooth modulation
 * - Stereo width and voice spreading controls
 * - Vintage-style filtering and saturation
 */

#include "../EffectBase.h"
#include <memory>

namespace dawg {
namespace dsp {
namespace modulation {

/**
 * @brief Professional chorus effect for lush, wide sound
 * 
 * Advanced features:
 * - Multiple independent chorus voices (1-8)
 * - Various chorus modes (Classic, Ensemble, Wide, Shimmer)
 * - Multiple LFO waveforms (Sine, Triangle, Saw, Random)
 * - High-quality cubic interpolation for smooth modulation
 * - Voice spreading across stereo field
 * - Vintage-style low-pass filtering
 * - Optional subtle saturation for warmth
 * - Tempo synchronization support
 */
class Chorus : public Effect {
public:
    /**
     * @brief Chorus processing modes
     */
    enum class ChorusMode {
        Classic,        ///< Traditional chorus sound
        Ensemble,       ///< Multiple voice ensemble effect
        Wide,           ///< Stereo widening chorus
        Shimmer,        ///< Shimmer/doubling effect
        Vintage         ///< Vintage analog chorus emulation
    };
    
    /**
     * @brief LFO waveform types
     */
    enum class LFOWaveform {
        Sine,           ///< Smooth sine wave
        Triangle,       ///< Linear triangle wave
        Sawtooth,       ///< Ascending sawtooth
        Square,         ///< Square wave
        Random          ///< Sample-and-hold random
    };
    
    // ========================================================================
    // CONSTRUCTION/DESTRUCTION
    // ========================================================================
    
    Chorus();
    virtual ~Chorus();
    
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
    
    std::string getName() const override { return "Chorus"; }
    uint32_t getLatency() const override;
    
    // ========================================================================
    // CHORUS-SPECIFIC CONTROL
    // ========================================================================
    
    /**
     * @brief Set modulation depth
     * @param depth Modulation depth (0.0 to 1.0)
     */
    void setDepth(double depth);
    
    /**
     * @brief Set modulation rate
     * @param rate LFO rate in Hz (0.1 to 10.0Hz)
     */
    void setRate(double rate);
    
    /**
     * @brief Set base delay time
     * @param delay Base delay in milliseconds (5-50ms)
     */
    void setDelay(double delay);
    
    /**
     * @brief Set feedback amount
     * @param feedback Feedback level (0.0 to 0.7)
     */
    void setFeedback(double feedback);
    
    /**
     * @brief Set wet/dry mix
     * @param mix Mix level (0.0 = dry only, 1.0 = wet only)
     */
    void setMix(double mix);
    
    /**
     * @brief Set number of chorus voices
     * @param voices Number of voices (1-8)
     */
    void setVoices(uint32_t voices);
    
    /**
     * @brief Set chorus processing mode
     * @param mode Chorus mode type
     */
    void setChorusMode(ChorusMode mode);
    
    /**
     * @brief Set LFO waveform
     * @param waveform LFO waveform type
     */
    void setLFOWaveform(LFOWaveform waveform);
    
    /**
     * @brief Set stereo width
     * @param width Stereo width (0.0 to 2.0)
     */
    void setStereoWidth(double width);
    
    /**
     * @brief Set voice spreading
     * @param spread Voice spread across stereo field (0.0 to 1.0)
     */
    void setVoiceSpread(double spread);
    
    /**
     * @brief Enable/disable tempo synchronization
     * @param sync True to enable tempo sync
     */
    void setTempoSync(bool sync);
    
    /**
     * @brief Set tempo for synchronization
     * @param bpm Tempo in beats per minute
     */
    void setTempo(double bpm);
    
    /**
     * @brief Set vintage filtering amount
     * @param amount Vintage filter amount (0.0 to 1.0)
     */
    void setVintageFilter(double amount);
    
    /**
     * @brief Set saturation amount
     * @param amount Saturation amount (0.0 to 1.0)
     */
    void setSaturation(double amount);
    
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
    // ADVANCED VOICE CONTROL
    // ========================================================================
    
    /**
     * @brief Set individual voice delay offset
     * @param voiceIndex Voice index (0-7)
     * @param offset Delay offset multiplier (0.5 to 2.0)
     */
    void setVoiceDelayOffset(uint32_t voiceIndex, double offset);
    
    /**
     * @brief Set individual voice rate offset
     * @param voiceIndex Voice index (0-7)
     * @param offset Rate offset multiplier (0.5 to 2.0)
     */
    void setVoiceRateOffset(uint32_t voiceIndex, double offset);
    
    /**
     * @brief Set individual voice level
     * @param voiceIndex Voice index (0-7)
     * @param level Voice level (0.0 to 1.0)
     */
    void setVoiceLevel(uint32_t voiceIndex, double level);
    
    /**
     * @brief Set individual voice pan
     * @param voiceIndex Voice index (0-7)
     * @param pan Voice pan (-1.0 = left, +1.0 = right)
     */
    void setVoicePan(uint32_t voiceIndex, double pan);
    
    // ========================================================================
    // METERING AND ANALYSIS
    // ========================================================================
    
    /**
     * @brief Get current LFO phase
     * @return LFO phase (0.0 to 1.0)
     */
    double getLFOPhase() const;
    
    /**
     * @brief Get current modulation value
     * @return Current modulation value (-1.0 to +1.0)
     */
    double getModulationValue() const;
    
    /**
     * @brief Check if chorus is currently active
     * @return True if chorus effect is present
     */
    bool isActive() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace modulation
} // namespace dsp
} // namespace dawg
