#include "../../../include/dawg/dsp/dynamics/Compressor.h"
#include "../../../include/dawg/core/AudioBuffer.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace dawg {
namespace dsp {
namespace dynamics {

// Compressor Private Implementation (pImpl pattern)
class Compressor::Impl {
public:
    // Compressor parameters
    double threshold = 0.7;        // Linear threshold (default ~-3dB)
    double ratio = 4.0;            // Compression ratio
    double makeupGain = 1.0;       // Linear makeup gain
    
    // Compression state
    double gainReduction = 0.0;    // Current gain reduction
    
    void process(dawg::core::AudioBuffer& buffer) {
        // Basic threshold-based gain reduction
        for (uint32_t channel = 0; channel < buffer.getChannelCount(); ++channel) {
            float* samples = buffer.getChannelData(channel);
            for (uint32_t sample = 0; sample < buffer.getSampleCount(); ++sample) {
                float input = std::abs(samples[sample]);
                if (input > threshold) {
                    // Simple gain reduction with ratio
                    float excess = input - threshold;
                    float reduction = excess / ratio;
                    samples[sample] *= (threshold + reduction) / input;
                }
                // Apply makeup gain
                samples[sample] *= makeupGain;
            }
        }
    }
};

// ========================================================================
// CONSTRUCTION/DESTRUCTION
// ========================================================================

Compressor::Compressor() : pImpl(std::make_unique<Impl>()) {}
Compressor::~Compressor() = default;

// ========================================================================
// CORE PROCESSING
// ========================================================================

void Compressor::process(dawg::core::AudioBuffer& buffer) {
    if (!isBypassed()) {
        pImpl->process(buffer);
    }
}

void Compressor::reset() {
    pImpl->gainReduction = 0.0;
}

// ========================================================================
// CONFIGURATION
// ========================================================================

void Compressor::setSampleRate(uint32_t sampleRate) {
    this->sampleRate = sampleRate;
}

void Compressor::setBufferSize(uint32_t bufferSize) {
    this->bufferSize = bufferSize;
}

// ========================================================================
// PARAMETER CONTROL
// ========================================================================

void Compressor::setParameter(const std::string& name, double value) {
    if (name == "threshold") {
        setThreshold(value);
    } else if (name == "ratio") {
        setRatio(value);
    } else if (name == "makeup") {
        setMakeupGain(value);
    }
}

double Compressor::getParameter(const std::string& name) const {
    if (name == "threshold") return 20.0 * std::log10(pImpl->threshold);  // Convert linear to dB
    if (name == "ratio") return pImpl->ratio;
    if (name == "makeup") return 20.0 * std::log10(pImpl->makeupGain);  // Convert linear to dB
    return 0.0;
}

std::vector<std::string> Compressor::getParameterNames() const {
    return {"threshold", "ratio", "makeup"};
}

uint32_t Compressor::getLatency() const {
    return 0; // No latency for basic compressor
}

// ========================================================================
// COMPRESSOR-SPECIFIC CONTROLS
// ========================================================================

void Compressor::setModel(CompressorModel model) {
    // TODO: Implement different compressor models
}

void Compressor::setThreshold(double threshold) {
    // Convert dB to linear and clamp to reasonable range
    double thresholdDb = (threshold < -60.0) ? -60.0 : ((threshold > 0.0) ? 0.0 : threshold);
    pImpl->threshold = std::pow(10.0, thresholdDb / 20.0);  // Convert dB to linear
}

void Compressor::setRatio(double ratio) {
    pImpl->ratio = (ratio < 1.0) ? 1.0 : ((ratio > 20.0) ? 20.0 : ratio);
}

void Compressor::setAttack(double attack) {
    // TODO: Implement attack time
}

void Compressor::setRelease(double release) {
    // TODO: Implement release time
}

void Compressor::setKnee(double knee) {
    // TODO: Implement knee parameter
}

void Compressor::setMakeupGain(double gain) {
    // Convert dB to linear and clamp to reasonable range
    double gainDb = (gain < -20.0) ? -20.0 : ((gain > 20.0) ? 20.0 : gain);
    pImpl->makeupGain = std::pow(10.0, gainDb / 20.0);  // Convert dB to linear
}

void Compressor::setAutoMakeup(bool enabled) {
    // TODO: Implement auto makeup gain
}

void Compressor::setLookAhead(double time) {
    // TODO: Implement look-ahead processing
}

void Compressor::setSideChainEnabled(bool enabled) {
    // TODO: Implement side-chain
}

void Compressor::processSideChain(const dawg::core::AudioBuffer& sideChain) {
    // TODO: Implement side-chain processing
}

void Compressor::setSideChainHighPass(double frequency) {
    // TODO: Implement side-chain high-pass filter
}

// ========================================================================
// METERING
// ========================================================================

double Compressor::getGainReduction() const {
    return pImpl->gainReduction;
}

double Compressor::getInputLevel() const {
    return 0.0; // TODO: Implement input level metering
}

double Compressor::getOutputLevel() const {
    return 0.0; // TODO: Implement output level metering
}

Compressor::CompressorModel Compressor::getModel() const {
    return CompressorModel::VCA; // TODO: Return actual model
}

bool Compressor::isCompressing() const {
    return false; // TODO: Implement compression detection
}

} // namespace dynamics
} // namespace dsp
} // namespace dawg
