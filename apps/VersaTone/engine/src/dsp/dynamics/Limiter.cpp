#include "../../../include/dawg/dsp/dynamics/Limiter.h"
#include "../../../include/dawg/core/AudioBuffer.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dawg {
namespace dsp {
namespace dynamics {

// Professional Limiter Implementation
class Limiter::Impl {
public:
    // ========================================================================
    // LIMITER PARAMETERS
    // ========================================================================
    
    double ceiling = -0.1;           // Output ceiling in dB
    double release = 50.0;           // Release time in ms (manual mode)
    double lookAheadTime = 5.0;      // Look-ahead time in ms
    double inputGain = 0.0;          // Input gain in dB
    double outputGain = 0.0;         // Output gain in dB
    
    ReleaseMode releaseMode = ReleaseMode::Auto;
    uint32_t oversamplingFactor = 2; // 2x oversampling default
    bool ispDetection = true;        // Inter-sample peak detection
    
    uint32_t sampleRate = 48000;
    
    // ========================================================================
    // PROCESSING STATE
    // ========================================================================
    
    // Look-ahead delay line
    std::vector<std::vector<float>> delayLine;
    std::vector<size_t> delayIndex;
    size_t lookAheadSamples = 0;
    
    // Envelope following
    double envelope = 0.0;
    double targetGainReduction = 0.0;
    double currentGainReduction = 0.0;
    
    // Release timing
    double attackCoeff = 0.0;
    double releaseCoeff = 0.0;
    double adaptiveRelease = 50.0;   // Current adaptive release time
    
    // Oversampling buffers
    std::vector<std::vector<float>> oversampledBuffer;
    std::vector<float> upsampleFilter;
    std::vector<float> downsampleFilter;
    
    // Metering
    double truePeakLevel = -96.0;    // dBTP
    double rmsLevel = -96.0;         // dB RMS
    double gainReductionMeter = 0.0; // For UI display
    bool isCurrentlyLimiting = false;
    
    // Signal analysis for adaptive release
    double signalEnergy = 0.0;
    double transientDetector = 0.0;
    size_t analysisCounter = 0;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    Impl() {
        initialize(48000);
    }
    
    void initialize(uint32_t newSampleRate) {
        sampleRate = newSampleRate;
        
        // Calculate look-ahead buffer size
        updateLookAheadBuffer();
        
        // Initialize envelope coefficients
        updateTimingCoefficients();
        
        // Initialize oversampling
        initializeOversampling();
        
        // Reset state
        reset();
    }
    
    void updateLookAheadBuffer() {
        lookAheadSamples = static_cast<size_t>(lookAheadTime * sampleRate / 1000.0);
        if (lookAheadSamples == 0) lookAheadSamples = 1; // Minimum 1 sample
        
        // Resize delay line for 2 channels
        delayLine.resize(2);
        delayIndex.resize(2);
        
        for (size_t ch = 0; ch < 2; ++ch) {
            delayLine[ch].resize(lookAheadSamples, 0.0f);
            delayIndex[ch] = 0;
        }
    }
    
    void updateTimingCoefficients() {
        // Attack: very fast (0.1ms) for limiting
        double attackTime = 0.1; // ms
        attackCoeff = 1.0 - std::exp(-1.0 / (attackTime * sampleRate / 1000.0));
        
        // Release: depends on mode
        double effectiveRelease = getEffectiveReleaseTime();
        releaseCoeff = 1.0 - std::exp(-1.0 / (effectiveRelease * sampleRate / 1000.0));
    }
    
    double getEffectiveReleaseTime() const {
        switch (releaseMode) {
            case ReleaseMode::Manual:
                return release;
            case ReleaseMode::Adaptive:
                return adaptiveRelease;
            case ReleaseMode::Auto:
            default:
                // Program-dependent: faster for transients, slower for sustained material
                double baseRelease = 25.0; // Base release time
                double transientFactor = 1.0 + transientDetector * 2.0; // 1.0 to 3.0
                return baseRelease * transientFactor;
        }
    }
    
