#pragma once

/**
 * @file AudioEngine.h
 * @brief Core DAWG audio engine class
 * @version 1.0.0
 * 
 * The main audio engine that coordinates all audio processing,
 * voice management, and real-time audio operations.
 */

#include <memory>
#include <vector>
#include <atomic>
#include <string>

namespace dawg {
namespace core {

// Forward declarations
class VoiceManager;
class Transport;
class Timeline;
class AudioBuffer;

/**
 * @brief Engine configuration parameters
 */
struct EngineConfig {
    uint32_t sampleRate = 48000;        ///< Sample rate in Hz
    uint16_t channels = 2;              ///< Number of output channels
    uint32_t bufferSize = 512;          ///< Audio buffer size in samples
    uint32_t maxRAMVoices = 4096;       ///< Maximum RAM voices
    uint32_t maxStreamingVoices = 32768; ///< Maximum streaming voices
    uint32_t maxVirtualVoices = 256000;  ///< Maximum virtual voices
    bool enableMachineLearning = true;   ///< Enable ML optimization
    std::string audioDeviceName;        ///< Preferred audio device
    
    /// Default configuration for professional use
    static EngineConfig defaultConfig() {
        return EngineConfig{};
    }
    
    /// High-performance configuration
    static EngineConfig highPerformanceConfig() {
        EngineConfig config;
        config.sampleRate = 96000;
        config.bufferSize = 256;
        config.maxRAMVoices = 8192;
        return config;
    }
    
    /// Low-latency configuration
    static EngineConfig lowLatencyConfig() {
        EngineConfig config;
        config.bufferSize = 128;
        config.maxRAMVoices = 2048;
        config.maxStreamingVoices = 16384;
        return config;
    }
};

/**
 * @brief Main DAWG audio engine class
 * 
 * This is the central hub that coordinates all audio processing,
 * voice management, and real-time operations.
 */
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    
    // Non-copyable
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;
    
    // Movable
    AudioEngine(AudioEngine&&) noexcept;
    AudioEngine& operator=(AudioEngine&&) noexcept;
    
    /**
     * @brief Initialize the audio engine
     * @param config Configuration parameters
     * @return true if successful, false otherwise
     */
    bool initialize(const EngineConfig& config);
    
    /**
     * @brief Shutdown the audio engine
     */
    void shutdown();
    
    /**
     * @brief Check if the engine is initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const noexcept;
    
    /**
     * @brief Start audio processing
     * @return true if successful, false otherwise
     */
    bool start();
    
    /**
     * @brief Stop audio processing
     */
    void stop();
    
    /**
     * @brief Check if audio is running
     * @return true if running, false otherwise
     */
    bool isRunning() const noexcept;
    
    /**
     * @brief Process audio block (called by audio thread)
     * @param outputBuffer Output audio buffer
     * @param inputBuffer Input audio buffer (optional)
     */
    void processAudio(AudioBuffer& outputBuffer, const AudioBuffer* inputBuffer = nullptr);
    
    /**
     * @brief Get the voice manager
     * @return Reference to voice manager
     */
    VoiceManager& getVoiceManager() noexcept;
    
    /**
     * @brief Get the transport controls
     * @return Reference to transport
     */
    Transport& getTransport() noexcept;
    
    /**
     * @brief Get the timeline
     * @return Reference to timeline
     */
    Timeline& getTimeline() noexcept;
    
    /**
     * @brief Get current configuration
     * @return Current engine configuration
     */
    const EngineConfig& getConfig() const noexcept;
    
    /**
     * @brief Get engine performance statistics
     * @return Performance stats structure
     */
    struct PerformanceStats {
        float cpuUsage = 0.0f;
        uint32_t activeVoices = 0;
        uint32_t droppedSamples = 0;
        float latencyMs = 0.0f;
        uint64_t samplesProcessed = 0;
    };
    
    PerformanceStats getPerformanceStats() const;
    
    /**
     * @brief Set master volume
     * @param volume Volume level (0.0 to 1.0)
     */
    void setMasterVolume(float volume);
    
    /**
     * @brief Get master volume
     * @return Current master volume (0.0 to 1.0)
     */
    float getMasterVolume() const noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace core
} // namespace dawg
