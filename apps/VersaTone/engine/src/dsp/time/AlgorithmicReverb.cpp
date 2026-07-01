#include "../../../include/dawg/dsp/time/AlgorithmicReverb.h"
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

// Professional Algorithmic Reverb Implementation (Schroeder + Freeverb)
class AlgorithmicReverb::Impl {
public:
    // ========================================================================
    // REVERB PARAMETERS
    // ========================================================================
    
    double roomSize = 0.5;        // 0.0 to 1.0
    double damping = 0.5;         // High frequency damping (0.0 to 1.0)
    double earlyLevel = 0.3;      // Early reflections level
    double tailLevel = 0.7;       // Late reverb tail level
    double predelay = 0.0;        // Predelay in seconds (0.0 to 0.1)
    double width = 1.0;           // Stereo width (0.0 to 1.0)
    double wetLevel = 0.3;        // Wet/dry mix (0.0 to 1.0)
    bool freeze = false;          // Freeze mode
    
    ReverbType currentType = ReverbType::Hall;
    uint32_t sampleRate = 48000;
    
    // ========================================================================
    // COMB FILTERS (Late Reverb)
    // ========================================================================
    
    struct CombFilter {
        std::vector<float> buffer;
        size_t bufferSize;
        size_t index;
        float feedback;
        float filterStore;
        float damping1;
        float damping2;
        
        CombFilter() : bufferSize(0), index(0), feedback(0.5f), 
                      filterStore(0.0f), damping1(0.5f), damping2(0.5f) {}
        
        void setSize(size_t size) {
            bufferSize = size;
            buffer.assign(size, 0.0f);
            index = 0;
        }
        
        void setDamping(float damp) {
            damping1 = damp;
            damping2 = 1.0f - damp;
        }
        
        void setFeedback(float fb) {
            feedback = fb;
        }
        
        float process(float input) {
            float output = buffer[index];
            filterStore = (output * damping2) + (filterStore * damping1);
            buffer[index] = input + (filterStore * feedback);
            
            if (++index >= bufferSize) index = 0;
            return output;
        }
        
        void mute() {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
            filterStore = 0.0f;
        }
    };
    
    // ========================================================================
    // ALLPASS FILTERS (Diffusion)
    // ========================================================================
    
    struct AllpassFilter {
        std::vector<float> buffer;
        size_t bufferSize;
        size_t index;
        float feedback;
        
        AllpassFilter() : bufferSize(0), index(0), feedback(0.5f) {}
        
        void setSize(size_t size) {
            bufferSize = size;
            buffer.assign(size, 0.0f);
            index = 0;
        }
        
        void setFeedback(float fb) {
            feedback = fb;
        }
        
        float process(float input) {
            float buffOut = buffer[index];
            float output = -input + buffOut;
            buffer[index] = input + (buffOut * feedback);
            
            if (++index >= bufferSize) index = 0;
            return output;
        }
        
        void mute() {
            std::fill(buffer.begin(), buffer.end(), 0.0f);
        }
    };
    
    // ========================================================================
    // EARLY REFLECTIONS
    // ========================================================================
    
    struct EarlyReflections {
        struct Tap {
            size_t delay;
            float gain;
            float pan;  // -1.0 = left, 1.0 = right
        };
        
        std::vector<float> delayLine;
        size_t writeIndex;
        std::vector<Tap> taps;
        
        EarlyReflections() : writeIndex(0) {}
        