    void initializeOversampling() {
        if (oversamplingFactor < 2) {
            oversamplingFactor = 1; // No oversampling
            return;
        }
        
        // Simple linear interpolation filters for oversampling
        // In production, would use proper anti-aliasing filters
        size_t filterLength = 64;
        upsampleFilter.resize(filterLength);
        downsampleFilter.resize(filterLength);
        
        // Design simple lowpass filters
        double cutoff = 0.4; // Normalized frequency
        for (size_t i = 0; i < filterLength; ++i) {
            double n = static_cast<double>(i) - (filterLength - 1) * 0.5;
            if (n == 0.0) {
                upsampleFilter[i] = static_cast<float>(cutoff);
                downsampleFilter[i] = static_cast<float>(cutoff);
            } else {
                double sinc = std::sin(M_PI * cutoff * n) / (M_PI * n);
                double window = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / (filterLength - 1)); // Hamming
                upsampleFilter[i] = static_cast<float>(sinc * window);
                downsampleFilter[i] = static_cast<float>(sinc * window);
            }
        }
        
        // Initialize oversampled buffers
        oversampledBuffer.resize(2);
        // Will be resized per buffer as needed
    }
    
    // ========================================================================
    // MAIN PROCESSING
    // ========================================================================
    
    void process(dawg::core::AudioBuffer& buffer) {
        if (buffer.getChannelCount() == 0 || buffer.getSampleCount() == 0) return;
        
        uint32_t channels = buffer.getChannelCount();
        if (channels > 2) channels = 2;  // Limit to stereo processing
        uint32_t samples = buffer.getSampleCount();
        
        // Apply input gain
        if (inputGain != 0.0) {
            float inputGainLinear = static_cast<float>(std::pow(10.0, inputGain / 20.0));
            for (uint32_t ch = 0; ch < channels; ++ch) {
                float* channelData = buffer.getChannelData(ch);
                for (uint32_t i = 0; i < samples; ++i) {
                    channelData[i] *= inputGainLinear;
                }
            }
        }
        
        // Process with or without oversampling
        if (oversamplingFactor > 1 && ispDetection) {
            processWithOversampling(buffer, channels, samples);
        } else {
            processDirectly(buffer, channels, samples);
        }
        
        // Apply output gain
        if (outputGain != 0.0) {
            float outputGainLinear = static_cast<float>(std::pow(10.0, outputGain / 20.0));
            for (uint32_t ch = 0; ch < channels; ++ch) {
                float* channelData = buffer.getChannelData(ch);
                for (uint32_t i = 0; i < samples; ++i) {
                    channelData[i] *= outputGainLinear;
                }
            }
        }
        
        // Update metering
        updateMetering(buffer, channels, samples);
    }
    
