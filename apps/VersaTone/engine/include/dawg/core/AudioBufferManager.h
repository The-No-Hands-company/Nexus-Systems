#pragma once

/**
 * @file AudioBufferManager.h
 * @brief Professional Low-Latency Audio Buffer Management
 * @version 1.0.0
 * 
 * Ultra-low latency circular buffer system for real-time audio:
 * - Lock-free circular buffers for zero-blocking I/O
 * - Multiple buffer sizes (64, 128, 256, 512 samples)
 * - Automatic buffer size adaptation
 * - Real-time safe memory allocation
 * - NUMA-aware memory layout
 * - Sub-sample accurate timing
 */

#include <cstdint>
#include <atomic>
#include <memory>
#include <vector>

// Forward declarations
namespace dawg { namespace core { class AudioBuffer; } }

namespace dawg {
namespace core {

/**
 * @brief Lock-free circular buffer for real-time audio
 * 
 * Thread-safe, wait-free circular buffer optimized for audio I/O.
 * Uses atomic operations for coordination between producer and consumer.
 */
class CircularAudioBuffer {
public:
    CircularAudioBuffer(uint32_t channels, uint32_t capacityInSamples);
    ~CircularAudioBuffer();
    
    // Writing (producer thread)
    uint32_t write(const float* data, uint32_t numSamples);
    uint32_t writeInterleaved(const float* data, uint32_t numFrames);
    uint32_t getWriteAvailable() const;
    
    // Reading (consumer thread)
    uint32_t read(float* data, uint32_t numSamples);
    uint32_t readInterleaved(float* data, uint32_t numFrames);
    uint32_t getReadAvailable() const;
    
    // Buffer management
    void clear();
    void reset();
    uint32_t getCapacity() const { return capacity; }
    uint32_t getChannels() const { return channels; }
    
    // Performance monitoring
    bool hasOverrun() const { return overrunFlag.load(); }
    bool hasUnderrun() const { return underrunFlag.load(); }
    void clearFlags();
    
private:
    uint32_t channels;
    uint32_t capacity;
    std::unique_ptr<float[]> buffer;
    
    // Atomic indices for lock-free operation
    std::atomic<uint32_t> writeIndex{0};
    std::atomic<uint32_t> readIndex{0};
    
    // Performance flags
    std::atomic<bool> overrunFlag{false};
    std::atomic<bool> underrunFlag{false};
    
    // Helper methods
    uint32_t advanceIndex(uint32_t index, uint32_t amount) const;
    uint32_t getDistance(uint32_t from, uint32_t to) const;
};

/**
 * @brief Adaptive buffer size manager
 * 
 * Automatically adjusts buffer sizes based on system performance:
 * - Monitors CPU load and audio dropouts
 * - Adapts buffer size for optimal latency vs stability
 * - Provides buffer size recommendations
 */
class AdaptiveBufferManager {
public:
    AdaptiveBufferManager();
    ~AdaptiveBufferManager();
    
    // Performance monitoring
    void reportCpuLoad(double cpuPercentage);
    void reportXrun();  // Buffer underrun/overrun
    void reportLatency(double latencyMs);
    
    // Buffer size management
    uint32_t getRecommendedBufferSize() const;
    void setTargetLatency(double targetLatencyMs);
    double getTargetLatency() const;
    
    // Statistics
    double getAverageCpuLoad() const;
    uint32_t getXrunCount() const;
    double getAverageLatency() const;
    void resetStatistics();
    
    // Configuration
    void setMinBufferSize(uint32_t minSize);
    void setMaxBufferSize(uint32_t maxSize);
    uint32_t getMinBufferSize() const;
    uint32_t getMaxBufferSize() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Real-time safe memory pool
 * 
 * Pre-allocated memory pool for real-time audio processing:
 * - No malloc/free in audio callback
 * - Multiple pool sizes for different allocations
 * - Automatic memory alignment (SIMD friendly)
 * - NUMA-aware allocation on multi-CPU systems
 */
class AudioMemoryPool {
public:
    AudioMemoryPool();
    ~AudioMemoryPool();
    
    // Pool configuration
    void initialize(uint32_t maxBuffers, uint32_t maxSamplesPerBuffer, uint32_t maxChannels);
    void shutdown();
    
    // Buffer allocation (real-time safe)
    AudioBuffer* acquireBuffer(uint32_t samples, uint32_t channels);
    void releaseBuffer(AudioBuffer* buffer);
    
    // Pool statistics
    uint32_t getTotalBuffers() const;
    uint32_t getAvailableBuffers() const;
    uint32_t getUsedBuffers() const;
    double getFragmentation() const;
    
    // Memory management
    void defragment();  // Call from non-RT thread
    size_t getTotalMemoryUsage() const;
    size_t getActiveMemoryUsage() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

/**
 * @brief Professional audio buffer manager
 * 
 * Coordinates all buffer management for the audio engine:
 * - Manages circular buffers for I/O
 * - Provides adaptive buffer sizing
 * - Real-time safe memory allocation
 * - Performance monitoring and optimization
 */
class AudioBufferManager {
public:
    AudioBufferManager();
    ~AudioBufferManager();
    
    // Initialization
    void initialize(uint32_t sampleRate, uint32_t initialBufferSize, 
                   uint32_t maxChannels = 64);
    void shutdown();
    
    // Buffer creation
    std::unique_ptr<CircularAudioBuffer> createCircularBuffer(
        uint32_t channels, uint32_t capacityInSamples);
    
    // Adaptive management
    void enableAdaptiveBuffering(bool enable);
    bool isAdaptiveBufferingEnabled() const;
    
    // Performance monitoring
    void reportPerformanceData(double cpuLoad, uint32_t xruns, double latency);
    uint32_t getRecommendedBufferSize() const;
    
    // Memory management
    AudioBuffer* acquireBuffer(uint32_t samples, uint32_t channels);
    void releaseBuffer(AudioBuffer* buffer);
    
    // Configuration
    void setSampleRate(uint32_t sampleRate);
    void setBufferSize(uint32_t bufferSize);
    uint32_t getSampleRate() const;
    uint32_t getBufferSize() const;
    
    // Statistics
    struct Statistics {
        double averageCpuLoad;
        uint32_t totalXruns;
        double averageLatency;
        uint32_t totalBuffersAllocated;
        uint32_t activeBuffers;
        size_t memoryUsage;
        double bufferEfficiency;
    };
    
    Statistics getStatistics() const;
    void resetStatistics();
    
private:
    class Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace core
} // namespace dawg
