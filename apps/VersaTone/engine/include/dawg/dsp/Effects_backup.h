#pragma once

/**
 * @file Effects.h
 * @brief Professional Real-Time DSP Effects Suite
 * @version 2.0.0
 * 
 * Complete modular audio effects library for professional DAW-quality processing.
 * 
 * This header provides access to all effect categories through a clean,
 * modular architecture that follows Single Responsibility Principle (SoC)
 * for maximum maintainability and extensibility.
 * 
 * ðŸ“Š DYNAMICS PROCESSING:
 * - Compressor, Limiter, Expander, Gate, Multiband Compressor
 * - De-esser, Transient Shaper
 * 
 * ðŸŽšï¸ EQUALIZATION:
 * - Parametric EQ, Graphic EQ, Linear Phase EQ
 * - High/Low/Band-pass filters, Shelving EQ, Notch filters
 * 
 * ðŸŒŠ MODULATION EFFECTS:
 * - Chorus, Flanger, Phaser, Tremolo, Vibrato
 * - Ring Modulator, Auto-Pan
 * 
 * â° TIME-BASED EFFECTS:
 * - Reverb, Delay, Ping-Pong Delay, Multi-tap Delay
 * - Slapback Echo
 * 
 * ðŸŽµ PITCH & HARMONY:
 * - Pitch Shifter, Harmonizer, Octaver, Auto-Tune
 * - Vocoder, Talk Box
 * 
 * ðŸ”Š DISTORTION & SATURATION:
 * - Overdrive, Distortion, Fuzz, Bit Crusher
 * - Sample Rate Reducer, Tape/Tube Saturation
 * 
 * ðŸŽ¯ SPECIALIZED PROCESSING:
 * - Stereo Widener, Mid/Side Processing, Crossover
 * - Spectrum Analyzer, Oscilloscope, Tuner
 * 
 * ðŸŽ¼ CREATIVE/SPECIALTY:
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
// ðŸ“Š DYNAMICS PROCESSING EFFECTS
// ============================================================================
#include "dynamics/Compressor.h"
// #include "dynamics/Limiter.h"                    // TODO: Implement
// #include "dynamics/Expander.h"                   // TODO: Implement  
// #include "dynamics/Gate.h"                       // TODO: Implement
// #include "dynamics/DeEsser.h"                    // TODO: Implement
// #include "dynamics/TransientShaper.h"            // TODO: Implement
// #include "dynamics/MultibandCompressor.h"        // TODO: Implement

// ============================================================================
// ðŸŽšï¸ EQUALIZATION EFFECTS
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
// ðŸŒŠ MODULATION EFFECTS
// ============================================================================
// #include "modulation/Chorus.h"                   // TODO: Implement
// #include "modulation/Flanger.h"                  // TODO: Implement
// #include "modulation/Phaser.h"                   // TODO: Implement
// #include "modulation/Tremolo.h"                  // TODO: Implement
// #include "modulation/Vibrato.h"                  // TODO: Implement
// #include "modulation/RingModulator.h"            // TODO: Implement
// #include "modulation/AutoPan.h"                  // TODO: Implement

// ============================================================================
// â° TIME-BASED EFFECTS
// ============================================================================
#include "time/AlgorithmicReverb.h"
// #include "time/Delay.h"                          // TODO: Implement
// #include "time/PingPongDelay.h"                  // TODO: Implement
// #include "time/MultiTapDelay.h"                  // TODO: Implement
// #include "time/SlapbackEcho.h"                   // TODO: Implement

// ============================================================================
// ðŸŽµ PITCH & HARMONY EFFECTS
// ============================================================================
// #include "pitch/PitchShifter.h"                  // TODO: Implement
// #include "pitch/Harmonizer.h"                    // TODO: Implement
// #include "pitch/Octaver.h"                       // TODO: Implement
// #include "pitch/AutoTune.h"                      // TODO: Implement
// #include "pitch/Vocoder.h"                       // TODO: Implement
// #include "pitch/TalkBox.h"                       // TODO: Implement

// ============================================================================
// ðŸ”Š DISTORTION & SATURATION EFFECTS
// ============================================================================
// #include "distortion/HarmonicDistortion.h"       // TODO: Move from old file
// #include "distortion/Overdrive.h"                // TODO: Implement
// #include "distortion/Fuzz.h"                     // TODO: Implement
// #include "distortion/BitCrusher.h"               // TODO: Implement
// #include "distortion/SampleRateReducer.h"        // TODO: Implement
// #include "distortion/TapeSaturation.h"           // TODO: Implement
// #include "distortion/TubeSaturation.h"           // TODO: Implement

// ============================================================================
// ðŸŽ¯ SPECIALIZED PROCESSING EFFECTS
// ============================================================================
// #include "spatial/StereoWidener.h"               // TODO: Implement
// #include "spatial/MidSideProcessor.h"            // TODO: Implement
// #include "spatial/Crossover.h"                   // TODO: Implement
// #include "analysis/SpectrumAnalyzer.h"           // TODO: Implement
// #include "analysis/Oscilloscope.h"               // TODO: Implement
// #include "analysis/Tuner.h"                      // TODO: Implement

// ============================================================================
// ðŸŽ¼ CREATIVE/SPECIALTY EFFECTS
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
    EffectsChain();
    ~EffectsChain();
    
    // ========================================================================
    // CHAIN MANAGEMENT
    // ========================================================================
    
    /**
     * @brief Add effect to end of chain
     * @param effect Effect to add (takes ownership)
     */
    void addEffect(std::unique_ptr<Effect> effect);
    
    /**
     * @brief Insert effect at specific position
     * @param index Position to insert at
     * @param effect Effect to insert (takes ownership)
     */
    void insertEffect(size_t index, std::unique_ptr<Effect> effect);
    
    /**
     * @brief Remove effect from chain
     * @param index Effect index to remove
     * @return Removed effect (ownership transferred)
     */
    std::unique_ptr<Effect> removeEffect(size_t index);
    
    /**
     * @brief Move effect to different position
     * @param from Source index
     * @param to Destination index
     */
    void moveEffect(size_t from, size_t to);
    
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
     * @brief Get effect at specific index (const)
     * @param index Effect index
     * @return Const pointer to effect (null if invalid index)
     */
    const Effect* getEffect(size_t index) const;
    
    /**
     * @brief Get number of effects in chain
     * @return Number of effects
     */
    size_t getNumEffects() const;
    
    /**
     * @brief Check if chain is empty
     * @return True if no effects in chain
     */
    bool isEmpty() const;
    
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
    
    // ========================================================================
    // UTILITY
    // ========================================================================
    
    /**
     * @brief Get total latency of all effects
     * @return Total latency in samples
     */
    uint32_t getTotalLatency() const;
    
    /**
     * @brief Get chain information string
     * @return String describing the effects chain
     */
    std::string getChainInfo() const;