        void initialize(uint32_t sampleRate, EarlyReflectionPattern pattern) {
            size_t maxDelay = static_cast<size_t>(0.1 * sampleRate); // 100ms max
            delayLine.assign(maxDelay, 0.0f);
            writeIndex = 0;
            
            // Define reflection patterns based on room type
            taps.clear();
            switch (pattern) {
                case EarlyReflectionPattern::Hall:
                    taps = {
                        {static_cast<size_t>(0.015 * sampleRate), 0.8f, -0.3f},
                        {static_cast<size_t>(0.025 * sampleRate), 0.6f, 0.5f},
                        {static_cast<size_t>(0.035 * sampleRate), 0.4f, -0.7f},
                        {static_cast<size_t>(0.045 * sampleRate), 0.3f, 0.2f},
                        {static_cast<size_t>(0.055 * sampleRate), 0.2f, -0.1f},
                        {static_cast<size_t>(0.065 * sampleRate), 0.15f, 0.6f}
                    };
                    break;
                case EarlyReflectionPattern::Room:
                    taps = {
                        {static_cast<size_t>(0.008 * sampleRate), 0.7f, -0.2f},
                        {static_cast<size_t>(0.012 * sampleRate), 0.5f, 0.4f},
                        {static_cast<size_t>(0.018 * sampleRate), 0.3f, -0.6f},
                        {static_cast<size_t>(0.025 * sampleRate), 0.2f, 0.3f}
                    };
                    break;
                default: // Studio
                    taps = {
                        {static_cast<size_t>(0.005 * sampleRate), 0.6f, -0.1f},
                        {static_cast<size_t>(0.008 * sampleRate), 0.4f, 0.2f},
                        {static_cast<size_t>(0.012 * sampleRate), 0.3f, -0.3f}
                    };
                    break;
            }
        }
        
        void process(float input, float& leftOut, float& rightOut) {
            delayLine[writeIndex] = input;
            
            leftOut = rightOut = 0.0f;
            
            for (const auto& tap : taps) {
                size_t readIndex = (writeIndex + delayLine.size() - tap.delay) % delayLine.size();
                float tapOutput = delayLine[readIndex] * tap.gain;
                
                if (tap.pan <= 0.0f) {
                    leftOut += tapOutput * (1.0f + tap.pan);
                    rightOut += tapOutput * (1.0f - std::abs(tap.pan));
                } else {
                    leftOut += tapOutput * (1.0f - tap.pan);
                    rightOut += tapOutput * (1.0f + tap.pan);
                }
            }
            
            writeIndex = (writeIndex + 1) % delayLine.size();
        }
        
        void mute() {
            std::fill(delayLine.begin(), delayLine.end(), 0.0f);
            writeIndex = 0;
        }
    };
    
    // ========================================================================
    // STEREO CHANNEL PROCESSOR
    // ========================================================================
    
    struct ChannelProcessor {
        std::vector<CombFilter> combFilters;
        std::vector<AllpassFilter> allpassFilters;
        
        ChannelProcessor() {
            combFilters.resize(8);     // 8 parallel comb filters
            allpassFilters.resize(4);  // 4 series allpass filters
        }
        
        void initialize(uint32_t sampleRate, bool isRightChannel = false) {
            // Comb filter delay times (in samples)
            std::vector<size_t> combDelays = {
                static_cast<size_t>(0.025 * sampleRate),
                static_cast<size_t>(0.027 * sampleRate),
                static_cast<size_t>(0.029 * sampleRate),
                static_cast<size_t>(0.031 * sampleRate),
                static_cast<size_t>(0.033 * sampleRate),
                static_cast<size_t>(0.035 * sampleRate),
                static_cast<size_t>(0.037 * sampleRate),
                static_cast<size_t>(0.039 * sampleRate)
            };
            
            // Allpass filter delay times
            std::vector<size_t> allpassDelays = {
                static_cast<size_t>(0.005 * sampleRate),
                static_cast<size_t>(0.0125 * sampleRate),
                static_cast<size_t>(0.01 * sampleRate),
                static_cast<size_t>(0.015 * sampleRate)
            };
            
            // Slightly detune right channel for stereo width
            if (isRightChannel) {
                for (auto& delay : combDelays) delay += 23;      // Prime number offset
                for (auto& delay : allpassDelays) delay += 11;   // Different prime offset
            }
            
            // Initialize comb filters
            for (size_t i = 0; i < combFilters.size(); ++i) {
                combFilters[i].setSize(combDelays[i]);
                combFilters[i].setFeedback(0.84f);
                combFilters[i].setDamping(0.2f);
            }
            
            // Initialize allpass filters
            for (size_t i = 0; i < allpassFilters.size(); ++i) {
                allpassFilters[i].setSize(allpassDelays[i]);
                allpassFilters[i].setFeedback(0.5f);
            }
        }
        
