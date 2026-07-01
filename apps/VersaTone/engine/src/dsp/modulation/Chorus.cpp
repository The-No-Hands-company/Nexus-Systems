#include "../../../include/dawg/dsp/modulation/Chorus.h"
#include "../../../include/dawg/core/AudioBuffer.h"
#include <algorithm>
#include <cmath>
#include <vector>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// C++14 compatible clamp function
template<typename T>
constexpr const T& clamp(const T& v, const T& lo, const T& hi) {
    return (v < lo) ? lo : (hi < v) ? hi : v;
}

namespace dawg {
namespace dsp {
namespace modulation {

// ============================================================================
// IMPLEMENTATION CLASS
// ============================================================================

class Chorus::Impl {
public:
    // ========================================================================
    // VOICE STATE
    // ========================================================================
    
    struct VoiceState {
        std::vector<float> delayLine;   // Delay line buffer
        uint32_t writeIndex = 0;        // Current write position
        double phase = 0.0;             // LFO phase
        double delayOffset = 1.0;       // Delay time multiplier
        double rateOffset = 1.0;        // Rate multiplier
        double level = 1.0;             // Voice level
        double pan = 0.0;               // Voice pan (-1 to +1)
        
        // Per-voice filtering
        double lpFilter[2] = {0.0, 0.0}; // Low-pass filter state
        double hpFilter[2] = {0.0, 0.0}; // High-pass filter state
        
        // Per-voice saturation
        double saturationState = 0.0;
    };
    
    // ========================================================================
    // PARAMETERS
    // ========================================================================
    
    double depth = 0.5;                 // Modulation depth
    double rate = 0.5;                  // LFO rate (Hz)
    double delay = 20.0;                // Base delay (ms)
    double feedback = 0.2;              // Feedback amount
    double mix = 0.5;                   // Wet/dry mix
    uint32_t numVoices = 2;             // Number of voices
    
    ChorusMode mode = ChorusMode::Classic;
    LFOWaveform waveform = LFOWaveform::Sine;
    
    double stereoWidth = 1.0;           // Stereo width
    double voiceSpread = 0.5;           // Voice stereo spread
    bool tempoSync = false;             // Tempo synchronization
    double tempo = 120.0;               // Tempo (BPM)
    double vintageFilter = 0.0;         // Vintage filtering
    double saturation = 0.0;            // Saturation amount
    double inputGain = 1.0;             // Input gain
    double outputGain = 1.0;            // Output gain
    
    // ========================================================================
    // RUNTIME STATE
    // ========================================================================
    
    uint32_t sampleRate = 44100;
    uint32_t bufferSize = 512;
    
    std::vector<VoiceState> voices;
    uint32_t delayLineSize = 0;
    
    // Global LFO state
    double globalPhase = 0.0;
    double phaseIncrement = 0.0;
    
    // Feedback state
    std::vector<float> feedbackBuffer;
    
    // Random number generation for random LFO
    mutable std::mt19937 rng{std::random_device{}()};
    mutable std::uniform_real_distribution<double> randomDist{-1.0, 1.0};
    mutable double lastRandomValue = 0.0;
    mutable double randomHoldCounter = 0.0;
    mutable double randomHoldTime = 0.0;
    
    // ========================================================================
    // INITIALIZATION
    // ========================================================================
    
    Impl() {
        initializeVoices();
        updateInternalParameters();
    }
    
    void initializeVoices() {
        voices.resize(8); // Maximum voices
        
        // Initialize voice phases with spread
        for (uint32_t i = 0; i < 8; ++i) {
            voices[i].phase = static_cast<double>(i) / 8.0; // Phase spread
            voices[i].delayOffset = 1.0;
            voices[i].rateOffset = 1.0;
            voices[i].level = 1.0;
            voices[i].pan = 0.0;
        }
        
        updateVoiceConfiguration();
    }
    
    void updateVoiceConfiguration() {
        // Set up voice parameters based on mode
        switch (mode) {
            case ChorusMode::Classic:
                setupClassicMode();
                break;
            case ChorusMode::Ensemble:
                setupEnsembleMode();
                break;
            case ChorusMode::Wide:
                setupWideMode();
                break;
            case ChorusMode::Shimmer:
                setupShimmerMode();
                break;
            case ChorusMode::Vintage:
                setupVintageMode();
                break;
        }
        
        // Apply voice spreading
        applyVoiceSpread();
    }
    