    void processDirectly(dawg::core::AudioBuffer& buffer, uint32_t channels, uint32_t samples) {
        for (uint32_t i = 0; i < samples; ++i) {
            // Get peak level across all channels
            float peakLevel = 0.0f;
            for (uint32_t ch = 0; ch < channels; ++ch) {
                float sample = buffer.getChannelData(ch)[i];
                peakLevel = std::max(peakLevel, std::abs(sample));
            }
            
            // Look-ahead: get the delayed samples for output
            std::vector<float> outputSamples(channels);
            for (uint32_t ch = 0; ch < channels; ++ch) {
                // Store current sample in delay line
                delayLine[ch][delayIndex[ch]] = buffer.getChannelData(ch)[i];
                
                // Get delayed sample for output
                size_t readIndex = (delayIndex[ch] + 1) % lookAheadSamples;
                outputSamples[ch] = delayLine[ch][readIndex];
                
                // Advance delay index
                delayIndex[ch] = (delayIndex[ch] + 1) % lookAheadSamples;
            }
            
            // Calculate required gain reduction
            double ceilingLinear = std::pow(10.0, ceiling / 20.0);
            double requiredGR = 0.0;
            
            if (peakLevel > ceilingLinear) {
                requiredGR = 20.0 * std::log10(ceilingLinear / peakLevel);
            }
            
            // Update target gain reduction
            targetGainReduction = requiredGR;
            
            // Smooth gain reduction with attack/release
            if (targetGainReduction < currentGainReduction) {
                // Attack (gain reduction increasing = level decreasing)
                currentGainReduction += (targetGainReduction - currentGainReduction) * attackCoeff;
            } else {
                // Release (gain reduction decreasing = level increasing)
                currentGainReduction += (targetGainReduction - currentGainReduction) * releaseCoeff;
            }
            
            // Apply gain reduction
            float grLinear = static_cast<float>(std::pow(10.0, currentGainReduction / 20.0));
            
            for (uint32_t ch = 0; ch < channels; ++ch) {
                buffer.getChannelData(ch)[i] = outputSamples[ch] * grLinear;
            }
            
            // Update adaptive parameters
            updateAdaptiveParameters(peakLevel);
        }
        
        // Update timing coefficients if needed
        if (releaseMode != ReleaseMode::Manual) {
            updateTimingCoefficients();
        }
    }
    
    void processWithOversampling(dawg::core::AudioBuffer& buffer, uint32_t channels, uint32_t samples) {
        // For this implementation, we'll use a simplified oversampling approach
        // In production, would use proper polyphase filters
        
        // Resize oversampled buffers
        size_t oversampledLength = samples * oversamplingFactor;
        for (uint32_t ch = 0; ch < channels; ++ch) {
            oversampledBuffer[ch].resize(oversampledLength);
        }
        
        // Upsample by zero-insertion and filtering
        for (uint32_t ch = 0; ch < channels; ++ch) {
            float* channelData = buffer.getChannelData(ch);
            
            // Zero-insert
            for (size_t i = 0; i < oversampledLength; ++i) {
                if (i % oversamplingFactor == 0) {
                    oversampledBuffer[ch][i] = channelData[i / oversamplingFactor] * oversamplingFactor;
                } else {
                    oversampledBuffer[ch][i] = 0.0f;
                }
            }
            
            // Simple filtering (would use proper polyphase in production)
            // Apply a simple moving average for anti-aliasing
            for (size_t i = 1; i < oversampledLength - 1; ++i) {
                oversampledBuffer[ch][i] = (oversampledBuffer[ch][i-1] + 
                                           oversampledBuffer[ch][i] + 
                                           oversampledBuffer[ch][i+1]) / 3.0f;
            }
        }
        
        // Process at oversampled rate
        processOversampledData(oversampledLength, channels);
        
        // Downsample back to original rate
        for (uint32_t ch = 0; ch < channels; ++ch) {
            float* channelData = buffer.getChannelData(ch);
            for (uint32_t i = 0; i < samples; ++i) {
                channelData[i] = oversampledBuffer[ch][i * oversamplingFactor];
            }
        }
    }
    
    void processOversampledData(size_t samples, uint32_t channels) {
        // Process the oversampled data with the limiter
        for (size_t i = 0; i < samples; ++i) {
            // Find peak across channels
            float peakLevel = 0.0f;
            for (uint32_t ch = 0; ch < channels; ++ch) {
                peakLevel = std::max(peakLevel, std::abs(oversampledBuffer[ch][i]));
            }
            
            // Apply limiting (simplified for oversampled processing)
            double ceilingLinear = std::pow(10.0, ceiling / 20.0);
            if (peakLevel > ceilingLinear) {
                float reductionFactor = static_cast<float>(ceilingLinear / peakLevel);
                for (uint32_t ch = 0; ch < channels; ++ch) {
                    oversampledBuffer[ch][i] *= reductionFactor;
                }
            }
        }
    }
    