        float process(float input) {
            // Process through parallel comb filters
            float combSum = 0.0f;
            for (auto& comb : combFilters) {
                combSum += comb.process(input);
            }
            
            // Process through series allpass filters (diffusion)
            float output = combSum;
            for (auto& allpass : allpassFilters) {
                output = allpass.process(output);
            }
            
            return output;
        }
        
        void updateParameters(double roomSize, double damping, bool freeze) {
            // Update comb filter parameters
            float feedback = freeze ? 1.0f : (0.28f + 0.7f * static_cast<float>(roomSize));
            
            for (auto& comb : combFilters) {
                comb.setFeedback(feedback);
                comb.setDamping(static_cast<float>(damping));
            }
        }
        
        void mute() {
            for (auto& comb : combFilters) comb.mute();
            for (auto& allpass : allpassFilters) allpass.mute();
        }
    };
    
    // ========================================================================
    // PREDELAY
    // ========================================================================
    
    std::vector<float> predelayBuffer;
    size_t predelayIndex;
    size_t predelaySize;
    
    // ========================================================================
    // MAIN PROCESSOR COMPONENTS
    // ========================================================================
    
    EarlyReflections earlyReflections;
    ChannelProcessor leftProcessor;
    ChannelProcessor rightProcessor;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    Impl() : predelayIndex(0), predelaySize(0) {
        initialize(48000);
    }
    
    void initialize(uint32_t newSampleRate) {
        sampleRate = newSampleRate;
        
        // Initialize predelay buffer (up to 100ms)
        size_t maxPredelay = static_cast<size_t>(0.1 * sampleRate);
        predelayBuffer.assign(maxPredelay, 0.0f);
        predelayIndex = 0;
        updatePredelaySize();
        
        // Initialize early reflections
        earlyReflections.initialize(sampleRate, EarlyReflectionPattern::Hall);
        
        // Initialize stereo processors
        leftProcessor.initialize(sampleRate, false);
        rightProcessor.initialize(sampleRate, true);
        
        updateAllParameters();
    }
    
    void updatePredelaySize() {
        predelaySize = static_cast<size_t>(predelay * sampleRate);
        if (predelaySize >= predelayBuffer.size()) {
            predelaySize = predelayBuffer.size() - 1;
        }
    }
    
    void updateAllParameters() {
        leftProcessor.updateParameters(roomSize, damping, freeze);
        rightProcessor.updateParameters(roomSize, damping, freeze);
    }
    
    // ========================================================================
    // MAIN PROCESSING
    // ========================================================================
    
