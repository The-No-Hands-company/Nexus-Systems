#pragma once

/**
 * @file DeviceManager.h
 * @brief Professional Real-Time Audio Device Manager
 * @version 1.0.0
 * 
 * WASAPI/DirectSound/ASIO integration for professional audio I/O
 * Ultra-low latency real-time audio processing
 */

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <cstdint>

// Forward declarations
namespace dawg { namespace core { class AudioBuffer; } }

namespace dawg {
namespace io {

/**
 * @brief Audio device information
 */
struct AudioDevice {
    std::string id;
    std::string name;
    std::string driver;
    uint32_t maxInputChannels;
    uint32_t maxOutputChannels;
    std::vector<uint32_t> supportedSampleRates;
    std::vector<uint32_t> supportedBufferSizes;
    bool isDefault;
    bool isExclusive;
};

/**
 * @brief Audio stream configuration
 */
struct AudioStreamConfig {
    uint32_t sampleRate = 44100;
    uint32_t bufferSize = 256;  // Ultra-low latency
    uint32_t inputChannels = 0;
    uint32_t outputChannels = 2;
    std::string inputDeviceId;
    std::string outputDeviceId;
    bool useExclusiveMode = true;  // Professional mode
};

/**
 * @brief Audio callback function type
 */
using AudioCallback = std::function<void(
    const dawg::core::AudioBuffer& input,
    dawg::core::AudioBuffer& output,
    uint32_t frameCount
)>;

/**
 * @brief Professional Real-Time Audio Device Manager
 * 
 * Features:
 * - WASAPI exclusive mode for ultra-low latency
 * - DirectSound fallback for compatibility
 * - ASIO driver support for professional interfaces
 * - Real-time priority audio threads
 * - Automatic device detection and monitoring
 */
class DeviceManager {
public:
    DeviceManager();
    virtual ~DeviceManager();
    
    // Device enumeration
    std::vector<AudioDevice> getInputDevices() const;
    std::vector<AudioDevice> getOutputDevices() const;
    AudioDevice getDefaultInputDevice() const;
    AudioDevice getDefaultOutputDevice() const;
    
    // Stream management
    bool openStream(const AudioStreamConfig& config, AudioCallback callback);
    bool closeStream();
    bool startStream();
    bool stopStream();
    bool isStreamActive() const;
    bool isStreamRunning() const;
    
    // Real-time performance
    double getStreamLatency() const;
    uint32_t getCurrentBufferSize() const;
    uint32_t getCurrentSampleRate() const;
    double getCpuLoad() const;
    uint32_t getXruns() const;  // Buffer underruns/overruns
    
    // Configuration
    void setCallback(AudioCallback callback);
    bool setBufferSize(uint32_t bufferSize);
    bool setSampleRate(uint32_t sampleRate);
    
    // Error handling
    std::string getLastError() const;
    void clearErrors();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace io
} // namespace dawg