    void setupClassicMode() {
        for (uint32_t i = 0; i < numVoices; ++i) {
            voices[i].delayOffset = 1.0 + (i * 0.1);
            voices[i].rateOffset = 1.0 + (i * 0.05);
            voices[i].level = 1.0 / std::sqrt(static_cast<double>(numVoices));
        }
    }
    
    void setupEnsembleMode() {
        for (uint32_t i = 0; i < numVoices; ++i) {
            voices[i].delayOffset = 0.8 + (i * 0.3);
            voices[i].rateOffset = 0.9 + (i * 0.2);
            voices[i].level = 0.8 / std::sqrt(static_cast<double>(numVoices));
        }
    }
    
    void setupWideMode() {
        for (uint32_t i = 0; i < numVoices; ++i) {
            voices[i].delayOffset = 1.0 + (i * 0.15);
            voices[i].rateOffset = 1.0;
            voices[i].level = 1.0 / std::sqrt(static_cast<double>(numVoices));
            // Wide mode uses more extreme panning
            voices[i].pan = (i % 2 == 0) ? -0.7 : 0.7;
        }
    }
    
    void setupShimmerMode() {
        for (uint32_t i = 0; i < numVoices; ++i) {
            voices[i].delayOffset = 0.5 + (i * 0.1);
            voices[i].rateOffset = 1.5 + (i * 0.1);
            voices[i].level = 0.7 / std::sqrt(static_cast<double>(numVoices));
        }
    }
    
    void setupVintageMode() {
        for (uint32_t i = 0; i < numVoices; ++i) {
            voices[i].delayOffset = 1.2 + (i * 0.08);
            voices[i].rateOffset = 0.8 + (i * 0.03);
            voices[i].level = 0.9 / std::sqrt(static_cast<double>(numVoices));
        }
    }
    
    void applyVoiceSpread() {
        for (uint32_t i = 0; i < numVoices; ++i) {
            if (mode != ChorusMode::Wide) { // Wide mode sets its own panning
                double normalizedIndex = static_cast<double>(i) / std::max(1.0, static_cast<double>(numVoices - 1));
                voices[i].pan = (normalizedIndex - 0.5) * 2.0 * voiceSpread;
                voices[i].pan = clamp(voices[i].pan, -1.0, 1.0);
            }
        }
    }
    
    void updateInternalParameters() {
        // Update phase increment
        double effectiveRate = rate;
        if (tempoSync) {
            // Convert BPM to Hz (quarter note sync)
            effectiveRate = tempo / 60.0 / 4.0;
        }
        
        phaseIncrement = effectiveRate / static_cast<double>(sampleRate);
        
        // Update delay line size
        double maxDelayMs = delay * 2.0; // Allow for modulation
        delayLineSize = static_cast<uint32_t>(maxDelayMs * sampleRate / 1000.0) + 1;
        
        // Resize delay lines
        for (auto& voice : voices) {
            voice.delayLine.resize(delayLineSize, 0.0f);
            voice.writeIndex = 0;
        }
        
        // Resize feedback buffer
        feedbackBuffer.resize(delayLineSize * 2, 0.0f); // Stereo
        
        // Update random LFO parameters
        randomHoldTime = sampleRate / (effectiveRate * 8.0); // 8 updates per cycle
    }
    
    // ========================================================================
    // LFO GENERATION
    // ========================================================================
    
    double generateLFO(double phase) const {
        switch (waveform) {
            case LFOWaveform::Sine:
                return std::sin(phase * 2.0 * M_PI);
            
            case LFOWaveform::Triangle:
                {
                    double normalizedPhase = phase - std::floor(phase);
                    if (normalizedPhase < 0.5) {
                        return normalizedPhase * 4.0 - 1.0;
                    } else {
                        return 3.0 - normalizedPhase * 4.0;
                    }
                }
            
            case LFOWaveform::Sawtooth:
                {
                    double normalizedPhase = phase - std::floor(phase);
                    return normalizedPhase * 2.0 - 1.0;
                }
            
            case LFOWaveform::Square:
                {
                    double normalizedPhase = phase - std::floor(phase);
                    return (normalizedPhase < 0.5) ? -1.0 : 1.0;
                }
            
            case LFOWaveform::Random:
                return generateRandomLFO();
            
            default:
                return 0.0;
        }
    }
    
