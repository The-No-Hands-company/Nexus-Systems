/*
 * â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•— â–ˆâ–ˆâ•—    â–ˆâ–ˆâ•— â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•— 
 * â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘    â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•”â•â•â•â•â• 
 * â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘ â–ˆâ•— â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ–ˆâ•—
 * â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘â–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘
 * â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â•šâ–ˆâ–ˆâ–ˆâ•”â–ˆâ–ˆâ–ˆâ•”â•â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•
 * â•šâ•â•â•â•â•â• â•šâ•â•  â•šâ•â• â•šâ•â•â•â•šâ•â•â•  â•šâ•â•â•â•â•â• 
 * 
 * Digital Audio Workstation Engine
 * High-Performance C++ Audio Processing Framework
 * 
 * Copyright (c) 2025 The No-Hands Company
 * Licensed under MIT License
 * 
 * File: VoiceManager.h
 * Purpose: Header file for audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#pragma once

/**
 * @file VoiceManager.h
 * @brief Revolutionary multi-tier voice management system
 * @version 1.0.0
 * 
 * Manages 292,864+ simultaneous voices across multiple tiers:
 * - RAM voices (4,096): Ultra-low latency, full samples in memory
 * - Streaming voices (32,768): Low latency, disk streaming
 * - Virtual voices (256,000): CPU-free when inaudible
 */

#include "AudioBuffer.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <atomic>

namespace dawg {
namespace core {

/**
 * @brief Voice tier classification for optimal resource usage
 */
enum class VoiceTier : uint8_t {
    RAM = 0,         ///< Full sample in RAM (1-4ms latency)
    Streaming = 1,   ///< Disk streaming (2-8ms latency)  
    Virtual = 2      ///< Computed/synthesized (CPU-free when inaudible)
};

/**
 * @brief Voice priority system with intelligent management
 */
enum class VoicePriority : uint8_t {
    Background = 0,   ///< Ambient, reverb tails
    Low = 2,          ///< Secondary elements
    Normal = 4,       ///< Regular instruments
    Important = 6,    ///< Lead instruments
    Critical = 8,     ///< Drums, bass
    Solo = 10,        ///< Solo instruments
    Master = 12,      ///< Main melody
    Essential = 14,   ///< Must never be stolen
    Untouchable = 15  ///< System-critical voices
};

/**
 * @brief Voice allocation strategy
 */
enum class AllocationStrategy : uint8_t {
    PerformanceFirst, ///< Prioritize low latency
    CapacityFirst,    ///< Prioritize voice count
    QualityFirst,     ///< Prioritize audio quality
    Balanced,         ///< Balance all factors
    Adaptive          ///< AI-driven allocation
};

/**
 * @brief Sample quality configuration
 */
struct SampleQuality {
    uint8_t bitDepth = 24;        ///< Bit depth (1-64 bits)
    uint32_t sampleRate = 48000;  ///< Sample rate (8kHz-384kHz)
    uint8_t channels = 1;         ///< Number of channels (1-128)
    bool isFloat = false;         ///< Integer vs float samples
    bool isCompressed = false;    ///< Lossless compression
    
    // Quality presets
    static SampleQuality CD() { return {16, 44100, 2, false, false}; }
    static SampleQuality DVD() { return {24, 48000, 2, false, false}; }
    static SampleQuality Studio() { return {24, 96000, 2, false, false}; }
    static SampleQuality Ultimate() { return {32, 192000, 2, true, false}; }
};

/**
 * @brief Sample layer for multi-sampling and velocity layers
 */
struct SampleLayer {
    std::vector<uint8_t> rawSampleData;  ///< Raw sample data
    SampleQuality quality;               ///< Quality settings
    uint64_t sampleCount = 0;           ///< Number of samples
    
    // Mapping parameters
    uint8_t keyMin = 0, keyMax = 127;           ///< Key range
    uint8_t velocityMin = 0, velocityMax = 127; ///< Velocity range
    uint8_t rootKey = 60;                       ///< Root key (middle C)
    
    // Loop settings
    bool loop = false;
    uint64_t loopStart = 0, loopEnd = 0;
    
    // Playback settings
    float volume = 1.0f;
    float pitch = 1.0f;
    VoiceTier preferredTier = VoiceTier::RAM;
};

/**
 * @brief Voice instance with complete state
 */
struct Voice {
    // Voice identification
    uint32_t voiceId = 0;
    uint64_t startTime = 0;
    
    // MIDI parameters
    uint8_t note = 60;
    uint8_t velocity = 127;
    uint8_t channel = 0;
    
    // Playback state
    std::atomic<bool> active{false};
    std::atomic<bool> isReleasing{false};
    std::atomic<double> phase{0.0};
    VoiceTier currentTier = VoiceTier::RAM;
    VoicePriority priority = VoicePriority::Normal;
    