private:
    std::vector<std::unique_ptr<Effect>> effects;
    bool chainBypassed = false;
    uint32_t sampleRate = 48000;
    uint32_t bufferSize = 512;
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

#include <cstdint>
#include <memory>
#include <vector>
#include <string>

// Forward declarations
namespace dawg { namespace core { class AudioBuffer; } }

namespace dawg {
namespace dsp {

/**
 * @brief Base class for all audio effects
 */
class Effect {
public:
    virtual ~Effect() = default;
    
    // Core processing
    virtual void process(core::AudioBuffer& buffer) = 0;
    virtual void reset() = 0;
    
    // Parameter control
    virtual void setParameter(const std::string& name, double value) = 0;
    virtual double getParameter(const std::string& name) const = 0;
    virtual std::vector<std::string> getParameterNames() const = 0;
    
    // State management
    virtual void setSampleRate(uint32_t sampleRate) = 0;
    virtual void setBufferSize(uint32_t bufferSize) = 0;
    
    // Bypass control
    void setBypassed(bool bypassed) { this->bypassed = bypassed; }
    bool isBypassed() const { return bypassed; }
    
protected:
    bool bypassed = false;
    uint32_t sampleRate = 48000;
    uint32_t bufferSize = 512;
};

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
    enum class FilterType {
        HighPass,
        LowPass,
        HighShelf,
        LowShelf,
        Bell,
        Notch
    };
    
    struct Band {
        FilterType type = FilterType::Bell;
        double frequency = 1000.0;    // Hz
        double gain = 0.0;            // dB
        double q = 0.707;             // Quality factor
        bool enabled = false;
    };
    
    ParametricEQ();
    virtual ~ParametricEQ();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    // Parameter control
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Band control
    void setBand(uint32_t bandIndex, const Band& band);
    Band getBand(uint32_t bandIndex) const;
    uint32_t getNumBands() const { return 8; }
    