    double generateRandomLFO() const {
        randomHoldCounter += 1.0;
        if (randomHoldCounter >= randomHoldTime) {
            lastRandomValue = randomDist(rng);
            randomHoldCounter = 0.0;
        }
        return lastRandomValue;
    }
    
    // ========================================================================
    // INTERPOLATION
    // ========================================================================
    
    float cubicInterpolate(const std::vector<float>& buffer, double position) const {
        int32_t size = static_cast<int32_t>(buffer.size());
        int32_t index = static_cast<int32_t>(position);
        double fraction = position - index;
        
        // Get four samples for cubic interpolation
        int32_t i0 = (index - 1 + size) % size;
        int32_t i1 = index % size;
        int32_t i2 = (index + 1) % size;
        int32_t i3 = (index + 2) % size;
        
        float y0 = buffer[i0];
        float y1 = buffer[i1];
        float y2 = buffer[i2];
        float y3 = buffer[i3];
        
        // Cubic interpolation
        float a = y3 - y2 - y0 + y1;
        float b = y0 - y1 - a;
        float c = y2 - y0;
        float d = y1;
        
        return a * fraction * fraction * fraction + 
               b * fraction * fraction + 
               c * fraction + d;
    }
    
    // ========================================================================
    // FILTERING
    // ========================================================================
    
    void applyVintageFiltering(VoiceState& voice, float& sample) {
        if (vintageFilter > 0.0) {
            // Simple low-pass filter for vintage warmth
            double cutoff = 8000.0 * (1.0 - vintageFilter * 0.5);
            double rc = 1.0 / (cutoff * 2.0 * M_PI);
            double dt = 1.0 / sampleRate;
            double alpha = dt / (rc + dt);
            
            voice.lpFilter[0] += alpha * (sample - voice.lpFilter[0]);
            sample = static_cast<float>(voice.lpFilter[0]);
        }
    }
    
    void applySaturation(VoiceState& voice, float& sample) {
        if (saturation > 0.0) {
            // Soft saturation
            double input = sample * (1.0 + saturation * 2.0);
            double output = input / (1.0 + std::abs(input));
            sample = static_cast<float>(output * 0.7); // Compensate gain
        }
    }
    
    // ========================================================================
    // PROCESSING
    // ========================================================================
    
    void process(core::AudioBuffer& buffer) {
        uint32_t numSamples = buffer.getSampleCount();
        uint32_t numChannels = buffer.getChannelCount();
        
        if (numChannels == 0 || numSamples == 0) return;
        
        // Ensure stereo processing
        bool isMono = (numChannels == 1);
        
        for (uint32_t sample = 0; sample < numSamples; ++sample) {
            // Get input samples
            float inputL = buffer.sample(0, sample) * static_cast<float>(inputGain);
            float inputR = isMono ? inputL : buffer.sample(1, sample) * static_cast<float>(inputGain);
            
            float wetL = 0.0f, wetR = 0.0f;
            
            // Process each voice
            for (uint32_t v = 0; v < numVoices; ++v) {
                VoiceState& voice = voices[v];
                
                // Calculate modulated delay time
                double lfoValue = generateLFO(voice.phase);
                double modulatedDelay = delay * voice.delayOffset * (1.0 + lfoValue * depth * 0.5);
                double delaySamples = modulatedDelay * sampleRate / 1000.0;
                
                // Calculate read position
                double readPos = voice.writeIndex - delaySamples;
                if (readPos < 0) readPos += delayLineSize;
                
                // Interpolate delayed sample
                float delayedSample = cubicInterpolate(voice.delayLine, readPos);
                
                // Apply vintage filtering
                applyVintageFiltering(voice, delayedSample);
                
                // Apply saturation
                applySaturation(voice, delayedSample);
                
                // Apply voice level
                delayedSample *= static_cast<float>(voice.level);
                
                // Pan voice
                double panL = std::sqrt((1.0 - voice.pan) * 0.5);
                double panR = std::sqrt((1.0 + voice.pan) * 0.5);
                
                wetL += delayedSample * static_cast<float>(panL);
                wetR += delayedSample * static_cast<float>(panR);
                
                // Write to delay line with feedback
                float inputToWrite = (inputL + inputR) * 0.5f + delayedSample * static_cast<float>(feedback);
                voice.delayLine[voice.writeIndex] = inputToWrite;
                
                // Advance write position
                voice.writeIndex = (voice.writeIndex + 1) % delayLineSize;
                
                // Update voice phase
                voice.phase += phaseIncrement * voice.rateOffset;
                if (voice.phase >= 1.0) voice.phase -= 1.0;
            }
            
            // Apply stereo width
            if (!isMono && stereoWidth != 1.0) {
                float mid = (wetL + wetR) * 0.5f;
                float side = (wetL - wetR) * 0.5f * static_cast<float>(stereoWidth);
                wetL = mid + side;
                wetR = mid - side;
            }
            
            // Mix with dry signal
            float dryMix = static_cast<float>(1.0 - mix);
            float wetMix = static_cast<float>(mix);
            
            float outputL = (inputL * dryMix + wetL * wetMix) * static_cast<float>(outputGain);
            float outputR = (inputR * dryMix + wetR * wetMix) * static_cast<float>(outputGain);
            
            // Write output
            buffer.sample(0, sample) = outputL;
            if (!isMono) {
                buffer.sample(1, sample) = outputR;
            }
            
            // Update global phase
            globalPhase += phaseIncrement;
            if (globalPhase >= 1.0) globalPhase -= 1.0;
        }
    }
    
