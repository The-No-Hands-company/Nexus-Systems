#pragma once

/**
 * @file Delay.h
 * @brief Professional digital delay with multiple modes and creative features
 * @version 1.0.0
 * 
 * Features:
 * - Multiple delay modes (Mono, Stereo, Ping-Pong, Multi-tap)
 * - Tempo synchronization with musical note values
 * - High-quality filtering (low-cut, high-cut)
 * - Modulation for chorus-like effects
 * - Multiple feedback routing options
 * - Advanced ducking and gate features
 */

#include "../EffectBase.h"
#include <memory>

namespace dawg {
namespace dsp {
namespace time {

/**
 * @brief Professional digital delay with creative features
 * 
 * Advanced features:
 * - Multiple delay modes (Mono, Stereo, Ping-Pong, Multi-tap)
 * - Tempo sync with musical note values (1/4, 1/8, 1/16, etc.)
 * - High-quality filtering for feedback shaping
 * - LFO modulation for chorus/vibrato effects
 * - Ducking (delay level reduces when input is present)
 * - Freeze mode for infinite sustain
 * - Multiple tap configuration for rhythmic delays
 */
class Delay : public Effect {
public:
    /**
     * @brief Delay processing modes
     */
    enum class DelayMode {
        Mono,           ///< Simple mono delay
        Stereo,         ///< Independent L/R delays
        PingPong,       ///< Bouncing between channels
        MultiTap        ///< Multiple delay taps for rhythmic effects
    };
    
    /**
     * @brief Tempo sync note values
     */
    enum class NoteValue {
        Eighth = 0,     ///< 1/8 note
        Quarter,        ///< 1/4 note
        Half,           ///< 1/2 note
        Whole,          ///< Whole note
        Dotted8th,      ///< Dotted 1/8 note
        Dotted4th,      ///< Dotted 1/4 note
        Triplet8th,     ///< 1/8 note triplet
        Triplet4th,     ///< 1/4 note triplet
        Sixteenth       ///< 1/16 note
    };
    
    // ========================================================================
    // CONSTRUCTION/DESTRUCTION
    // ========================================================================
    
    Delay();
    virtual ~Delay();
    
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
    
    std::string getName() const override { return "Delay"; }
    uint32_t getLatency() const override;
    
    // ========================================================================
    // DELAY-SPECIFIC CONTROL
    // ========================================================================
    
    /**
     * @brief Set delay time in milliseconds
     * @param time Delay time in ms (1-2000ms)
     */
    void setDelayTime(double time);
    
    /**
     * @brief Set feedback amount
     * @param feedback Feedback level (0.0 to 0.98)
     */
    void setFeedback(double feedback);
    
    /**
     * @brief Set wet/dry mix
     * @param mix Mix level (0.0 = dry only, 1.0 = wet only)
     */
    void setMix(double mix);
    
    /**
     * @brief Set delay processing mode
     * @param mode Delay mode type
     */
    void setDelayMode(DelayMode mode);
    
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
     * @brief Set note value for tempo sync
     * @param noteValue Musical note value
     */
    void setNoteValue(NoteValue noteValue);
    
    /**
     * @brief Set low-cut filter frequency
     * @param frequency Cutoff frequency in Hz (20-2000Hz)
     */
    void setLowCut(double frequency);
    
    /**
     * @brief Set high-cut filter frequency
     * @param frequency Cutoff frequency in Hz (1000-20000Hz)
     */
    void setHighCut(double frequency);
    
    /**
     * @brief Set modulation depth
     * @param depth Modulation depth (0.0 to 1.0)
     */
    void setModulationDepth(double depth);
    
    /**
     * @brief Set modulation rate
     * @param rate Modulation rate in Hz (0.1 to 10.0Hz)
     */
    void setModulationRate(double rate);
    
    /**
     * @brief Set ducking amount
     * @param amount Ducking amount (0.0 to 1.0)
     */
    void setDucking(double amount);
    
    /**
     * @brief Set stereo width for ping-pong mode
     * @param width Stereo width (0.0 to 1.0)
     */
    void setStereoWidth(double width);
    
    /**
     * @brief Enable/disable freeze mode
     * @param freeze True for infinite sustain
     */
    void setFreeze(bool freeze);
    
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
    // MULTI-TAP CONTROL
    // ========================================================================
    
    /**
     * @brief Set number of taps for multi-tap mode
     * @param taps Number of taps (1-8)
     */
    void setTapCount(uint32_t taps);
    
    /**
     * @brief Set individual tap time
     * @param tapIndex Tap index (0-7)
     * @param time Tap time multiplier (0.1 to 2.0)
     */
    void setTapTime(uint32_t tapIndex, double time);
    
    /**
     * @brief Set individual tap level
     * @param tapIndex Tap index (0-7)
     * @param level Tap level (0.0 to 1.0)
     */
    void setTapLevel(uint32_t tapIndex, double level);
    
    /**
     * @brief Set individual tap pan
     * @param tapIndex Tap index (0-7)
     * @param pan Tap pan (-1.0 = left, +1.0 = right)
     */
    void setTapPan(uint32_t tapIndex, double pan);
    
    // ========================================================================
    // METERING AND ANALYSIS
    // ========================================================================
    
    /**
     * @brief Get current delay time being used
     * @return Effective delay time in ms
     */
    double getEffectiveDelayTime() const;
    
    /**
     * @brief Get delay buffer fill level
     * @return Buffer fill percentage (0.0 to 1.0)
     */
    double getBufferFill() const;
    
    /**
     * @brief Check if delay is currently active
     * @return True if delay signal is present
     */
    bool isActive() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace time
} // namespace dsp
} // namespace dawg