    void updateAdaptiveParameters(float peakLevel) {
        // Update signal analysis for adaptive release
        signalEnergy = signalEnergy * 0.999 + peakLevel * peakLevel * 0.001;
        
        // Simple transient detection
        float instantEnergy = peakLevel * peakLevel;
        if (instantEnergy > signalEnergy * 4.0) {
            transientDetector = std::min(1.0, transientDetector + 0.1);
        } else {
            transientDetector = std::max(0.0, transientDetector - 0.001);
        }
        
        // Update adaptive release time
        if (releaseMode == ReleaseMode::Adaptive) {
            // Faster release for steady-state, slower for transient material
            double baseRelease = 30.0;
            double dynamicRange = std::max(0.1, signalEnergy);
            adaptiveRelease = baseRelease * (0.5 + 0.5 / dynamicRange);
            adaptiveRelease = std::max(10.0, std::min(200.0, adaptiveRelease));
        }
    }
    
    void updateMetering(dawg::core::AudioBuffer& buffer, uint32_t channels, uint32_t samples) {
        // Update peak metering
        float bufferPeak = 0.0f;
        double bufferRMS = 0.0;
        
        for (uint32_t ch = 0; ch < channels; ++ch) {
            float* channelData = buffer.getChannelData(ch);
            for (uint32_t i = 0; i < samples; ++i) {
                float sample = std::abs(channelData[i]);
                bufferPeak = std::max(bufferPeak, sample);
                bufferRMS += sample * sample;
            }
        }
        
        // Update true peak level
        if (bufferPeak > 0.0f) {
            double peakdB = 20.0 * std::log10(bufferPeak);
            truePeakLevel = std::max(truePeakLevel * 0.95, peakdB);
        } else {
            truePeakLevel *= 0.95; // Decay
        }
        
        // Update RMS level
        bufferRMS = std::sqrt(bufferRMS / (samples * channels));
        if (bufferRMS > 0.0) {
            double rmsdB = 20.0 * std::log10(bufferRMS);
            rmsLevel = std::max(rmsLevel * 0.95, rmsdB);
        } else {
            rmsLevel *= 0.95; // Decay
        }
        
        // Update gain reduction meter
        gainReductionMeter = currentGainReduction;
        isCurrentlyLimiting = (std::abs(currentGainReduction) > 0.1);
    }
    
