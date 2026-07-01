#pragma once

/**
 * @file Mixer.h
 * @brief Professional Digital Mixing Console
 * @version 1.0.0
 * 
 * Industry-standard digital mixing console with:
 * - 128+ input channels with full processing
 * - 32 aux sends/returns
 * - 8 stereo mix buses
 * - Professional automation system
 * - Real-time metering and analysis
 * - Sub-sample accurate routing
 */

#include <vector>
#include <memory>
#include <string>
#include <map>
#include <atomic>
#include <cstdint>

// Forward declarations
namespace dawg { 
    namespace core { class AudioBuffer; }
    namespace dsp { class Effect; class EffectsChain; }
}

namespace dawg {
namespace dsp {

/**
 * @brief Professional channel strip with full processing
 */
class ChannelStrip {
public:
    struct Settings {
        // Input
        double inputGain = 0.0;       // dB (-60 to +20)
        bool inputInvert = false;
        
        // EQ section
        bool eqEnabled = true;
        double highShelfFreq = 12000.0;   // Hz
        double highShelfGain = 0.0;       // dB
        double highMidFreq = 2500.0;      // Hz  
        double highMidGain = 0.0;         // dB
        double highMidQ = 0.707;
        double lowMidFreq = 250.0;        // Hz
        double lowMidGain = 0.0;          // dB
        double lowMidQ = 0.707;
        double lowShelfFreq = 80.0;       // Hz
        double lowShelfGain = 0.0;        // dB
        
        // Dynamics
        bool compressorEnabled = false;
        double compThreshold = -12.0;     // dB
        double compRatio = 4.0;           // X:1
        double compAttack = 10.0;         // ms
        double compRelease = 100.0;       // ms
        bool gateEnabled = false;
        double gateThreshold = -40.0;     // dB
        double gateRange = -80.0;         // dB
        
        // Insert effects
        bool insertsEnabled = true;
        
        // Aux sends (8 sends)
        std::vector<double> auxSendLevels = std::vector<double>(8, 0.0);  // dB
        std::vector<bool> auxSendPre = std::vector<bool>(8, false);       // Pre/post fader
        
        // Main section
        double faderLevel = 0.0;          // dB (-inf to +10)
        double panPosition = 0.0;         // -1.0 (L) to +1.0 (R)
        bool mute = false;
        bool solo = false;
        
        // Routing
        uint32_t outputBus = 0;           // Which mix bus (0 = main L/R)
    };
    
    ChannelStrip(uint32_t channelNumber);
    ~ChannelStrip();
    
    // Processing
    void process(core::AudioBuffer& input, std::vector<core::AudioBuffer*>& auxOutputs, 
                core::AudioBuffer& mainOutput);
    void reset();
    void setSampleRate(uint32_t sampleRate);
    void setBufferSize(uint32_t bufferSize);
    
    // Configuration
    void setSettings(const Settings& settings);
    Settings getSettings() const;
    
    // Insert effects
    void addInsertEffect(std::unique_ptr<Effect> effect);
    void removeInsertEffect(size_t index);
    Effect* getInsertEffect(size_t index);
    size_t getNumInsertEffects() const;
    
    // Metering
    double getInputLevel() const;         // dBFS
    double getOutputLevel() const;        // dBFS
    double getGainReduction() const;      // dB
    
    // Channel info
    uint32_t getChannelNumber() const { return channelNumber; }
    void setChannelName(const std::string& name) { channelName = name; }
    std::string getChannelName() const { return channelName; }
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
    uint32_t channelNumber;
    std::string channelName;
};

/**
 * @brief Auxiliary send/return processor
 */
class AuxBus {
public:
    AuxBus(uint32_t busNumber);
    ~AuxBus();
    
    // Processing
    void process(core::AudioBuffer& input, core::AudioBuffer& output);
    void reset();
    void setSampleRate(uint32_t sampleRate);
    void setBufferSize(uint32_t bufferSize);
    
    // Controls
    void setReturnLevel(double level);    // dB
    double getReturnLevel() const;
    void setMute(bool mute);
    bool isMuted() const;
    
    // Effects
    void addEffect(std::unique_ptr<Effect> effect);
    void removeEffect(size_t index);
    Effect* getEffect(size_t index);
    size_t getNumEffects() const;
    
    // Info
    uint32_t getBusNumber() const { return busNumber; }
    void setBusName(const std::string& name) { busName = name; }
    std::string getBusName() const { return busName; }
    
    // Metering
    double getInputLevel() const;
    double getOutputLevel() const;
    
private:
    uint32_t busNumber;
    std::string busName;
};

/**
 * @brief Professional digital mixing console
 * 
 * Features:
 * - 128 input channels with full processing
 * - 32 auxiliary sends/returns  
 * - 8 stereo mix buses
 * - Master section with processing
 * - Real-time automation
 * - Professional metering
 * - Sub-sample accurate timing
 */
class MixingConsole {
public:
    MixingConsole();
    ~MixingConsole();
    
    // Console configuration
    void setNumChannels(uint32_t numChannels);
    void setNumAuxBuses(uint32_t numAuxBuses);
    void setNumMixBuses(uint32_t numMixBuses);
    uint32_t getNumChannels() const;
    uint32_t getNumAuxBuses() const;
    uint32_t getNumMixBuses() const;
    
    // Audio processing
    void process(std::vector<core::AudioBuffer>& inputs, 
                std::vector<core::AudioBuffer>& outputs);
    void reset();
    void setSampleRate(uint32_t sampleRate);
    void setBufferSize(uint32_t bufferSize);
    
    // Channel access
    ChannelStrip* getChannel(uint32_t channelIndex);
    const ChannelStrip* getChannel(uint32_t channelIndex) const;
    
    // Aux bus access
    AuxBus* getAuxBus(uint32_t busIndex);
    const AuxBus* getAuxBus(uint32_t busIndex) const;
    
    // Master section
    void setMasterLevel(double level);    // dB
    double getMasterLevel() const;
    void setMasterMute(bool mute);
    bool isMasterMuted() const;
    
    // Master effects
    void addMasterEffect(std::unique_ptr<Effect> effect);
    void removeMasterEffect(size_t index);
    Effect* getMasterEffect(size_t index);
    size_t getNumMasterEffects() const;
    
    // Solo system
    void clearAllSolos();
    bool isAnySoloActive() const;
    std::vector<uint32_t> getSoloedChannels() const;
    
    // Metering
    double getMasterOutputLevel() const;  // dBFS
    std::vector<double> getChannelLevels() const;
    std::vector<double> getAuxLevels() const;
    
    // Automation
    void setAutomationEnabled(bool enabled);
    bool isAutomationEnabled() const;
    void recordAutomation(uint32_t channelIndex, const std::string& parameter, 
                         double value, uint64_t timestamp);
    
    // Scene management
    void saveScene(const std::string& sceneName);
    void loadScene(const std::string& sceneName);
    std::vector<std::string> getSceneNames() const;
    
private:
    // TODO: Add implementation members
};

} // namespace dsp
} // namespace dawg
