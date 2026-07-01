#pragma once

/**
 * @file Effects.h
 * @brief Professional Real-Time DSP Effects Suite - Modular Architecture
 * @version 2.0.0
 * 
 * Clean modular effects library following Single Responsibility Principle (SoC).
 * Each effect is in its own file for maximum maintainability and extensibility.
 * 
 * 📊 DYNAMICS PROCESSING:
 * - Compressor, Limiter, Expander, Gate, Multiband Compressor
 * - De-esser, Transient Shaper
 * 
 * 🎚️ EQUALIZATION:
 * - Parametric EQ, Graphic EQ, Linear Phase EQ
 * - High/Low/Band-pass filters, Shelving EQ, Notch filters
 * 
 * 🌊 MODULATION EFFECTS:
 * - Chorus, Flanger, Phaser, Tremolo, Vibrato
 * - Ring Modulator, Auto-Pan
 * 
 * ⏰ TIME-BASED EFFECTS:
 * - Reverb, Delay, Ping-Pong Delay, Multi-tap Delay
 * - Slapback Echo
 * 
 * 🎵 PITCH & HARMONY:
 * - Pitch Shifter, Harmonizer, Octaver, Auto-Tune
 * - Vocoder, Talk Box
 * 
 * 🔊 DISTORTION & SATURATION:
 * - Overdrive, Distortion, Fuzz, Bit Crusher
 * - Sample Rate Reducer, Tape/Tube Saturation
 * 
 * 🎯 SPECIALIZED PROCESSING:
 * - Stereo Widener, Mid/Side Processing, Crossover
 * - Spectrum Analyzer, Oscilloscope, Tuner
 * 
 * 🎼 CREATIVE/SPECIALTY:
 * - Granular Delay, Reverse Reverb, Gated Reverb
 * - Filter Sweep, Stutter Edit, Glitch, Formant Filter
 * 
 * @example
 * ```cpp
 * #include "dawg/dsp/Effects.h"
 * 
 * using namespace dawg::dsp;
 * 
 * // Create modular effects
 * auto compressor = std::make_unique<dynamics::Compressor>();
 * auto eq = std::make_unique<eq::ParametricEQ>();
 * auto reverb = std::make_unique<time::AlgorithmicReverb>();
 * 
 * // Configure effects
 * compressor->setThreshold(-12.0);
 * compressor->setRatio(4.0);
 * 
 * eq->setBand(0, {ParametricEQ::FilterType::HighPass, 80.0});
 * eq->setBand(1, {ParametricEQ::FilterType::Bell, 1000.0, 3.0, 1.4});
 * 
 * reverb->setReverbType(AlgorithmicReverb::ReverbType::Hall);
 * reverb->setDecayTime(2.5);
 * 
 * // Build effects chain
 * EffectsChain chain;
 * chain.addEffect(std::move(compressor));
 * chain.addEffect(std::move(eq));
 * chain.addEffect(std::move(reverb));
 * 
 * // Process audio
 * chain.process(audioBuffer);
 * ```
 */

// ============================================================================
// BASE EFFECT INTERFACE
// ============================================================================
#include "EffectBase.h"

// ============================================================================
// 📊 DYNAMICS PROCESSING EFFECTS
// ============================================================================
#include "dynamics/Compressor.h"
// #include "dynamics/Limiter.h"                    // TODO: Implement
// #include "dynamics/Expander.h"                   // TODO: Implement  
// #include "dynamics/Gate.h"                       // TODO: Implement
// #include "dynamics/DeEsser.h"                    // TODO: Implement
// #include "dynamics/TransientShaper.h"            // TODO: Implement
// #include "dynamics/MultibandCompressor.h"        // TODO: Implement

// ============================================================================
// 🎚️ EQUALIZATION EFFECTS
// ============================================================================
#include "eq/ParametricEQ.h"
// #include "eq/GraphicEQ.h"                        // TODO: Implement
// #include "eq/LinearPhaseEQ.h"                    // TODO: Implement
// #include "eq/HighPassFilter.h"                   // TODO: Implement
// #include "eq/LowPassFilter.h"                    // TODO: Implement
// #include "eq/BandPassFilter.h"                   // TODO: Implement
// #include "eq/NotchFilter.h"                      // TODO: Implement
// #include "eq/ShelvingEQ.h"                       // TODO: Implement

// ============================================================================
// 🌊 MODULATION EFFECTS
// ============================================================================
// #include "modulation/Chorus.h"                   // TODO: Implement
// #include "modulation/Flanger.h"                  // TODO: Implement
// #include "modulation/Phaser.h"                   // TODO: Implement
// #include "modulation/Tremolo.h"                  // TODO: Implement
// #include "modulation/Vibrato.h"                  // TODO: Implement
// #include "modulation/RingModulator.h"            // TODO: Implement
// #include "modulation/AutoPan.h"                  // TODO: Implement

// ============================================================================
// ⏰ TIME-BASED EFFECTS
// ============================================================================
#include "time/AlgorithmicReverb.h"
// #include "time/Delay.h"                          // TODO: Implement
// #include "time/PingPongDelay.h"                  // TODO: Implement
// #include "time/MultiTapDelay.h"                  // TODO: Implement
// #include "time/SlapbackEcho.h"                   // TODO: Implement

