#include "dawg/dsp/Mixer.h"
#include "dawg/core/AudioBuffer.h"
#include <vector>
#include <memory>
#include <atomic>
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dawg {
namespace dsp {

// Forward declaration for Effect (will be implemented later)
class Effect {
public:
    virtual ~Effect() = default;
    virtual void process(core::AudioBuffer& buffer) = 0;
    virtual void setSampleRate(uint32_t sampleRate) = 0;
    virtual void setBufferSize(uint32_t bufferSize) = 0;
    virtual bool isBypassed() const = 0;
};

//=============================================================================
// ChannelStrip::Impl - Private implementation
//=============================================================================
class ChannelStrip::Impl {
public:
    Impl(uint32_t channelNum) : channelNumber(channelNum) {
        // Initialize with default settings
        settings.inputGain = 0.0;
        settings.faderLevel = 0.0;
        settings.panPosition = 0.0;
        settings.auxSendLevels.resize(8, -60.0); // -60dB (muted)
        settings.auxSendPre.resize(8, false);
    }
    
    void process(core::AudioBuffer& input, std::vector<core::AudioBuffer*>& auxOutputs, 
                core::AudioBuffer& mainOutput) {
        if (settings.mute) {
            mainOutput.clear();
            return;
        }
        
        // Copy input to working buffer
        workingBuffer.copyFrom(input);
        
        // Input gain stage
        float inputGainLinear = dbToLinear(settings.inputGain);
        workingBuffer.applyGain(inputGainLinear);
        
        // EQ processing
        if (settings.eqEnabled) {
            processEQ(workingBuffer);
        }
        
        // Dynamics processing
        if (settings.compressorEnabled) {
            processCompressor(workingBuffer);
        }
        
        // Insert effects processing
        for (auto& effect : insertEffects) {
            if (effect && !effect->isBypassed()) {
                effect->process(workingBuffer);
            }
        }
        
        // Aux sends (pre or post fader)
        for (size_t i = 0; i < auxOutputs.size() && i < settings.auxSendLevels.size(); ++i) {
            if (auxOutputs[i] && settings.auxSendLevels[i] > -60.0) {
                float sendLevel = dbToLinear(settings.auxSendLevels[i]);
                
                // Use pre-fader signal if enabled
                core::AudioBuffer& sendSource = settings.auxSendPre[i] ? workingBuffer : mainOutput;
                
                // Add to aux bus with send level
                for (uint16_t ch = 0; ch < sendSource.getChannels() && ch < auxOutputs[i]->getChannels(); ++ch) {
                    for (uint32_t frame = 0; frame < sendSource.getSampleCount(); ++frame) {
                        float sample = sendSource.getSample(ch, frame) * sendLevel;
                        float existing = auxOutputs[i]->getSample(ch, frame);
                        auxOutputs[i]->setSample(ch, frame, existing + sample);
                    }
                }
            }
        }
        
        // Main fader
        float faderGainLinear = dbToLinear(settings.faderLevel);
        workingBuffer.applyGain(faderGainLinear);
        
        // Pan processing
        if (workingBuffer.getChannels() == 1 && mainOutput.getChannels() >= 2) {
            // Mono to stereo panning
            float panLeft = std::cos((settings.panPosition + 1.0) * M_PI / 4.0);
            float panRight = std::sin((settings.panPosition + 1.0) * M_PI / 4.0);
            
            for (uint32_t frame = 0; frame < workingBuffer.getSampleCount(); ++frame) {
                float monoSample = workingBuffer.getSample(0, frame);
                mainOutput.setSample(0, frame, monoSample * panLeft);
                mainOutput.setSample(1, frame, monoSample * panRight);
            }
        } else {
            // Direct copy for matching channel counts
            mainOutput.copyFrom(workingBuffer);
        }
        
        // Update metering
        updateMetering(workingBuffer);
    }
    
    void setSampleRate(uint32_t sr) {
        sampleRate = sr;
        // Update internal processing sample rates
        for (auto& effect : insertEffects) {
            if (effect) {
                effect->setSampleRate(sr);
            }
        }
    }
    
    void setBufferSize(uint32_t bs) {
        bufferSize = bs;
        // Resize working buffer if needed
        if (workingBuffer.getSampleCount() != bs) {
            workingBuffer = core::AudioBuffer(workingBuffer.getChannels(), bs);
        }
        
        for (auto& effect : insertEffects) {
            if (effect) {
                effect->setBufferSize(bs);
            }
        }
    }
    
private:
    uint32_t channelNumber;
    uint32_t sampleRate = 48000;
    uint32_t bufferSize = 512;
    
    ChannelStrip::Settings settings;
    core::AudioBuffer workingBuffer{2, 512}; // Stereo working buffer
    
    std::vector<std::unique_ptr<Effect>> insertEffects;
    
    // Metering
    std::atomic<float> inputLevel{0.0f};
    std::atomic<float> outputLevel{0.0f};
    std::atomic<float> gainReduction{0.0f};
    
    // Helper functions
    float dbToLinear(double db) const {
        if (db <= -60.0) return 0.0f;
        return std::pow(10.0f, db / 20.0f);
    }
    
    void processEQ(core::AudioBuffer& buffer) {
        // TODO: Implement parametric EQ
        // For now, simple high/low shelf simulation
    }
    
    void processCompressor(core::AudioBuffer& buffer) {
        // TODO: Implement professional compressor
        // For now, simple limiting
        for (uint16_t ch = 0; ch < buffer.getChannels(); ++ch) {
            for (uint32_t frame = 0; frame < buffer.getSampleCount(); ++frame) {
                float sample = buffer.getSample(ch, frame);
                if (std::abs(sample) > 0.95f) {
                    sample = std::copysign(0.95f, sample);
                    buffer.setSample(ch, frame, sample);
                    gainReduction = 20.0f * std::log10(0.95f / std::abs(sample));
                }
            }
        }
    }
    
    void updateMetering(const core::AudioBuffer& buffer) {
        float inputSum = 0.0f, outputSum = 0.0f;
        uint32_t sampleCount = buffer.getSampleCount() * buffer.getChannels();
        
        for (uint16_t ch = 0; ch < buffer.getChannels(); ++ch) {
            for (uint32_t frame = 0; frame < buffer.getSampleCount(); ++frame) {
                float sample = buffer.getSample(ch, frame);
                inputSum += sample * sample;
                outputSum += sample * sample;
            }
        }
        
        inputLevel = std::sqrt(inputSum / sampleCount);
        outputLevel = std::sqrt(outputSum / sampleCount);
    }
    
    friend class ChannelStrip;
};

//=============================================================================
// ChannelStrip public interface
//=============================================================================
ChannelStrip::ChannelStrip(uint32_t channelNumber) 
    : pImpl(std::make_unique<Impl>(channelNumber)), channelNumber(channelNumber) {}

ChannelStrip::~ChannelStrip() = default;

void ChannelStrip::process(core::AudioBuffer& input, std::vector<core::AudioBuffer*>& auxOutputs, 
                          core::AudioBuffer& mainOutput) {
    pImpl->process(input, auxOutputs, mainOutput);
}

void ChannelStrip::reset() {
    // Reset internal state
}

void ChannelStrip::setSampleRate(uint32_t sampleRate) {
    pImpl->setSampleRate(sampleRate);
}

void ChannelStrip::setBufferSize(uint32_t bufferSize) {
    pImpl->setBufferSize(bufferSize);
}

void ChannelStrip::setSettings(const Settings& settings) {
    pImpl->settings = settings;
}

ChannelStrip::Settings ChannelStrip::getSettings() const {
    return pImpl->settings;
}

void ChannelStrip::addInsertEffect(std::unique_ptr<Effect> effect) {
    if (effect) {
        effect->setSampleRate(pImpl->sampleRate);
        effect->setBufferSize(pImpl->bufferSize);
        pImpl->insertEffects.push_back(std::move(effect));
    }
}

void ChannelStrip::removeInsertEffect(size_t index) {
    if (index < pImpl->insertEffects.size()) {
        pImpl->insertEffects.erase(pImpl->insertEffects.begin() + index);
    }
}

Effect* ChannelStrip::getInsertEffect(size_t index) {
    if (index < pImpl->insertEffects.size()) {
        return pImpl->insertEffects[index].get();
    }
    return nullptr;
}

size_t ChannelStrip::getNumInsertEffects() const {
    return pImpl->insertEffects.size();
}

double ChannelStrip::getInputLevel() const {
    return 20.0 * std::log10(std::max(0.001f, pImpl->inputLevel.load()));
}

double ChannelStrip::getOutputLevel() const {
    return 20.0 * std::log10(std::max(0.001f, pImpl->outputLevel.load()));
}

double ChannelStrip::getGainReduction() const {
    return pImpl->gainReduction.load();
}

// AuxBus implementation  
AuxBus::AuxBus(uint32_t busNumber) : busNumber(busNumber) {}
AuxBus::~AuxBus() = default;

void AuxBus::process(core::AudioBuffer& input, core::AudioBuffer& output) {}
void AuxBus::reset() {}
void AuxBus::setSampleRate(uint32_t sampleRate) {}
void AuxBus::setBufferSize(uint32_t bufferSize) {}
void AuxBus::setReturnLevel(double level) {}
double AuxBus::getReturnLevel() const { return 0.0; }
void AuxBus::setMute(bool mute) {}
bool AuxBus::isMuted() const { return false; }
void AuxBus::addEffect(std::unique_ptr<Effect> effect) {}
void AuxBus::removeEffect(size_t index) {}
Effect* AuxBus::getEffect(size_t index) { return nullptr; }
size_t AuxBus::getNumEffects() const { return 0; }
double AuxBus::getInputLevel() const { return 0.0; }
double AuxBus::getOutputLevel() const { return 0.0; }

// MixingConsole implementation
MixingConsole::MixingConsole() {}
MixingConsole::~MixingConsole() = default;

void MixingConsole::setNumChannels(uint32_t numChannels) {}
void MixingConsole::setNumAuxBuses(uint32_t numAuxBuses) {}
void MixingConsole::setNumMixBuses(uint32_t numMixBuses) {}
uint32_t MixingConsole::getNumChannels() const { return 0; }
uint32_t MixingConsole::getNumAuxBuses() const { return 0; }
uint32_t MixingConsole::getNumMixBuses() const { return 0; }

void MixingConsole::process(std::vector<core::AudioBuffer>& inputs, 
                           std::vector<core::AudioBuffer>& outputs) {
    // TODO: Implement full mixing console processing
}

void MixingConsole::reset() {}
void MixingConsole::setSampleRate(uint32_t sampleRate) {}
void MixingConsole::setBufferSize(uint32_t bufferSize) {}

ChannelStrip* MixingConsole::getChannel(uint32_t channelIndex) { return nullptr; }
const ChannelStrip* MixingConsole::getChannel(uint32_t channelIndex) const { return nullptr; }
AuxBus* MixingConsole::getAuxBus(uint32_t busIndex) { return nullptr; }
const AuxBus* MixingConsole::getAuxBus(uint32_t busIndex) const { return nullptr; }

void MixingConsole::setMasterLevel(double level) {}
double MixingConsole::getMasterLevel() const { return 0.0; }
void MixingConsole::setMasterMute(bool mute) {}
bool MixingConsole::isMasterMuted() const { return false; }

void MixingConsole::addMasterEffect(std::unique_ptr<Effect> effect) {}
void MixingConsole::removeMasterEffect(size_t index) {}
Effect* MixingConsole::getMasterEffect(size_t index) { return nullptr; }
size_t MixingConsole::getNumMasterEffects() const { return 0; }

void MixingConsole::clearAllSolos() {}
bool MixingConsole::isAnySoloActive() const { return false; }
std::vector<uint32_t> MixingConsole::getSoloedChannels() const { return {}; }

double MixingConsole::getMasterOutputLevel() const { return 0.0; }
std::vector<double> MixingConsole::getChannelLevels() const { return {}; }
std::vector<double> MixingConsole::getAuxLevels() const { return {}; }

void MixingConsole::setAutomationEnabled(bool enabled) {}
bool MixingConsole::isAutomationEnabled() const { return false; }
void MixingConsole::recordAutomation(uint32_t channelIndex, const std::string& parameter, 
                     double value, uint64_t timestamp) {}

void MixingConsole::saveScene(const std::string& sceneName) {}
void MixingConsole::loadScene(const std::string& sceneName) {}
std::vector<std::string> MixingConsole::getSceneNames() const { return {}; }

} // namespace dsp
} // namespace dawg