    // Analysis
    std::vector<double> getFrequencyResponse(const std::vector<double>& frequencies) const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Professional multiband compressor
 * 
 * Features:
 * - 4 independent compression bands
 * - Vintage VCA, FET, or Optical modeling
 * - Automatic gain compensation
 * - Side-chain support
 * - Look-ahead limiting
 * - Real-time gain reduction metering
 */
class MultibandCompressor : public Effect {
public:
    enum class CompressorType {
        VCA,        // Clean, precise compression
        FET,        // Warm, musical compression  
        Optical,    // Smooth, program-dependent
        Digital     // Transparent, surgical
    };
    
    struct Band {
        double lowFreq = 100.0;       // Hz
        double highFreq = 1000.0;     // Hz
        double threshold = -12.0;     // dB
        double ratio = 4.0;           // X:1
        double attack = 10.0;         // ms
        double release = 100.0;       // ms
        double knee = 2.0;            // dB
        double makeup = 0.0;          // dB
        CompressorType type = CompressorType::VCA;
        bool enabled = true;
    };
    
    MultibandCompressor();
    virtual ~MultibandCompressor();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    // Parameter control
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Band control
    void setBand(uint32_t bandIndex, const Band& band);
    Band getBand(uint32_t bandIndex) const;
    uint32_t getNumBands() const { return 4; }
    
    // Metering
    std::vector<double> getGainReduction() const;  // dB per band
    double getOverallGainReduction() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

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
    enum class ReverbType {
        Hall,       // Large concert hall
        Room,       // Medium room/studio
        Plate,      // Vintage plate reverb
        Spring,     // Guitar amp spring tank
        Chamber     // Echo chamber
    };
    
    AlgorithmicReverb();
    virtual ~AlgorithmicReverb();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    // Parameter control
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Reverb control
    void setReverbType(ReverbType type);
    void setRoomSize(double size);        // 0.0 - 1.0
    void setDecayTime(double time);       // seconds
    void setDamping(double damping);      // 0.0 - 1.0
    void setDryWetMix(double mix);        // 0.0 = dry, 1.0 = wet
    void setPreDelay(double delay);       // ms
    void setModulation(double depth, double rate);
    void setFreeze(bool freeze);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Harmonic distortion and saturation
 * 
 * Features:
 * - Multiple saturation models (Tube, Tape, Transformer)
 * - Harmonic enhancement algorithms
 * - Bit crushing and sample rate reduction
 * - Drive and tone controls
 * - Mix control for parallel processing
 */
class HarmonicDistortion : public Effect {
public:
    enum class DistortionType {
        Tube,           // Warm tube saturation
        Tape,           // Analog tape compression
        Transformer,    // Iron core saturation
        Digital,        // Digital clipping
        BitCrush        // Lo-fi bit reduction
    };
    
    HarmonicDistortion();
    virtual ~HarmonicDistortion();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    // Parameter control
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Distortion control
    void setDistortionType(DistortionType type);
    void setDrive(double drive);          // 0.0 - 10.0
    void setTone(double tone);            // -1.0 to 1.0 (dark to bright)
    void setMix(double mix);              // 0.0 = dry, 1.0 = wet
    void setBitDepth(uint32_t bits);      // For bit crushing
    void setSampleRateReduction(double factor); // For sample rate reduction
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// ============================================================================
// ðŸ“Š DYNAMICS PROCESSING EFFECTS
// ============================================================================

/**
 * @brief Professional single-band compressor
 * 
 * Features:
 * - Multiple compressor models (VCA, FET, Optical, Digital)
 * - Variable knee and look-ahead
 * - Side-chain support
 * - Auto-makeup gain
 * - Real-time gain reduction metering
 */
class Compressor : public Effect {
public:
    enum class CompressorModel {
        VCA,        // Clean, precise compression
        FET,        // Warm, musical compression (1176 style)
        Optical,    // Smooth, program-dependent (LA-2A style)
        Digital     // Transparent, surgical
    };
    
    Compressor();
    virtual ~Compressor();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Compressor control
    void setModel(CompressorModel model);
    void setThreshold(double threshold);      // dB
    void setRatio(double ratio);              // X:1
    void setAttack(double attack);            // ms
    void setRelease(double release);          // ms
    void setKnee(double knee);                // dB
    void setMakeupGain(double gain);          // dB
    void setAutoMakeup(bool enabled);
    void setLookAhead(double time);           // ms
    
