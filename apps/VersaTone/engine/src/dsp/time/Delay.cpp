#include "../../../include/dawg/dsp/time/Delay.h"
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
namespace time {

// Professional Delay Implementation
class Delay::Impl {
public:
    // ========================================================================
    // DELAY PARAMETERS
    // ========================================================================
    
    double delayTime = 250.0;        // Delay time in ms
    double feedback = 0.3;           // Feedback amount (0.0 to 0.98)
    double mix = 0.3;                // Wet/dry mix (0.0 to 1.0)
    double inputGain = 0.0;          // Input gain in dB
    double outputGain = 0.0;         // Output gain in dB
    
    DelayMode mode = DelayMode::Stereo;
    bool tempoSync = false;
    double tempo = 120.0;            // BPM
    NoteValue noteValue = NoteValue::Quarter;
    
    // Filtering
    double lowCutFreq = 80.0;        // Hz
    double highCutFreq = 8000.0;     // Hz
    
    // Modulation
    double modDepth = 0.0;           // 0.0 to 1.0
    double modRate = 0.5;            // Hz
    double modPhase = 0.0;           // LFO phase
    
    // Advanced features
    double ducking = 0.0;            // Ducking amount
    double stereoWidth = 1.0;        // Ping-pong width
    bool freeze = false;             // Freeze mode
    
    uint32_t sampleRate = 48000;
    
    // ========================================================================
    // PROCESSING STATE
    // ========================================================================
    
    // Delay lines (stereo)
    std::vector<std::vector<float>> delayBuffer;
    std::vector<size_t> delayIndex;
    size_t maxDelayLength;
    size_t currentDelayLength;
    
    // Multi-tap configuration
    struct DelayTap {
        double timeMultiplier = 1.0;  // Relative to main delay time
        double level = 1.0;           // Tap level
        double pan = 0.0;             // Tap pan (-1 to +1)
        bool active = true;
    };
    
    uint32_t tapCount = 1;
    std::vector<DelayTap> taps;
    
    // Filtering state
    struct FilterState {
        double lowCutState = 0.0;
        double highCutState = 0.0;
        double lowCutCoeff = 0.0;
        double highCutCoeff = 0.0;
    };
    std::vector<FilterState> filterState;
    
    // Modulation LFO
    double lfoPhase = 0.0;
    double lfoIncrement = 0.0;
    
    // Ducking envelope follower
    double duckingEnvelope = 0.0;
    double duckingAttack = 0.0;
    double duckingRelease = 0.0;
    
    // Ping-pong state
    bool pingPongToggle = false;
    double pingPongCrossfeed = 0.0;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    Impl() {
        taps.resize(8);  // Support up to 8 taps
        initialize(48000);
    }
    
    void initialize(uint32_t newSampleRate) {
        sampleRate = newSampleRate;
        
        // Calculate maximum delay length (2 seconds)
        maxDelayLength = static_cast<size_t>(2.0 * sampleRate);
        
        // Initialize delay buffers (stereo)
        delayBuffer.resize(2);
        delayIndex.resize(2);
        filterState.resize(2);
        
        for (size_t ch = 0; ch < 2; ++ch) {
            delayBuffer[ch].resize(maxDelayLength, 0.0f);
            delayIndex[ch] = 0;
        }
        
        // Update all calculated parameters
        updateDelayLength();
        updateFilterCoefficients();
        updateModulationParameters();
        updateDuckingParameters();
        
        reset();
    }
    
    void updateDelayLength() {
        double effectiveTime = delayTime;
        
        // Apply tempo sync if enabled
        if (tempoSync) {
            effectiveTime = calculateSyncedTime();
        }
        
        // Convert to samples
        currentDelayLength = static_cast<size_t>(effectiveTime * sampleRate / 1000.0);
        currentDelayLength = std::max(size_t(1), std::min(currentDelayLength, maxDelayLength - 1));
    }
    
    double calculateSyncedTime() const {
        double beatTime = 60000.0 / tempo; // Beat time in ms
        
        switch (noteValue) {
            case NoteValue::Whole:      return beatTime * 4.0;
            case NoteValue::Half:       return beatTime * 2.0;
            case NoteValue::Quarter:    return beatTime;
            case NoteValue::Eighth:     return beatTime * 0.5;
            case NoteValue::Sixteenth:  return beatTime * 0.25;
            case NoteValue::Dotted4th:  return beatTime * 1.5;
            case NoteValue::Dotted8th:  return beatTime * 0.75;
            case NoteValue::Triplet4th: return beatTime * (2.0/3.0);
            case NoteValue::Triplet8th: return beatTime * (1.0/3.0);
            default:                    return beatTime;
        }
    }
    
