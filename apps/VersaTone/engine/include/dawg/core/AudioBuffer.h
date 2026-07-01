#pragma once

/**
 * @file AudioBuffer.h
 * @brief Professional audio buffer management for DAWG engine
 * @version 1.0.0
 * 
 * High-performance audio buffer with sample-accurate timing,
 * multi-channel support, and optimized memory management.
 */

#include <cstdint>
#include <memory>

namespace dawg {
namespace core {

/**
 * @brief Audio format specification
 */
struct AudioFormat {
    uint32_t sampleRate = 48000;      ///< Sample rate in Hz
    uint16_t channels = 2;            ///< Number of channels
    uint16_t bitsPerSample = 32;      ///< Bits per sample (32-bit float)
    uint32_t bufferSize = 512;        ///< Buffer size in samples
    
    bool operator==(const AudioFormat& other) const noexcept {
        return sampleRate == other.sampleRate && 
               channels == other.channels && 
               bitsPerSample == other.bitsPerSample;
    }
    
    bool operator!=(const AudioFormat& other) const noexcept {
        return !(*this == other);
    }
};

/**
 * @brief High-performance audio buffer with sample-accurate timing
 * 
 * Optimized for real-time audio processing with SIMD alignment,
 * efficient memory management, and multi-channel support.
 */
class AudioBuffer {
public:
    /**
     * @brief Construct audio buffer with specified format
     * @param channels Number of audio channels
     * @param samples Number of samples per channel
     */
    AudioBuffer(uint16_t channels, uint32_t samples);
    
    /**
     * @brief Destructor
     */
    ~AudioBuffer();
    
    // Non-copyable but movable for performance
    AudioBuffer(const AudioBuffer&) = delete;
    AudioBuffer& operator=(const AudioBuffer&) = delete;
    AudioBuffer(AudioBuffer&&) noexcept;
    AudioBuffer& operator=(AudioBuffer&&) noexcept;
    
    /**
     * @brief Get pointer to channel data
     * @param channel Channel index (0-based)
     * @return Pointer to channel data or nullptr if invalid
     */
    float* getChannelData(uint16_t channel) noexcept;
    const float* getChannelData(uint16_t channel) const noexcept;
    
    /**
     * @brief Access individual sample
     * @param channel Channel index
     * @param index Sample index
     * @return Reference to sample
     */
    float& sample(uint16_t channel, uint32_t index) noexcept;
    const float& sample(uint16_t channel, uint32_t index) const noexcept;
    
    /**
     * @brief Get number of channels
     * @return Channel count
     */
    uint16_t getChannelCount() const noexcept;
    
    /**
     * @brief Get number of samples per channel
     * @return Sample count
     */
    uint32_t getSampleCount() const noexcept;
    
    /**
     * @brief Clear all audio data to silence
     */
    void clear() noexcept;
    
    /**
     * @brief Copy data from another buffer
     * @param source Source buffer to copy from
     */
    void copyFrom(const AudioBuffer& source) noexcept;
    
    /**
     * @brief Add data from another buffer with optional gain
     * @param source Source buffer to add
     * @param gain Gain multiplier (default 1.0)
     */
    void addFrom(const AudioBuffer& source, float gain = 1.0f) noexcept;
    
    /**
     * @brief Apply gain to all channels
     * @param gain Gain multiplier
     */
    void applyGain(float gain) noexcept;
    
    /**
     * @brief Get RMS level for a channel
     * @param channel Channel index
     * @return RMS level (0.0 to 1.0+)
     */
    float getRMSLevel(uint16_t channel) const noexcept;
    
    /**
     * @brief Get peak level for a channel
     * @param channel Channel index
     * @return Peak level (0.0 to 1.0+)
     */
    float getPeakLevel(uint16_t channel) const noexcept;
    
    /**
     * @brief Set timestamp for this buffer
     * @param timestamp Sample-accurate timestamp
     */
    void setTimestamp(uint64_t timestamp) noexcept;
    
    /**
     * @brief Get timestamp for this buffer
     * @return Sample-accurate timestamp
     */
    uint64_t getTimestamp() const noexcept;
    
    /**
     * @brief Get sample at specific channel and frame
     * @param channel Channel index
     * @param frame Frame index
     * @return Sample value
     */
    float getSample(uint16_t channel, uint32_t frame) const noexcept;
    
    /**
     * @brief Set sample at specific channel and frame
     * @param channel Channel index
     * @param frame Frame index
     * @param value Sample value
     */
    void setSample(uint16_t channel, uint32_t frame, float value) noexcept;
    
    /**
     * @brief Get number of channels
     * @return Channel count
     */
    uint16_t getChannels() const noexcept { return m_channels; }
    
    /**
     * @brief Copy buffer data to interleaved format
     * @param dest Destination buffer (interleaved)
     * @param numFrames Number of frames to copy
     */
    void copyToInterleaved(float* dest, uint32_t numFrames) const noexcept;

private:
    uint16_t m_channels;
    uint32_t m_samples;
    uint64_t m_timestamp;
    std::unique_ptr<float[]> m_data;
};

} // namespace core
} // namespace dawg