// ============================================================================
// 🎵 PITCH & HARMONY EFFECTS
// ============================================================================
// #include "pitch/PitchShifter.h"                  // TODO: Implement
// #include "pitch/Harmonizer.h"                    // TODO: Implement
// #include "pitch/Octaver.h"                       // TODO: Implement
// #include "pitch/AutoTune.h"                      // TODO: Implement
// #include "pitch/Vocoder.h"                       // TODO: Implement
// #include "pitch/TalkBox.h"                       // TODO: Implement

// ============================================================================
// 🔊 DISTORTION & SATURATION EFFECTS
// ============================================================================
// #include "distortion/HarmonicDistortion.h"       // TODO: Move from old file
// #include "distortion/Overdrive.h"                // TODO: Implement
// #include "distortion/Fuzz.h"                     // TODO: Implement
// #include "distortion/BitCrusher.h"               // TODO: Implement
// #include "distortion/SampleRateReducer.h"        // TODO: Implement
// #include "distortion/TapeSaturation.h"           // TODO: Implement
// #include "distortion/TubeSaturation.h"           // TODO: Implement

// ============================================================================
// 🎯 SPECIALIZED PROCESSING EFFECTS
// ============================================================================
// #include "spatial/StereoWidener.h"               // TODO: Implement
// #include "spatial/MidSideProcessor.h"            // TODO: Implement
// #include "spatial/Crossover.h"                   // TODO: Implement
// #include "analysis/SpectrumAnalyzer.h"           // TODO: Implement
// #include "analysis/Oscilloscope.h"               // TODO: Implement
// #include "analysis/Tuner.h"                      // TODO: Implement

// ============================================================================
// 🎼 CREATIVE/SPECIALTY EFFECTS
// ============================================================================
// #include "creative/GranularDelay.h"              // TODO: Implement
// #include "creative/ReverseReverb.h"              // TODO: Implement
// #include "creative/GatedReverb.h"                // TODO: Implement
// #include "creative/FilterSweep.h"                // TODO: Implement
// #include "creative/StutterEdit.h"                // TODO: Implement
// #include "creative/Glitch.h"                     // TODO: Implement
// #include "creative/FormantFilter.h"              // TODO: Implement

// ============================================================================
// EFFECTS CHAIN SYSTEM
// ============================================================================

#include <vector>
#include <memory>

namespace dawg {
namespace dsp {

/**
 * @brief Effects chain for combining multiple effects in series
 * 
 * Provides a professional effects routing system with:
 * - Serial effect processing
 * - Individual effect bypass
 * - Chain-wide bypass
 * - Effect reordering
 * - Real-time safe operation
 */
class EffectsChain {
public:
    EffectsChain() = default;
    ~EffectsChain() = default;
    
    // ========================================================================
    // CHAIN MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Add effect to end of chain
     * @param effect Effect to add (takes ownership)
     */
    void addEffect(std::unique_ptr<Effect> effect);
    
    /**
     * @brief Remove effect from chain
     * @param index Effect index to remove
     * @return Removed effect (ownership transferred)
     */
    std::unique_ptr<Effect> removeEffect(size_t index);
    
    /**
     * @brief Clear all effects from chain
     */
    void clear();
    
    // ========================================================================
    // PROCESSING
    // ========================================================================
    
    /**
     * @brief Process audio through entire chain
     * @param buffer Audio buffer to process
     */
    void process(core::AudioBuffer& buffer);
    
    /**
     * @brief Reset all effects in chain
     */
    void reset();
    
    /**
     * @brief Set sample rate for all effects
     * @param sampleRate Sample rate in Hz
     */
    void setSampleRate(uint32_t sampleRate);
    
    /**
     * @brief Set buffer size for all effects
     * @param bufferSize Buffer size in samples
     */
    void setBufferSize(uint32_t bufferSize);
    
    // ========================================================================
    // ACCESS AND CONTROL
    // ========================================================================
    
    /**
     * @brief Get effect at specific index
     * @param index Effect index
     * @return Pointer to effect (null if invalid index)
     */
    Effect* getEffect(size_t index);
    
    /**
     * @brief Get number of effects in chain
     * @return Number of effects
     */
    size_t getNumEffects() const;
    
    // ========================================================================
    // BYPASS CONTROL
    // ========================================================================
    
    /**
     * @brief Enable or disable chain-wide bypass
     * @param bypassed True to bypass entire chain
     */
    void setChainBypassed(bool bypassed);
    
    /**
     * @brief Check if chain is bypassed
     * @return True if chain is bypassed
     */
    bool isChainBypassed() const;

private:
    std::vector<std::unique_ptr<Effect>> effects;
    bool chainBypassed = false;
};

// ============================================================================
// CONVENIENCE ALIASES FOR EASIER ACCESS
// ============================================================================

// Type aliases for commonly used effects
using Comp = dynamics::Compressor;
using ParaEQ = eq::ParametricEQ;
using Reverb = time::AlgorithmicReverb;

} // namespace dsp
} // namespace dawg