    void updateFilterCoefficients() {
        // Simple first-order filters
        double nyquist = sampleRate * 0.5;
        
        // Low-cut (high-pass)
        double lowCutNorm = lowCutFreq / nyquist;
        lowCutNorm = std::max(0.001, std::min(0.499, lowCutNorm));
        double lowCutRC = 1.0 / (2.0 * M_PI * lowCutFreq);
        double lowCutDT = 1.0 / sampleRate;
        for (auto& state : filterState) {
            state.lowCutCoeff = lowCutDT / (lowCutRC + lowCutDT);
        }
        
        // High-cut (low-pass)
        double highCutNorm = highCutFreq / nyquist;
        highCutNorm = std::max(0.001, std::min(0.499, highCutNorm));
        double highCutRC = 1.0 / (2.0 * M_PI * highCutFreq);
        double highCutDT = 1.0 / sampleRate;
        for (auto& state : filterState) {
            state.highCutCoeff = highCutDT / (highCutRC + highCutDT);
        }
    }
    
    void updateModulationParameters() {
        lfoIncrement = 2.0 * M_PI * modRate / sampleRate;
    }
    
    void updateDuckingParameters() {
        // Fast attack, slower release for ducking
        duckingAttack = 1.0 - std::exp(-1.0 / (0.01 * sampleRate));  // 10ms attack
        duckingRelease = 1.0 - std::exp(-1.0 / (0.1 * sampleRate));   // 100ms release
    }
    
    // ========================================================================
    // MAIN PROCESSING
    // ========================================================================
    