    void reset() {
        // Reset all voice states
        for (auto& voice : voices) {
            std::fill(voice.delayLine.begin(), voice.delayLine.end(), 0.0f);
            voice.writeIndex = 0;
            voice.phase = 0.0;
            voice.lpFilter[0] = voice.lpFilter[1] = 0.0;
            voice.hpFilter[0] = voice.hpFilter[1] = 0.0;
            voice.saturationState = 0.0;
        }
        
        // Reset feedback buffer
        std::fill(feedbackBuffer.begin(), feedbackBuffer.end(), 0.0f);
        
        // Reset LFO state
        globalPhase = 0.0;
        randomHoldCounter = 0.0;
        lastRandomValue = 0.0;
    }
};

// ============================================================================
// CHORUS CLASS IMPLEMENTATION
// ============================================================================

Chorus::Chorus() : pImpl(std::make_unique<Impl>()) {}

Chorus::~Chorus() = default;

// ========================================================================
// EFFECT INTERFACE
// ========================================================================

void Chorus::process(core::AudioBuffer& buffer) {
    pImpl->process(buffer);
}

void Chorus::reset() {
    pImpl->reset();
}

void Chorus::setSampleRate(uint32_t sampleRate) {
    pImpl->sampleRate = sampleRate;
    pImpl->updateInternalParameters();
}

void Chorus::setBufferSize(uint32_t bufferSize) {
    pImpl->bufferSize = bufferSize;
}

void Chorus::setParameter(const std::string& name, double value) {
    if (name == "depth") setDepth(value);
    else if (name == "rate") setRate(value);
    else if (name == "delay") setDelay(value);
    else if (name == "feedback") setFeedback(value);
    else if (name == "mix") setMix(value);
    else if (name == "voices") setVoices(static_cast<uint32_t>(value));
    else if (name == "stereoWidth") setStereoWidth(value);
    else if (name == "voiceSpread") setVoiceSpread(value);
    else if (name == "vintageFilter") setVintageFilter(value);
    else if (name == "saturation") setSaturation(value);
    else if (name == "inputGain") setInputGain(value);
    else if (name == "outputGain") setOutputGain(value);
}

double Chorus::getParameter(const std::string& name) const {
    if (name == "depth") return pImpl->depth;
    else if (name == "rate") return pImpl->rate;
    else if (name == "delay") return pImpl->delay;
    else if (name == "feedback") return pImpl->feedback;
    else if (name == "mix") return pImpl->mix;
    else if (name == "voices") return static_cast<double>(pImpl->numVoices);
    else if (name == "stereoWidth") return pImpl->stereoWidth;
    else if (name == "voiceSpread") return pImpl->voiceSpread;
    else if (name == "vintageFilter") return pImpl->vintageFilter;
    else if (name == "saturation") return pImpl->saturation;
    else if (name == "inputGain") return pImpl->inputGain;
    else if (name == "outputGain") return pImpl->outputGain;
    return 0.0;
}

std::vector<std::string> Chorus::getParameterNames() const {
    return {
        "depth", "rate", "delay", "feedback", "mix", "voices",
        "stereoWidth", "voiceSpread", "vintageFilter", "saturation",
        "inputGain", "outputGain"
    };
}

uint32_t Chorus::getLatency() const {
    return static_cast<uint32_t>(pImpl->delay * pImpl->sampleRate / 1000.0);
}

// ========================================================================
// CHORUS-SPECIFIC CONTROL
// ========================================================================

void Chorus::setDepth(double depth) {
    pImpl->depth = clamp(depth, 0.0, 1.0);
}

void Chorus::setRate(double rate) {
    pImpl->rate = clamp(rate, 0.1, 10.0);
    pImpl->updateInternalParameters();
}

void Chorus::setDelay(double delay) {
    pImpl->delay = clamp(delay, 5.0, 50.0);
    pImpl->updateInternalParameters();
}

void Chorus::setFeedback(double feedback) {
    pImpl->feedback = clamp(feedback, 0.0, 0.7);
}

void Chorus::setMix(double mix) {
    pImpl->mix = clamp(mix, 0.0, 1.0);
}

void Chorus::setVoices(uint32_t voices) {
    pImpl->numVoices = clamp(voices, 1u, 8u);
    pImpl->updateVoiceConfiguration();
}

void Chorus::setChorusMode(ChorusMode mode) {
    pImpl->mode = mode;
    pImpl->updateVoiceConfiguration();
}

void Chorus::setLFOWaveform(LFOWaveform waveform) {
    pImpl->waveform = waveform;
}

void Chorus::setStereoWidth(double width) {
    pImpl->stereoWidth = clamp(width, 0.0, 2.0);
}

void Chorus::setVoiceSpread(double spread) {
    pImpl->voiceSpread = clamp(spread, 0.0, 1.0);
    pImpl->updateVoiceConfiguration();
}

void Chorus::setTempoSync(bool sync) {
    pImpl->tempoSync = sync;
    pImpl->updateInternalParameters();
}

void Chorus::setTempo(double bpm) {
    pImpl->tempo = clamp(bpm, 60.0, 200.0);
    if (pImpl->tempoSync) {
        pImpl->updateInternalParameters();
    }
}

void Chorus::setVintageFilter(double amount) {
    pImpl->vintageFilter = clamp(amount, 0.0, 1.0);
}

void Chorus::setSaturation(double amount) {
    pImpl->saturation = clamp(amount, 0.0, 1.0);
}

void Chorus::setInputGain(double gain) {
    double gainDb = clamp(gain, -20.0, 20.0);
    pImpl->inputGain = std::pow(10.0, gainDb / 20.0);
}

void Chorus::setOutputGain(double gain) {
    double gainDb = clamp(gain, -20.0, 20.0);
    pImpl->outputGain = std::pow(10.0, gainDb / 20.0);
}

// ========================================================================
// ADVANCED VOICE CONTROL
// ========================================================================

void Chorus::setVoiceDelayOffset(uint32_t voiceIndex, double offset) {
    if (voiceIndex < 8) {
        pImpl->voices[voiceIndex].delayOffset = clamp(offset, 0.5, 2.0);
    }
}

void Chorus::setVoiceRateOffset(uint32_t voiceIndex, double offset) {
    if (voiceIndex < 8) {
        pImpl->voices[voiceIndex].rateOffset = clamp(offset, 0.5, 2.0);
    }
}

void Chorus::setVoiceLevel(uint32_t voiceIndex, double level) {
    if (voiceIndex < 8) {
        pImpl->voices[voiceIndex].level = clamp(level, 0.0, 1.0);
    }
}

void Chorus::setVoicePan(uint32_t voiceIndex, double pan) {
    if (voiceIndex < 8) {
        pImpl->voices[voiceIndex].pan = clamp(pan, -1.0, 1.0);
    }
}

// ========================================================================
// METERING AND ANALYSIS
// ========================================================================

double Chorus::getLFOPhase() const {
    return pImpl->globalPhase;
}

double Chorus::getModulationValue() const {
    return pImpl->generateLFO(pImpl->globalPhase);
}

bool Chorus::isActive() const {
    return pImpl->mix > 0.0 && pImpl->depth > 0.0;
}

} // namespace modulation
} // namespace dsp
} // namespace dawg