    void process(dawg::core::AudioBuffer& buffer) {
        if (buffer.getChannelCount() == 0 || buffer.getSampleCount() == 0) return;
        
        // Process stereo (convert mono to stereo if needed)
        uint32_t channels = (buffer.getChannelCount() < 2u) ? buffer.getChannelCount() : 2u;
        
        for (uint32_t sample = 0; sample < buffer.getSampleCount(); ++sample) {
            // Get input (mix to mono for reverb input)
            float input = buffer.getChannelData(0)[sample];
            if (channels > 1) {
                input = (input + buffer.getChannelData(1)[sample]) * 0.5f;
            }
            
            // Predelay
            float delayedInput = input;
            if (predelaySize > 0) {
                size_t readIndex = (predelayIndex + predelayBuffer.size() - predelaySize) % predelayBuffer.size();
                delayedInput = predelayBuffer[readIndex];
                predelayBuffer[predelayIndex] = input;
                predelayIndex = (predelayIndex + 1) % predelayBuffer.size();
            }
            
            // Early reflections
            float earlyLeft, earlyRight;
            earlyReflections.process(delayedInput, earlyLeft, earlyRight);
            
            // Late reverb processing
            float lateLeft = leftProcessor.process(delayedInput);
            float lateRight = rightProcessor.process(delayedInput);
            
            // Combine early + late reflections
            float reverbLeft = earlyLeft * static_cast<float>(earlyLevel) + 
                              lateLeft * static_cast<float>(tailLevel);
            float reverbRight = earlyRight * static_cast<float>(earlyLevel) + 
                               lateRight * static_cast<float>(tailLevel);
            
            // Apply stereo width
            float mid = (reverbLeft + reverbRight) * 0.5f;
            float side = (reverbLeft - reverbRight) * 0.5f * static_cast<float>(width);
            reverbLeft = mid + side;
            reverbRight = mid - side;
            
            // Wet/dry mix and output
            float dryLevel = 1.0f - static_cast<float>(wetLevel);
            float wetGain = static_cast<float>(wetLevel);
            
            buffer.getChannelData(0)[sample] = 
                buffer.getChannelData(0)[sample] * dryLevel + reverbLeft * wetGain;
            
            if (channels > 1) {
                buffer.getChannelData(1)[sample] = 
                    buffer.getChannelData(1)[sample] * dryLevel + reverbRight * wetGain;
            }
        }
    }
    
    void reset() {
        earlyReflections.mute();
        leftProcessor.mute();
        rightProcessor.mute();
        std::fill(predelayBuffer.begin(), predelayBuffer.end(), 0.0f);
        predelayIndex = 0;
    }
};

// ========================================================================
// CONSTRUCTION/DESTRUCTION
// ========================================================================

AlgorithmicReverb::AlgorithmicReverb() : pImpl(std::make_unique<Impl>()) {}
AlgorithmicReverb::~AlgorithmicReverb() = default;

// ========================================================================
// CORE PROCESSING
// ========================================================================

void AlgorithmicReverb::process(dawg::core::AudioBuffer& buffer) {
    if (!isBypassed()) {
        pImpl->process(buffer);
    }
}

void AlgorithmicReverb::reset() {
    pImpl->reset();
}

// ========================================================================
// CONFIGURATION
// ========================================================================

void AlgorithmicReverb::setSampleRate(uint32_t sampleRate) {
    this->sampleRate = sampleRate;
    pImpl->initialize(sampleRate);
}

void AlgorithmicReverb::setBufferSize(uint32_t bufferSize) {
    this->bufferSize = bufferSize;
}

// ========================================================================
// PARAMETER CONTROL
// ========================================================================

void AlgorithmicReverb::setParameter(const std::string& name, double value) {
    if (name == "roomSize") {
        pImpl->roomSize = std::max(0.0, std::min(1.0, value));
        pImpl->updateAllParameters();
    } else if (name == "damping") {
        pImpl->damping = std::max(0.0, std::min(1.0, value));
        pImpl->updateAllParameters();
    } else if (name == "earlyLevel") {
        pImpl->earlyLevel = std::max(0.0, std::min(1.0, value));
    } else if (name == "tailLevel") {
        pImpl->tailLevel = std::max(0.0, std::min(1.0, value));
    } else if (name == "predelay") {
        pImpl->predelay = std::max(0.0, std::min(0.1, value));
        pImpl->updatePredelaySize();
    } else if (name == "width") {
        pImpl->width = std::max(0.0, std::min(1.0, value));
    } else if (name == "wetLevel") {
        pImpl->wetLevel = std::max(0.0, std::min(1.0, value));
    } else if (name == "freeze") {
        pImpl->freeze = (value > 0.5);
        pImpl->updateAllParameters();
    }
}

double AlgorithmicReverb::getParameter(const std::string& name) const {
    if (name == "roomSize") return pImpl->roomSize;
    if (name == "damping") return pImpl->damping;
    if (name == "earlyLevel") return pImpl->earlyLevel;
    if (name == "tailLevel") return pImpl->tailLevel;
    if (name == "predelay") return pImpl->predelay;
    if (name == "width") return pImpl->width;
    if (name == "wetLevel") return pImpl->wetLevel;
    if (name == "freeze") return pImpl->freeze ? 1.0 : 0.0;
    return 0.0;
}