    void process(dawg::core::AudioBuffer& buffer) {
        if (buffer.getChannelCount() == 0 || buffer.getSampleCount() == 0) return;
        
        uint32_t channels = buffer.getChannelCount();
        if (channels > 2) channels = 2;  // Limit to stereo
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
        
        // Process based on delay mode
        switch (mode) {
            case DelayMode::Mono:
                processMono(buffer, channels, samples);
                break;
            case DelayMode::Stereo:
                processStereo(buffer, channels, samples);
                break;
            case DelayMode::PingPong:
                processPingPong(buffer, channels, samples);
                break;
            case DelayMode::MultiTap:
                processMultiTap(buffer, channels, samples);
                break;
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
    }
    
    void processMono(dawg::core::AudioBuffer& buffer, uint32_t channels, uint32_t samples) {
        for (uint32_t i = 0; i < samples; ++i) {
            // Sum input to mono
            float input = buffer.getChannelData(0)[i];
            if (channels > 1) {
                input = (input + buffer.getChannelData(1)[i]) * 0.5f;
            }
            
            // Process delay
            float delayOutput = processDelaySample(input, 0);
            
            // Apply to all channels
            float wet = delayOutput * static_cast<float>(mix);
            float dry = input * (1.0f - static_cast<float>(mix));
            float output = dry + wet;
            
            for (uint32_t ch = 0; ch < channels; ++ch) {
                buffer.getChannelData(ch)[i] = output;
            }
        }
    }
    
    void processStereo(dawg::core::AudioBuffer& buffer, uint32_t channels, uint32_t samples) {
        for (uint32_t i = 0; i < samples; ++i) {
            for (uint32_t ch = 0; ch < channels; ++ch) {
                float input = buffer.getChannelData(ch)[i];
                float delayOutput = processDelaySample(input, ch);
                
                float wet = delayOutput * static_cast<float>(mix);
                float dry = input * (1.0f - static_cast<float>(mix));
                
                buffer.getChannelData(ch)[i] = dry + wet;
            }
            
            // Handle mono input to stereo
            if (channels == 1) {
                float input = buffer.getChannelData(0)[i];
                float delayOutput = processDelaySample(input, 1);
                float wet = delayOutput * static_cast<float>(mix);
                float dry = input * (1.0f - static_cast<float>(mix));
                // Don't write to channel 1 if buffer only has 1 channel
            }
        }
    }
    
    void processPingPong(dawg::core::AudioBuffer& buffer, uint32_t channels, uint32_t samples) {
        for (uint32_t i = 0; i < samples; ++i) {
            float inputL = buffer.getChannelData(0)[i];
            float inputR = (channels > 1) ? buffer.getChannelData(1)[i] : inputL;
            
            // Sum to mono for ping-pong input
            float monoInput = (inputL + inputR) * 0.5f;
            
            // Process ping-pong delay
            float delayL = processDelaySample(monoInput, 0);
            float delayR = processDelaySample(monoInput, 1);
            
            // Cross-feed for ping-pong effect
            float crossfeedAmount = static_cast<float>(stereoWidth * 0.5);
            float pingPongL = delayL + delayR * crossfeedAmount;
            float pingPongR = delayR + delayL * crossfeedAmount;
            
            // Apply mix
            float wetL = pingPongL * static_cast<float>(mix);
            float wetR = pingPongR * static_cast<float>(mix);
            float dryAmount = 1.0f - static_cast<float>(mix);
            
            buffer.getChannelData(0)[i] = inputL * dryAmount + wetL;
            if (channels > 1) {
                buffer.getChannelData(1)[i] = inputR * dryAmount + wetR;
            }
        }
    }
    
    void processMultiTap(dawg::core::AudioBuffer& buffer, uint32_t channels, uint32_t samples) {
        for (uint32_t i = 0; i < samples; ++i) {
            for (uint32_t ch = 0; ch < channels; ++ch) {
                float input = buffer.getChannelData(ch)[i];
                float tapSum = 0.0f;
                
                // Process all active taps
                for (uint32_t tap = 0; tap < tapCount && tap < taps.size(); ++tap) {
                    if (taps[tap].active) {
                        float tapOutput = processMultiTapSample(input, ch, tap);
                        
                        // Apply tap panning (simple amplitude panning)
                        float tapGain = static_cast<float>(taps[tap].level);
                        if (ch == 0) {
                            // Left channel
                            tapGain *= (1.0f - std::max(0.0f, static_cast<float>(taps[tap].pan)));
                        } else {
                            // Right channel
                            tapGain *= (1.0f + std::min(0.0f, static_cast<float>(taps[tap].pan)));
                        }
                        
                        tapSum += tapOutput * tapGain;
                    }
                }
                
                float wet = tapSum * static_cast<float>(mix);
                float dry = input * (1.0f - static_cast<float>(mix));
                
                buffer.getChannelData(ch)[i] = dry + wet;
            }
        }
    }
    
    float processDelaySample(float input, uint32_t channel) {
        // Calculate modulated delay time
        double modAmount = modDepth * std::sin(lfoPhase) * 0.1; // Up to 10% modulation
        size_t modDelayLength = static_cast<size_t>(currentDelayLength * (1.0 + modAmount));
        modDelayLength = std::max(size_t(1), std::min(modDelayLength, maxDelayLength - 1));
        
        // Read from delay buffer
        size_t readIndex = (delayIndex[channel] + delayBuffer[channel].size() - modDelayLength) % delayBuffer[channel].size();
        float delayOutput = delayBuffer[channel][readIndex];
        
        // Apply filtering
        delayOutput = applyFiltering(delayOutput, channel);
        
        // Apply ducking
        float inputLevel = std::abs(input);
        if (inputLevel > duckingEnvelope) {
            duckingEnvelope += (inputLevel - duckingEnvelope) * duckingAttack;
        } else {
            duckingEnvelope += (inputLevel - duckingEnvelope) * duckingRelease;
        }
        
        float duckingGain = 1.0f - static_cast<float>(ducking * duckingEnvelope);
        delayOutput *= duckingGain;
        
        // Calculate feedback
        float feedbackAmount = freeze ? 0.99f : static_cast<float>(feedback);
        float feedbackSignal = delayOutput * feedbackAmount + input;
        
        // Write to delay buffer
        delayBuffer[channel][delayIndex[channel]] = feedbackSignal;
        delayIndex[channel] = (delayIndex[channel] + 1) % delayBuffer[channel].size();
        
        // Update LFO
        lfoPhase += lfoIncrement;
        if (lfoPhase >= 2.0 * M_PI) lfoPhase -= 2.0 * M_PI;
        
        return delayOutput;
    }
    
    float processMultiTapSample(float input, uint32_t channel, uint32_t tapIndex) {
        const DelayTap& tap = taps[tapIndex];
        
        // Calculate tap delay length
        size_t tapDelayLength = static_cast<size_t>(currentDelayLength * tap.timeMultiplier);
        tapDelayLength = std::max(size_t(1), std::min(tapDelayLength, maxDelayLength - 1));
        
        // Read from delay buffer
        size_t readIndex = (delayIndex[channel] + delayBuffer[channel].size() - tapDelayLength) % delayBuffer[channel].size();
        float tapOutput = delayBuffer[channel][readIndex];
        
        // Apply filtering
        tapOutput = applyFiltering(tapOutput, channel);
        
        return tapOutput;
    }
    
    float applyFiltering(float sample, uint32_t channel) {
        FilterState& state = filterState[channel];
        
        // High-pass filter (low-cut)
        state.lowCutState += (sample - state.lowCutState) * state.lowCutCoeff;
        float highPassed = sample - state.lowCutState;
        
        // Low-pass filter (high-cut)
        state.highCutState += (highPassed - state.highCutState) * state.highCutCoeff;
        
        return static_cast<float>(state.highCutState);
    }
    
    void reset() {
        // Clear delay buffers
        for (auto& buffer : delayBuffer) {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
        }
        std::fill(delayIndex.begin(), delayIndex.end(), 0);
        
        // Reset filter states
        for (auto& state : filterState) {
            state.lowCutState = 0.0;
            state.highCutState = 0.0;
        }
        
        // Reset modulation
        lfoPhase = 0.0;
        
        // Reset ducking
        duckingEnvelope = 0.0;
        
        // Reset ping-pong
        pingPongToggle = false;
        pingPongCrossfeed = 0.0;
    }
};

// ========================================================================
// PUBLIC INTERFACE
// ========================================================================

Delay::Delay() : pImpl(std::make_unique<Impl>()) {}
Delay::~Delay() = default;

// ========================================================================
// CORE PROCESSING
// ========================================================================

void Delay::process(dawg::core::AudioBuffer& buffer) {
    if (!isBypassed()) {
        pImpl->process(buffer);
    }
}

void Delay::reset() {
    pImpl->reset();
}

void Delay::setSampleRate(uint32_t sampleRate) {
    pImpl->initialize(sampleRate);
}

void Delay::setBufferSize(uint32_t bufferSize) {
    // Buffer size doesn't affect delay configuration
    (void)bufferSize; // Suppress unused parameter warning
}

// ========================================================================
// PARAMETER CONTROL
// ========================================================================

void Delay::setParameter(const std::string& name, double value) {
    if (name == "delayTime") {
        pImpl->delayTime = std::max(1.0, std::min(2000.0, value));
        pImpl->updateDelayLength();
    } else if (name == "feedback") {
        pImpl->feedback = std::max(0.0, std::min(0.98, value));
    } else if (name == "mix") {
        pImpl->mix = std::max(0.0, std::min(1.0, value));
    } else if (name == "lowCut") {
        pImpl->lowCutFreq = std::max(20.0, std::min(2000.0, value));
        pImpl->updateFilterCoefficients();
    } else if (name == "highCut") {
        pImpl->highCutFreq = std::max(1000.0, std::min(20000.0, value));
        pImpl->updateFilterCoefficients();
    } else if (name == "modDepth") {
        pImpl->modDepth = std::max(0.0, std::min(1.0, value));
    } else if (name == "modRate") {
        pImpl->modRate = std::max(0.1, std::min(10.0, value));
        pImpl->updateModulationParameters();
    } else if (name == "ducking") {
        pImpl->ducking = std::max(0.0, std::min(1.0, value));
    } else if (name == "stereoWidth") {
        pImpl->stereoWidth = std::max(0.0, std::min(1.0, value));
    } else if (name == "inputGain") {
        pImpl->inputGain = std::max(-20.0, std::min(20.0, value));
    } else if (name == "outputGain") {
        pImpl->outputGain = std::max(-20.0, std::min(20.0, value));
    } else if (name == "tempo") {
        pImpl->tempo = std::max(60.0, std::min(200.0, value));
        if (pImpl->tempoSync) pImpl->updateDelayLength();
    } else if (name == "tempoSync") {
        pImpl->tempoSync = (value > 0.5);
        pImpl->updateDelayLength();
    } else if (name == "noteValue") {
        int noteVal = static_cast<int>(value);
        if (noteVal >= 0 && noteVal <= 8) {
            pImpl->noteValue = static_cast<NoteValue>(noteVal);
            if (pImpl->tempoSync) pImpl->updateDelayLength();
        }
    } else if (name == "mode") {
        int modeVal = static_cast<int>(value);
        if (modeVal >= 0 && modeVal <= 3) {
            pImpl->mode = static_cast<DelayMode>(modeVal);
        }
    } else if (name == "freeze") {
        pImpl->freeze = (value > 0.5);
    } else if (name == "tapCount") {
        pImpl->tapCount = std::max(1u, std::min(8u, static_cast<uint32_t>(value)));
    }
}

double Delay::getParameter(const std::string& name) const {
    if (name == "delayTime") return pImpl->delayTime;
    if (name == "feedback") return pImpl->feedback;
    if (name == "mix") return pImpl->mix;
    if (name == "lowCut") return pImpl->lowCutFreq;
    if (name == "highCut") return pImpl->highCutFreq;
    if (name == "modDepth") return pImpl->modDepth;
    if (name == "modRate") return pImpl->modRate;
    if (name == "ducking") return pImpl->ducking;
    if (name == "stereoWidth") return pImpl->stereoWidth;
    if (name == "inputGain") return pImpl->inputGain;
    if (name == "outputGain") return pImpl->outputGain;
    if (name == "tempo") return pImpl->tempo;
    if (name == "tempoSync") return pImpl->tempoSync ? 1.0 : 0.0;
    if (name == "noteValue") return static_cast<double>(pImpl->noteValue);
    if (name == "mode") return static_cast<double>(pImpl->mode);
    if (name == "freeze") return pImpl->freeze ? 1.0 : 0.0;
    if (name == "tapCount") return static_cast<double>(pImpl->tapCount);
    return 0.0;
}

std::vector<std::string> Delay::getParameterNames() const {
    return {
        "delayTime", "feedback", "mix", "lowCut", "highCut",
        "modDepth", "modRate", "ducking", "stereoWidth",
        "inputGain", "outputGain", "tempo", "tempoSync", "noteValue",
        "mode", "freeze", "tapCount"
    };
}

uint32_t Delay::getLatency() const {
    return 0; // Delay doesn't add processing latency (the delay itself is the effect)
}

// ========================================================================
// DELAY-SPECIFIC CONTROL
// ========================================================================

void Delay::setDelayTime(double time) {
    setParameter("delayTime", time);
}

void Delay::setFeedback(double feedback) {
    setParameter("feedback", feedback);
}

void Delay::setMix(double mix) {
    setParameter("mix", mix);
}

void Delay::setDelayMode(DelayMode mode) {
    setParameter("mode", static_cast<double>(mode));
}

void Delay::setTempoSync(bool sync) {
    setParameter("tempoSync", sync ? 1.0 : 0.0);
}

void Delay::setTempo(double bpm) {
    setParameter("tempo", bpm);
}

void Delay::setNoteValue(NoteValue noteValue) {
    setParameter("noteValue", static_cast<double>(noteValue));
}

void Delay::setLowCut(double frequency) {
    setParameter("lowCut", frequency);
}

void Delay::setHighCut(double frequency) {
    setParameter("highCut", frequency);
}

void Delay::setModulationDepth(double depth) {
    setParameter("modDepth", depth);
}

void Delay::setModulationRate(double rate) {
    setParameter("modRate", rate);
}

void Delay::setDucking(double amount) {
    setParameter("ducking", amount);
}

void Delay::setStereoWidth(double width) {
    setParameter("stereoWidth", width);
}

void Delay::setFreeze(bool freeze) {
    setParameter("freeze", freeze ? 1.0 : 0.0);
}

void Delay::setInputGain(double gain) {
    setParameter("inputGain", gain);
}

void Delay::setOutputGain(double gain) {
    setParameter("outputGain", gain);
}

// ========================================================================
// MULTI-TAP CONTROL
// ========================================================================

void Delay::setTapCount(uint32_t taps) {
    setParameter("tapCount", static_cast<double>(taps));
}

void Delay::setTapTime(uint32_t tapIndex, double time) {
    if (tapIndex < pImpl->taps.size()) {
        pImpl->taps[tapIndex].timeMultiplier = std::max(0.1, std::min(2.0, time));
    }
}

void Delay::setTapLevel(uint32_t tapIndex, double level) {
    if (tapIndex < pImpl->taps.size()) {
        pImpl->taps[tapIndex].level = std::max(0.0, std::min(1.0, level));
    }
}

void Delay::setTapPan(uint32_t tapIndex, double pan) {
    if (tapIndex < pImpl->taps.size()) {
        pImpl->taps[tapIndex].pan = std::max(-1.0, std::min(1.0, pan));
    }
}

// ========================================================================
// METERING AND ANALYSIS
// ========================================================================

double Delay::getEffectiveDelayTime() const {
    if (pImpl->tempoSync) {
        return pImpl->calculateSyncedTime();
    }
    return pImpl->delayTime;
}

double Delay::getBufferFill() const {
    return static_cast<double>(pImpl->currentDelayLength) / pImpl->maxDelayLength;
}

bool Delay::isActive() const {
    // Check if any delay buffer has non-zero content
    for (const auto& buffer : pImpl->delayBuffer) {
        for (float sample : buffer) {
            if (std::abs(sample) > 0.001f) return true;
        }
    }
    return false;
}

} // namespace time
} // namespace dsp
} // namespace dawg