    void reset() {
        // Reset delay lines
        for (auto& delay : delayLine) {
            std::fill(delay.begin(), delay.end(), 0.0f);
        }
        std::fill(delayIndex.begin(), delayIndex.end(), 0);
        
        // Reset processing state
        envelope = 0.0;
        targetGainReduction = 0.0;
        currentGainReduction = 0.0;
        
        // Reset metering
        truePeakLevel = -96.0;
        rmsLevel = -96.0;
        gainReductionMeter = 0.0;
        isCurrentlyLimiting = false;
        
        // Reset analysis
        signalEnergy = 0.0;
        transientDetector = 0.0;
        analysisCounter = 0;
        adaptiveRelease = 50.0;
    }
};

// ========================================================================
// PUBLIC INTERFACE
// ========================================================================

Limiter::Limiter() : pImpl(std::make_unique<Impl>()) {}
Limiter::~Limiter() = default;

// ========================================================================
// CORE PROCESSING
// ========================================================================

void Limiter::process(dawg::core::AudioBuffer& buffer) {
    if (!isBypassed()) {
        pImpl->process(buffer);
    }
}

void Limiter::reset() {
    pImpl->reset();
}

void Limiter::setSampleRate(uint32_t sampleRate) {
    pImpl->initialize(sampleRate);
}

void Limiter::setBufferSize(uint32_t bufferSize) {
    // Buffer size doesn't affect limiter configuration
    (void)bufferSize; // Suppress unused parameter warning
}

// ========================================================================
// PARAMETER CONTROL
// ========================================================================

void Limiter::setParameter(const std::string& name, double value) {
    if (name == "ceiling") {
        pImpl->ceiling = std::max(-12.0, std::min(0.0, value));
    } else if (name == "release") {
        pImpl->release = std::max(1.0, std::min(1000.0, value));
    } else if (name == "lookAhead") {
        pImpl->lookAheadTime = std::max(0.0, std::min(10.0, value));
        pImpl->updateLookAheadBuffer();
    } else if (name == "inputGain") {
        pImpl->inputGain = std::max(-20.0, std::min(20.0, value));
    } else if (name == "outputGain") {
        pImpl->outputGain = std::max(-20.0, std::min(20.0, value));
    } else if (name == "oversampling") {
        uint32_t factor = static_cast<uint32_t>(value);
        if (factor == 1 || factor == 2 || factor == 4 || factor == 8) {
            pImpl->oversamplingFactor = factor;
            pImpl->initializeOversampling();
        }
    } else if (name == "ispDetection") {
        pImpl->ispDetection = (value > 0.5);
    } else if (name == "releaseMode") {
        int mode = static_cast<int>(value);
        if (mode >= 0 && mode <= 2) {
            pImpl->releaseMode = static_cast<ReleaseMode>(mode);
        }
    }
    
    // Update timing coefficients when relevant parameters change
    if (name == "release" || name == "releaseMode") {
        pImpl->updateTimingCoefficients();
    }
}

double Limiter::getParameter(const std::string& name) const {
    if (name == "ceiling") return pImpl->ceiling;
    if (name == "release") return pImpl->release;
    if (name == "lookAhead") return pImpl->lookAheadTime;
    if (name == "inputGain") return pImpl->inputGain;
    if (name == "outputGain") return pImpl->outputGain;
    if (name == "oversampling") return static_cast<double>(pImpl->oversamplingFactor);
    if (name == "ispDetection") return pImpl->ispDetection ? 1.0 : 0.0;
    if (name == "releaseMode") return static_cast<double>(pImpl->releaseMode);
    return 0.0;
}

std::vector<std::string> Limiter::getParameterNames() const {
    return {
        "ceiling", "release", "lookAhead", "inputGain", "outputGain",
        "oversampling", "ispDetection", "releaseMode"
    };
}

uint32_t Limiter::getLatency() const {
    return static_cast<uint32_t>(pImpl->lookAheadSamples);
}

// ========================================================================
// LIMITER-SPECIFIC CONTROL
// ========================================================================

void Limiter::setCeiling(double ceiling) {
    setParameter("ceiling", ceiling);
}

void Limiter::setRelease(double release) {
    setParameter("release", release);
}

void Limiter::setReleaseMode(ReleaseMode mode) {
    setParameter("releaseMode", static_cast<double>(mode));
}

void Limiter::setLookAhead(double time) {
    setParameter("lookAhead", time);
}

void Limiter::setOversampling(uint32_t factor) {
    setParameter("oversampling", static_cast<double>(factor));
}

void Limiter::setISPDetection(bool enabled) {
    setParameter("ispDetection", enabled ? 1.0 : 0.0);
}

void Limiter::setInputGain(double gain) {
    setParameter("inputGain", gain);
}

void Limiter::setOutputGain(double gain) {
    setParameter("outputGain", gain);
}

// ========================================================================
// METERING AND ANALYSIS
// ========================================================================

double Limiter::getGainReduction() const {
    return pImpl->gainReductionMeter;
}

double Limiter::getTruePeakLevel() const {
    return pImpl->truePeakLevel;
}

double Limiter::getRMSLevel() const {
    return pImpl->rmsLevel;
}

double Limiter::getEffectiveRelease() const {
    return pImpl->getEffectiveReleaseTime();
}

bool Limiter::isLimiting() const {
    return pImpl->isCurrentlyLimiting;
}

} // namespace dynamics
} // namespace dsp
} // namespace dawg
