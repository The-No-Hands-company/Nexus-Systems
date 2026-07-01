#pragma once

/**
 * @file AlgorithmicReverb.h
 * @brief Studio-quality algorithmic reverb
 * @version 1.0.0
 * 
 * Professional reverb algorithms for spatial audio processing:
 * - Multiple reverb algorithms (Hall, Room, Plate, Spring, Chamber)
 * - Early reflections modeling with realistic geometry
 * - Late reverb tail with natural decay characteristics
 * - High-frequency damping for realistic acoustics
 * - Modulation for lush, animated reverb tails
 * - Freeze mode for infinite sustain effects
 */

#include "../EffectBase.h"
#include <memory>

namespace dawg {
namespace dsp {
namespace time {

/**
 * @brief Studio-quality algorithmic reverb
 * 
 * Features:
 * - Multiple reverb algorithms (Hall, Room, Plate, Spring)
 * - Early reflections modeling
 * - Late reverb tail with natural decay
 * - High-frequency damping
 * - Modulation for lush, animated sound
 * - Freeze mode for infinite sustain
 */
class AlgorithmicReverb : public Effect {
public:
    /**
     * @brief Reverb algorithm types
     */
    enum class ReverbType {
        Hall,       ///< Large concert hall (long, spacious)
        Room,       ///< Medium room/studio (warm, controlled)
        Plate,      ///< Vintage plate reverb (bright, metallic)
        Spring,     ///< Guitar amp spring tank (vintage, bouncy)
        Chamber,    ///< Echo chamber (smooth, musical)
        Ambient,    ///< Atmospheric reverb (ethereal, evolving)
        Nonlinear   ///< Gated/reverse reverb effects
    };
    
    /**
     * @brief Early reflection patterns
     */
    enum class EarlyReflectionPattern {
        Studio,     ///< Small studio space
        Hall,       ///< Concert hall
        Chamber,    ///< Stone chamber
        Room,       ///< Living room
        Booth,      ///< Vocal booth
        Outdoor     ///< Open air
    };
    
    // ========================================================================
    // CONSTRUCTION/DESTRUCTION
    // ========================================================================
    
    AlgorithmicReverb();
    virtual ~AlgorithmicReverb();
    
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
    
    std::string getName() const override { return "AlgorithmicReverb"; }
    uint32_t getLatency() const override;
    
    // ========================================================================
    // REVERB-SPECIFIC CONTROL
    // ========================================================================
    
    /**
     * @brief Set reverb algorithm type
     * @param type Reverb algorithm
     */
    void setReverbType(ReverbType type);
    
    /**
     * @brief Get current reverb type
     * @return Current reverb algorithm
     */
    ReverbType getReverbType() const;
    
    /**
     * @brief Set room size
     * @param size Room size (0.0 = small, 1.0 = large)
     */
    void setRoomSize(double size);
    
    /**
     * @brief Set decay time
     * @param time Decay time in seconds (0.1 to 60.0)
     */
    void setDecayTime(double time);
    
    /**
     * @brief Set high-frequency damping
     * @param damping Damping amount (0.0 = no damping, 1.0 = heavy damping)
     */
    void setDamping(double damping);
    
    /**
     * @brief Set dry/wet mix
     * @param mix Mix amount (0.0 = dry, 1.0 = wet)
     */
    void setDryWetMix(double mix);
    
    /**
     * @brief Set pre-delay time
     * @param delay Pre-delay in milliseconds (0 to 500)
     */
    void setPreDelay(double delay);
    
    /**
     * @brief Set stereo width
     * @param width Stereo width (0.0 = mono, 1.0 = normal, 2.0 = wide)
     */
    void setStereoWidth(double width);
    
    // ========================================================================
    // EARLY REFLECTIONS
    // ========================================================================
    
    /**
     * @brief Set early reflection pattern
     * @param pattern Early reflection pattern
     */
    void setEarlyReflectionPattern(EarlyReflectionPattern pattern);
    
    /**
     * @brief Set early reflection level
     * @param level Early reflection level (0.0 to 1.0)
     */
    void setEarlyReflectionLevel(double level);
    
    /**
     * @brief Set early reflection decay
     * @param decay Early reflection decay time in seconds
     */
    void setEarlyReflectionDecay(double decay);
    
    // ========================================================================
    // MODULATION
    // ========================================================================
    
    /**
     * @brief Set modulation depth and rate
     * @param depth Modulation depth (0.0 to 1.0)
     * @param rate Modulation rate in Hz (0.1 to 10.0)
     */
    void setModulation(double depth, double rate);
    
    /**
     * @brief Set chorus-style modulation
     * @param enabled True to enable chorus modulation
     */
    void setChorusModulation(bool enabled);
    
    // ========================================================================
    // ADVANCED CONTROLS
    // ========================================================================
    
    /**
     * @brief Enable/disable freeze mode
     * @param freeze True for infinite sustain
     */
    void setFreeze(bool freeze);
    
    /**
     * @brief Set diffusion amount
     * @param diffusion Diffusion (0.0 = focused, 1.0 = diffuse)
     */
    void setDiffusion(double diffusion);
    
    /**
     * @brief Set low-frequency decay ratio
     * @param ratio Low frequency decay ratio (0.1 to 2.0)
     */
    void setLowFrequencyDecay(double ratio);
    
    /**
     * @brief Set high-frequency decay ratio
     * @param ratio High frequency decay ratio (0.1 to 2.0)
     */
    void setHighFrequencyDecay(double ratio);
    
    /**
     * @brief Set crossover frequencies for decay ratios
     * @param lowCrossover Low crossover frequency in Hz
     * @param highCrossover High crossover frequency in Hz
     */
    void setCrossoverFrequencies(double lowCrossover, double highCrossover);
    
    // ========================================================================
    // ANALYSIS AND METERING
    // ========================================================================
    
    /**
     * @brief Get current reverb tail energy
     * @return Tail energy level (0.0 to 1.0)
     */
    double getTailEnergy() const;
    
    /**
     * @brief Get estimated RT60 time
     * @return RT60 decay time in seconds
     */
    double getRT60() const;
    
    /**
     * @brief Check if freeze mode is active
     * @return True if freeze is enabled
     */
    bool isFrozen() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace time
} // namespace dsp
} // namespace dawg