    // Side-chain
    void setSideChainEnabled(bool enabled);
    void processSideChain(const core::AudioBuffer& sideChain);
    
    // Metering
    double getGainReduction() const;          // dB
    double getInputLevel() const;             // dB
    double getOutputLevel() const;            // dB
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Brick-wall limiter for mastering
 * 
 * Features:
 * - True peak limiting with oversampling
 * - ISP (Inter-Sample Peak) detection
 * - Multiple release algorithms
 * - Look-ahead processing
 * - Dithering options
 */
class Limiter : public Effect {
public:
    enum class ReleaseMode {
        Auto,       // Program-dependent release
        Manual,     // Fixed release time
        Adaptive    // Dynamic release adaptation
    };
    
    Limiter();
    virtual ~Limiter();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Limiter control
    void setCeiling(double ceiling);          // dB
    void setRelease(double release);          // ms
    void setReleaseMode(ReleaseMode mode);
    void setLookAhead(double time);           // ms
    void setOversampling(uint32_t factor);    // 2x, 4x, 8x
    void setISPDetection(bool enabled);
    
    // Metering
    double getGainReduction() const;          // dB
    double getTruePeakLevel() const;          // dBTP
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Effects chain for combining multiple effects
 */
class EffectsChain {
public:
    EffectsChain();
    ~EffectsChain();
    
    // Chain management
    void addEffect(std::unique_ptr<Effect> effect);
    void removeEffect(size_t index);
    void moveEffect(size_t from, size_t to);
    void clear();
    
    // Processing
    void process(core::AudioBuffer& buffer);
    void reset();
    void setSampleRate(uint32_t sampleRate);
    void setBufferSize(uint32_t bufferSize);
    
    // Access
    Effect* getEffect(size_t index);
    size_t getNumEffects() const;
    
    // Bypass control
    void setChainBypassed(bool bypassed) { chainBypassed = bypassed; }
    bool isChainBypassed() const { return chainBypassed; }
    
private:
    std::vector<std::unique_ptr<Effect>> effects;
    bool chainBypassed = false;
};

// ============================================================================
// ðŸŒŠ MODULATION EFFECTS
// ============================================================================

/**
 * @brief Chorus effect for lush, wide sound
 */
class Chorus : public Effect {
public:
    Chorus();
    virtual ~Chorus();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Chorus control
    void setDepth(double depth);              // 0.0 to 1.0
    void setRate(double rate);                // Hz
    void setDelay(double delay);              // ms
    void setFeedback(double feedback);        // 0.0 to 1.0
    void setMix(double mix);                  // 0.0 = dry, 1.0 = wet
    void setVoices(uint32_t voices);          // Number of chorus voices
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Flanger effect for sweeping comb filtering
 */
class Flanger : public Effect {
public:
    Flanger();
    virtual ~Flanger();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Flanger control
    void setDepth(double depth);              // 0.0 to 1.0
    void setRate(double rate);                // Hz
    void setDelay(double delay);              // ms
    void setFeedback(double feedback);        // -1.0 to 1.0
    void setMix(double mix);                  // 0.0 = dry, 1.0 = wet
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Phaser effect for all-pass filtering modulation
 */
class Phaser : public Effect {
public:
    Phaser();
    virtual ~Phaser();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Phaser control
    void setDepth(double depth);              // 0.0 to 1.0
    void setRate(double rate);                // Hz
    void setFeedback(double feedback);        // 0.0 to 1.0
    void setStages(uint32_t stages);          // Number of all-pass stages
    void setMix(double mix);                  // 0.0 = dry, 1.0 = wet
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Tremolo effect for amplitude modulation
 */
class Tremolo : public Effect {
public:
    enum class WaveShape {
        Sine,
        Triangle,
        Square,
        Sawtooth
    };
    