    // Audio processing
    float gain = 1.0f;
    float panPosition = 0.0f;  // -1.0 to +1.0
    
    // Sample reference
    const SampleLayer* sampleLayer = nullptr;
    
    // Envelope parameters
    struct Envelope {
        float attackTime = 0.01f;    // seconds
        float decayTime = 0.1f;      // seconds
        float sustainLevel = 0.8f;   // 0.0 to 1.0
        float releaseTime = 0.2f;    // seconds
    } envelope;
    
    // Envelope state
    enum class EnvelopeState { Idle, Attack, Decay, Sustain, Release };
    EnvelopeState envelopeState = EnvelopeState::Idle;
    float envelopeValue = 0.0f;
    double envelopeStartTime = 0.0;
    
    // Performance tracking
    float cpuUsage = 0.0f;
    uint64_t samplesProcessed = 0;
};

/**
 * @brief Voice management statistics
 */
struct VoiceStats {
    uint32_t totalActiveVoices = 0;
    uint32_t ramVoices = 0;
    uint32_t streamingVoices = 0;
    uint32_t virtualVoices = 0;
    uint32_t voicesPromoted = 0;     ///< Virtual -> RAM/Streaming
    uint32_t voicesDemoted = 0;      ///< RAM/Streaming -> Virtual  
    uint32_t voicesStolen = 0;       ///< Voices stolen due to priority
    float totalCpuUsage = 0.0f;
    float allocationEfficiency = 1.0f;
};

/**
 * @brief Revolutionary multi-tier voice management system
 * 
 * Manages up to 292,864+ voices across multiple performance tiers
 * with intelligent allocation and real-time tier management.
 */
class VoiceManager {
public:
    /**
     * @brief Constructor
     * @param maxRAMVoices Maximum RAM voices (default 4096)
     * @param maxStreamingVoices Maximum streaming voices (default 32768)
     * @param maxVirtualVoices Maximum virtual voices (default 256000)
     */
    VoiceManager(uint32_t maxRAMVoices = 4096,
                 uint32_t maxStreamingVoices = 32768,
                 uint32_t maxVirtualVoices = 256000);
    
    /**
     * @brief Set the sample rate for audio processing
     * @param sampleRate Sample rate in Hz
     */
    void setSampleRate(uint32_t sampleRate);
    
    /**
     * @brief Destructor
     */
    ~VoiceManager();
    
    // Non-copyable
    VoiceManager(const VoiceManager&) = delete;
    VoiceManager& operator=(const VoiceManager&) = delete;
    
    /**
     * @brief Allocate a new voice
     * @param note MIDI note (0-127)
     * @param velocity MIDI velocity (0-127)
     * @param channel MIDI channel (0-15)
     * @param sampleLayer Sample layer to use
     * @param priority Voice priority
     * @return Pointer to allocated voice or nullptr if failed
     */
    Voice* allocateVoice(uint8_t note, uint8_t velocity, uint8_t channel,
                        const SampleLayer* sampleLayer,
                        VoicePriority priority = VoicePriority::Normal);
    
    /**
     * @brief Deallocate a voice
     * @param voice Voice to deallocate
     */
    void deallocateVoice(Voice* voice);
    
    /**
     * @brief Deallocate all voices
     */
    void deallocateAllVoices();
    
    /**
     * @brief Handle MIDI note off
     * @param note MIDI note
     * @param channel MIDI channel
     */
    void noteOff(uint8_t note, uint8_t channel);
    
    /**
     * @brief Stop all notes on a channel
     * @param channel MIDI channel (255 = all channels)
     */
    void allNotesOff(uint8_t channel = 255);
    
    /**
     * @brief Process all voices and fill audio buffer
     * @param buffer Output audio buffer
     */
    void processAllTiers(AudioBuffer& buffer);
    
    /**
     * @brief Set allocation strategy
     * @param strategy Allocation strategy to use
     */
    void setAllocationStrategy(AllocationStrategy strategy);
    
    /**
     * @brief Get current voice statistics
     * @return Voice statistics
     */
    VoiceStats getStats() const;
    
    /**
     * @brief Get total voice capacity
     * @return Maximum possible simultaneous voices
     */
    uint32_t getTotalVoiceCapacity() const;
    
    /**
     * @brief Get active voice count
     * @return Number of currently active voices
     */
    uint32_t getActiveVoiceCount() const;
    
    /**
     * @brief Get voice count by tier
     * @param tier Voice tier to query
     * @return Number of voices in specified tier
     */
    uint32_t getVoiceCountByTier(VoiceTier tier) const;

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace core
} // namespace dawg