std::vector<std::string> AlgorithmicReverb::getParameterNames() const {
    return {"roomSize", "damping", "earlyLevel", "tailLevel", 
            "predelay", "width", "wetLevel", "freeze"};
}

uint32_t AlgorithmicReverb::getLatency() const {
    return static_cast<uint32_t>(pImpl->predelaySize);
}

// ========================================================================
// REVERB-SPECIFIC CONTROLS
// ========================================================================

void AlgorithmicReverb::setReverbType(ReverbType type) {
    pImpl->currentType = type;
    
    // Update early reflection pattern based on reverb type
    EarlyReflectionPattern pattern;
    switch (type) {
        case ReverbType::Hall:
            pattern = EarlyReflectionPattern::Hall;
            break;
        case ReverbType::Room:
            pattern = EarlyReflectionPattern::Room;
            break;
        case ReverbType::Chamber:
            pattern = EarlyReflectionPattern::Chamber;
            break;
        default:
            pattern = EarlyReflectionPattern::Studio;
            break;
    }
    
    pImpl->earlyReflections.initialize(pImpl->sampleRate, pattern);
}

AlgorithmicReverb::ReverbType AlgorithmicReverb::getReverbType() const {
    return pImpl->currentType;
}

void AlgorithmicReverb::setEarlyReflectionPattern(EarlyReflectionPattern pattern) {
    pImpl->earlyReflections.initialize(pImpl->sampleRate, pattern);
}

void AlgorithmicReverb::setRoomSize(double size) {
    setParameter("roomSize", size);
}

void AlgorithmicReverb::setDamping(double damping) {
    setParameter("damping", damping);
}

void AlgorithmicReverb::setDryWetMix(double mix) {
    setParameter("wetLevel", mix);
}

void AlgorithmicReverb::setPreDelay(double delay) {
    setParameter("predelay", delay);
}

void AlgorithmicReverb::setStereoWidth(double width) {
    setParameter("width", width);
}

void AlgorithmicReverb::setDecayTime(double time) {
    // Convert decay time to room size (simplified mapping)
    setParameter("roomSize", std::max(0.0, std::min(1.0, time / 10.0)));
}

void AlgorithmicReverb::setEarlyReflectionLevel(double level) {
    setParameter("earlyLevel", level);
}

void AlgorithmicReverb::setEarlyReflectionDecay(double decay) {
    setParameter("tailLevel", decay);
}

void AlgorithmicReverb::setModulation(double depth, double rate) {
    // Modulation not implemented in this basic version
    (void)depth; (void)rate; // Suppress unused parameter warnings
}

void AlgorithmicReverb::setChorusModulation(bool enabled) {
    // Chorus modulation not implemented in this basic version
    (void)enabled; // Suppress unused parameter warning
}

void AlgorithmicReverb::setFreeze(bool freeze) {
    setParameter("freeze", freeze ? 1.0 : 0.0);
}

void AlgorithmicReverb::setDiffusion(double diffusion) {
    // Diffusion control could map to allpass feedback, not implemented
    (void)diffusion; // Suppress unused parameter warning
}

void AlgorithmicReverb::setLowFrequencyDecay(double ratio) {
    // Low frequency decay not implemented in this basic version
    (void)ratio; // Suppress unused parameter warning
}

void AlgorithmicReverb::setHighFrequencyDecay(double ratio) {
    // High frequency decay could map to damping
    setParameter("damping", ratio);
}

void AlgorithmicReverb::setCrossoverFrequencies(double lowCrossover, double highCrossover) {
    // Crossover frequencies not implemented in this basic version
    (void)lowCrossover; (void)highCrossover; // Suppress unused parameter warnings
}

} // namespace time
} // namespace dsp
} // namespace dawg