    Tremolo();
    virtual ~Tremolo();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Tremolo control
    void setDepth(double depth);              // 0.0 to 1.0
    void setRate(double rate);                // Hz
    void setWaveShape(WaveShape shape);
    void setStereoPhase(double phase);        // degrees
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// ============================================================================
// â° TIME-BASED EFFECTS
// ============================================================================

/**
 * @brief Digital delay with multiple taps and filtering
 */
class Delay : public Effect {
public:
    Delay();
    virtual ~Delay();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Delay control
    void setDelayTime(double time);           // ms
    void setFeedback(double feedback);        // 0.0 to 1.0
    void setMix(double mix);                  // 0.0 = dry, 1.0 = wet
    void setLowCut(double frequency);         // Hz
    void setHighCut(double frequency);        // Hz
    void setSync(bool sync);                  // Tempo sync
    void setTempoMultiplier(double multiplier); // For tempo sync
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Ping-pong delay bouncing between stereo channels
 */
class PingPongDelay : public Effect {
public:
    PingPongDelay();
    virtual ~PingPongDelay();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Ping-pong control
    void setDelayTime(double time);           // ms
    void setFeedback(double feedback);        // 0.0 to 1.0
    void setMix(double mix);                  // 0.0 = dry, 1.0 = wet
    void setStereoWidth(double width);        // 0.0 to 1.0
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// ============================================================================
// ðŸŽµ PITCH & HARMONY EFFECTS
// ============================================================================

/**
 * @brief Pitch shifter for real-time pitch changing
 */
class PitchShifter : public Effect {
public:
    enum class Algorithm {
        Granular,   // High quality, some latency
        PSOLA,      // Good for vocals
        PhaseVocoder, // Classic algorithm
        Formant     // Preserves formants
    };
    
    PitchShifter();
    virtual ~PitchShifter();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Pitch control
    void setPitchShift(double semitones);     // -24 to +24 semitones
    void setFineTune(double cents);           // -100 to +100 cents
    void setAlgorithm(Algorithm algorithm);
    void setMix(double mix);                  // 0.0 = dry, 1.0 = wet
    void setFormantCorrection(bool enabled);
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Harmonizer for adding harmonic intervals
 */
class Harmonizer : public Effect {
public:
    struct Voice {
        double pitchShift = 0.0;              // semitones
        double pan = 0.0;                     // -1.0 to 1.0
        double level = 1.0;                   // 0.0 to 1.0
        double delay = 0.0;                   // ms
        bool enabled = false;
    };
    
    Harmonizer();
    virtual ~Harmonizer();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Harmonizer control
    void setVoice(uint32_t voiceIndex, const Voice& voice);
    Voice getVoice(uint32_t voiceIndex) const;
    uint32_t getNumVoices() const { return 4; }
    void setMix(double mix);                  // 0.0 = dry, 1.0 = wet
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

// ============================================================================
// ðŸŽ¯ SPECIALIZED PROCESSING EFFECTS  
// ============================================================================

/**
 * @brief Stereo width enhancer
 */
class StereoWidener : public Effect {
public:
    StereoWidener();
    virtual ~StereoWidener();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Stereo control
    void setWidth(double width);              // 0.0 to 2.0 (0=mono, 1=normal, 2=wide)
    void setBass(double bass);                // Low frequency mono amount
    void setFrequency(double frequency);      // Hz (bass/stereo crossover)
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Mid/Side processor for independent center/sides processing
 */
class MidSideProcessor : public Effect {
public:
    MidSideProcessor();
    virtual ~MidSideProcessor();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Mid/Side control
    void setMidGain(double gain);             // dB
    void setSideGain(double gain);            // dB
    void setMidWidth(double width);           // 0.0 to 2.0
    void setSideWidth(double width);          // 0.0 to 2.0
    void setMonoBelow(double frequency);      // Hz
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Spectrum analyzer for visual frequency analysis
 */
class SpectrumAnalyzer : public Effect {
public:
    SpectrumAnalyzer();
    virtual ~SpectrumAnalyzer();
    
    void process(core::AudioBuffer& buffer) override;
    void reset() override;
    void setSampleRate(uint32_t sampleRate) override;
    void setBufferSize(uint32_t bufferSize) override;
    
    void setParameter(const std::string& name, double value) override;
    double getParameter(const std::string& name) const override;
    std::vector<std::string> getParameterNames() const override;
    
    // Analysis
    std::vector<double> getSpectrum() const;         // Magnitude spectrum
    std::vector<double> getFrequencies() const;      // Frequency bins
    double getPeakFrequency() const;                 // Hz
    double getRMSLevel() const;                      // dB
    
    // Configuration
    void setFFTSize(uint32_t size);                  // 256, 512, 1024, 2048, 4096
    void setWindowType(const std::string& window);   // Hann, Hamming, Blackman
    void setAveraging(double factor);                // 0.0 to 1.0
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace dsp
} // namespace dawg

