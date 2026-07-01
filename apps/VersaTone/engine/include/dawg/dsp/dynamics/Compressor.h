#pragma once

/**
 * @file Compressor.h
 * @brief Professional single-band compressor
 * @version 1.0.0
 * 
 * High-quality compressor with multiple models and advanced features:
 * - VCA, FET, Optical, and Digital compressor models
 * - Variable knee and look-ahead processing
 * - Side-chain support for ducking/pumping effects
 * - Auto-makeup gain compensation
 * - Real-time gain reduction metering
 */

#include "../EffectBase.h"
#include <memory>

namespace dawg {
namespace dsp {
namespace dynamics {

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
    /**
     * @brief Compressor model types
     */
    enum class CompressorModel {
        VCA,        ///< Clean, precise compression (SSL style)
        FET,        ///< Warm, musical compression (1176 style)
        Optical,    ///< Smooth, program-dependent (LA-2A style)
        Digital     ///< Transparent, surgical compression
    };
    
    // ========================================================================
    // CONSTRUCTION/DESTRUCTION
    // ========================================================================
    
    Compressor();
    virtual ~Compressor();
    
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
    
    std::string getName() const override { return "Compressor"; }
    uint32_t getLatency() const override;
    
    // ========================================================================
    // COMPRESSOR-SPECIFIC CONTROL
    // ========================================================================
    
    /**
     * @brief Set compressor model
     * @param model Compressor model type
     */
    void setModel(CompressorModel model);
    
    /**
     * @brief Set compression threshold
     * @param threshold Threshold in dB (-60 to 0)
     */
    void setThreshold(double threshold);
    
    /**
     * @brief Set compression ratio
     * @param ratio Compression ratio (1:1 to 20:1)
     */
    void setRatio(double ratio);
    
    /**
     * @brief Set attack time
     * @param attack Attack time in milliseconds (0.1 to 100)
     */
    void setAttack(double attack);
    
    /**
     * @brief Set release time
     * @param release Release time in milliseconds (1 to 5000)
     */
    void setRelease(double release);
    
    /**
     * @brief Set knee hardness
     * @param knee Knee in dB (0 = hard knee, 10 = soft knee)
     */
    void setKnee(double knee);
    
    /**
     * @brief Set makeup gain
     * @param gain Makeup gain in dB (-20 to +20)
     */
    void setMakeupGain(double gain);
    
    /**
     * @brief Enable/disable auto makeup gain
     * @param enabled True to enable auto makeup
     */
    void setAutoMakeup(bool enabled);
    
    /**
     * @brief Set look-ahead time
     * @param time Look-ahead time in milliseconds (0 to 10)
     */
    void setLookAhead(double time);
    
    // ========================================================================
    // SIDE-CHAIN PROCESSING
    // ========================================================================
    
    /**
     * @brief Enable/disable side-chain processing
     * @param enabled True to enable side-chain
     */
    void setSideChainEnabled(bool enabled);
    
    /**
     * @brief Process side-chain input
     * @param sideChain Side-chain audio buffer
     */
    void processSideChain(const core::AudioBuffer& sideChain);
    
    /**
     * @brief Set side-chain high-pass filter frequency
     * @param frequency Frequency in Hz (20 to 500)
     */
    void setSideChainHighPass(double frequency);
    
    // ========================================================================
    // METERING AND ANALYSIS
    // ========================================================================
    
    /**
     * @brief Get current gain reduction
     * @return Gain reduction in dB (always negative or zero)
     */
    double getGainReduction() const;
    
    /**
     * @brief Get input level
     * @return Input level in dB
     */
    double getInputLevel() const;
    
    /**
     * @brief Get output level
     * @return Output level in dB
     */
    double getOutputLevel() const;
    
    /**
     * @brief Get current compressor model
     * @return Current compressor model
     */
    CompressorModel getModel() const;
    
    /**
     * @brief Check if compressor is currently compressing
     * @return True if gain reduction is active
     */
    bool isCompressing() const;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace dynamics
} // namespace dsp
} // namespace dawg
