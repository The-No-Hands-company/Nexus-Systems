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
 * File: VoiceManager.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "dawg/core/VoiceManager.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <thread>
#include <mutex>
#include <atomic>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dawg {
namespace core {

// Helper function for clamping values
template<typename T>
constexpr T clamp(T value, T min, T max) {
    return std::max(min, std::min(value, max));
}

/**
 * @brief Private implementation class (PIMPL pattern)
 */
class VoiceManager::Impl {
public:
    // Voice pools
    std::vector<std::unique_ptr<Voice>> m_ramVoicePool;
    std::vector<std::unique_ptr<Voice>> m_streamingVoicePool;
    std::vector<std::unique_ptr<Voice>> m_virtualVoicePool;
    
    // Active voice lists
    std::vector<Voice*> m_activeRAMVoices;
    std::vector<Voice*> m_activeStreamingVoices;
    std::vector<Voice*> m_activeVirtualVoices;
    
// Configuration
uint32_t m_maxRAMVoices;
uint32_t m_maxStreamingVoices;
uint32_t m_maxVirtualVoices;
uint32_t m_sampleRate = 48000;
AllocationStrategy m_strategy = AllocationStrategy::Balanced;
    
    // Voice management
    std::atomic<uint32_t> m_nextVoiceId{1};
    std::atomic<bool> m_shouldStop{false};
    std::thread m_managementThread;
    mutable std::mutex m_voiceMutex;
    
    // Statistics
    mutable VoiceStats m_stats;
    
    // Constructor
    Impl(uint32_t maxRAMVoices, uint32_t maxStreamingVoices, uint32_t maxVirtualVoices)
        : m_maxRAMVoices(maxRAMVoices)
        , m_maxStreamingVoices(maxStreamingVoices)
        , m_maxVirtualVoices(maxVirtualVoices) {
        
        // Pre-allocate RAM voice pool
        m_ramVoicePool.reserve(maxRAMVoices);
        for (uint32_t i = 0; i < maxRAMVoices; ++i) {
            auto voice = std::make_unique<Voice>();
            voice->voiceId = m_nextVoiceId++;
            voice->currentTier = VoiceTier::RAM;
            m_ramVoicePool.push_back(std::move(voice));
        }
        
        // Pre-allocate streaming voice pool
        m_streamingVoicePool.reserve(maxStreamingVoices);
        for (uint32_t i = 0; i < maxStreamingVoices; ++i) {
            auto voice = std::make_unique<Voice>();
            voice->voiceId = m_nextVoiceId++;
            voice->currentTier = VoiceTier::Streaming;
            m_streamingVoicePool.push_back(std::move(voice));
        }
        
        // Pre-allocate virtual voice pool
        m_virtualVoicePool.reserve(maxVirtualVoices);
        for (uint32_t i = 0; i < maxVirtualVoices; ++i) {
            auto voice = std::make_unique<Voice>();
            voice->voiceId = m_nextVoiceId++;
            voice->currentTier = VoiceTier::Virtual;
            m_virtualVoicePool.push_back(std::move(voice));
        }
        
        // Start voice management thread
        m_managementThread = std::thread(&Impl::voiceManagementLoop, this);
    }
    
    // Destructor
    ~Impl() {
        m_shouldStop.store(true);
        if (m_managementThread.joinable()) {
            m_managementThread.join();
        }
    }
    
    // Voice allocation methods
    Voice* allocateVoice(uint8_t note, uint8_t velocity, uint8_t channel,
                        const SampleLayer* sampleLayer, VoicePriority priority) {
        std::lock_guard<std::mutex> lock(m_voiceMutex);
        
        // Try RAM tier first for best performance
        if (auto voice = allocateFromRAMPool(note, velocity, channel, priority, sampleLayer)) {
            return voice;
        }
        
        // Try streaming tier
        if (auto voice = allocateFromStreamingPool(note, velocity, channel, priority, sampleLayer)) {
            return voice;
        }
        
        // Fall back to virtual tier
        if (auto voice = allocateFromVirtualPool(note, velocity, channel, priority, sampleLayer)) {
            return voice;
        }
        
        return nullptr;
    }
    
    Voice* allocateFromRAMPool(uint8_t note, uint8_t velocity, uint8_t channel,
                              VoicePriority priority, const SampleLayer* layer) {
        // Find available voice
        for (auto& voice : m_ramVoicePool) {
            if (!voice->active.load()) {
                initializeVoice(voice.get(), note, velocity, channel, priority, layer);
                m_activeRAMVoices.push_back(voice.get());
                m_stats.ramVoices++;
                return voice.get();
            }
        }
        
        // Try to steal a lower priority voice
        Voice* candidate = findStealableVoice(m_activeRAMVoices, priority);
        if (candidate) {
            deallocateVoiceInternal(candidate);
            initializeVoice(candidate, note, velocity, channel, priority, layer);
            m_stats.voicesStolen++;
            return candidate;
        }
        
        return nullptr;
    }
    
    Voice* allocateFromStreamingPool(uint8_t note, uint8_t velocity, uint8_t channel,
                                    VoicePriority priority, const SampleLayer* layer) {
        // Find available voice
        for (auto& voice : m_streamingVoicePool) {
            if (!voice->active.load()) {
                initializeVoice(voice.get(), note, velocity, channel, priority, layer);
                m_activeStreamingVoices.push_back(voice.get());
                m_stats.streamingVoices++;
                return voice.get();
            }
        }
        
        // Try to steal a lower priority voice
        Voice* candidate = findStealableVoice(m_activeStreamingVoices, priority);
        if (candidate) {
            deallocateVoiceInternal(candidate);
            initializeVoice(candidate, note, velocity, channel, priority, layer);
            m_stats.voicesStolen++;
            return candidate;
        }
        
        return nullptr;
    }
    
    Voice* allocateFromVirtualPool(uint8_t note, uint8_t velocity, uint8_t channel,
                                  VoicePriority priority, const SampleLayer* layer) {
        // Find available voice
        for (auto& voice : m_virtualVoicePool) {
            if (!voice->active.load()) {
                initializeVoice(voice.get(), note, velocity, channel, priority, layer);
                m_activeVirtualVoices.push_back(voice.get());
                m_stats.virtualVoices++;
                return voice.get();
            }
        }
        
        return nullptr; // Virtual pool should rarely be full
    }
    
void initializeVoice(Voice* voice, uint8_t note, uint8_t velocity, uint8_t channel,
                     VoicePriority priority, const SampleLayer* layer) {
        voice->note = note;
        voice->velocity = velocity;
        voice->channel = channel;
        voice->priority = priority;
        voice->sampleLayer = layer;
        voice->phase.store(0.0);
        voice->gain = velocity / 127.0f;
        voice->panPosition = 0.0f;
        voice->cpuUsage = 0.0f;
        voice->samplesProcessed = 0;
        voice->startTime = std::chrono::steady_clock::now().time_since_epoch().count();
        voice->active.store(true);
        
        // Initialize envelope for note-on
        voice->envelopeState = Voice::EnvelopeState::Attack;
        voice->envelopeValue = 0.0f;
        voice->envelopeStartTime = std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    }
    
    Voice* findStealableVoice(const std::vector<Voice*>& voices, VoicePriority newPriority) {
        Voice* candidate = nullptr;
        VoicePriority lowestPriority = VoicePriority::Untouchable;
        
        for (auto voice : voices) {
            if (voice->priority < newPriority && voice->priority < lowestPriority) {
                candidate = voice;
                lowestPriority = voice->priority;
            }
        }
        
        return candidate;
    }
    
    void deallocateVoice(Voice* voice) {
        std::lock_guard<std::mutex> lock(m_voiceMutex);
        deallocateVoiceInternal(voice);
    }
    
    void deallocateVoiceInternal(Voice* voice) {
        if (!voice || !voice->active.load()) return;
        
        voice->active.store(false);
        
        // Remove from active lists
        auto removeFromVector = [voice](std::vector<Voice*>& vec) {
            vec.erase(std::remove(vec.begin(), vec.end(), voice), vec.end());
        };
        
        switch (voice->currentTier) {
            case VoiceTier::RAM:
                removeFromVector(m_activeRAMVoices);
                m_stats.ramVoices--;
                break;
            case VoiceTier::Streaming:
                removeFromVector(m_activeStreamingVoices);
                m_stats.streamingVoices--;
                break;
            case VoiceTier::Virtual:
                removeFromVector(m_activeVirtualVoices);
                m_stats.virtualVoices--;
                break;
        }
    }
    
    void deallocateAllVoices() {
        std::lock_guard<std::mutex> lock(m_voiceMutex);
        
        // Deallocate all active voices
        for (auto voice : m_activeRAMVoices) {
            voice->active.store(false);
        }
        for (auto voice : m_activeStreamingVoices) {
            voice->active.store(false);
        }
        for (auto voice : m_activeVirtualVoices) {
            voice->active.store(false);
        }
        
        m_activeRAMVoices.clear();
        m_activeStreamingVoices.clear();
        m_activeVirtualVoices.clear();
        
        m_stats.ramVoices = 0;
        m_stats.streamingVoices = 0;
        m_stats.virtualVoices = 0;
    }
    
    void noteOff(uint8_t note, uint8_t channel) {
        std::lock_guard<std::mutex> lock(m_voiceMutex);
        
        auto processVoices = [note, channel](std::vector<Voice*>& voices) {
            for (auto voice : voices) {
                if (voice->note == note && voice->channel == channel && voice->active.load()) {
                    // Trigger envelope release
                    voice->envelopeState = Voice::EnvelopeState::Release;
                    voice->envelopeStartTime = std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                }
            }
        };
        
        processVoices(m_activeRAMVoices);
        processVoices(m_activeStreamingVoices);
        processVoices(m_activeVirtualVoices);
    }
    
    void allNotesOff(uint8_t channel) {
        std::lock_guard<std::mutex> lock(m_voiceMutex);
        
        auto processVoices = [channel](std::vector<Voice*>& voices) {
            for (auto voice : voices) {
                if ((channel == 255 || voice->channel == channel) && voice->active.load()) {
                    // Trigger envelope release
                    voice->envelopeState = Voice::EnvelopeState::Release;
                    voice->envelopeStartTime = std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                }
            }
        };
        
        processVoices(m_activeRAMVoices);
        processVoices(m_activeStreamingVoices);
        processVoices(m_activeVirtualVoices);
    }
    
    void processAllTiers(AudioBuffer& buffer) {
        std::lock_guard<std::mutex> lock(m_voiceMutex);
        
        // Process all active voices and mix into buffer
        processVoiceList(m_activeRAMVoices, buffer);
        processVoiceList(m_activeStreamingVoices, buffer);
        processVoiceList(m_activeVirtualVoices, buffer);
        
        updateStats();
    }
    
void processVoiceList(std::vector<Voice*>& voices, AudioBuffer& buffer) {
    const float sampleRate = static_cast<float>(m_sampleRate); // Use configured sample rate
    const uint32_t frameCount = buffer.getSampleCount();
        
        for (auto it = voices.begin(); it != voices.end();) {
            Voice* voice = *it;
            
            if (!voice->active.load()) {
                it = voices.erase(it);
                continue;
            }
            
            // Simple sine wave synthesis for demonstration
            const float frequency = 440.0f * std::pow(2.0f, (voice->note - 69) / 12.0f);
            const float phaseIncrement = frequency / sampleRate;
            
for (uint32_t i = 0; i < frameCount; ++i) {
                 float sample = 0.0f;
                 
                 // Update envelope
                 double currentTime = std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
                 double timeInState = currentTime - voice->envelopeStartTime;
                 
                 switch (voice->envelopeState) {
                     case Voice::EnvelopeState::Attack:
                         if (timeInState >= voice->envelope.attackTime) {
                             voice->envelopeState = Voice::EnvelopeState::Decay;
                             voice->envelopeStartTime = currentTime;
                             timeInState = 0.0;
                         }
                         voice->envelopeValue = timeInState / voice->envelope.attackTime;
                         break;
                     
                     case Voice::EnvelopeState::Decay:
                         if (timeInState >= voice->envelope.decayTime) {
                             voice->envelopeState = Voice::EnvelopeState::Sustain;
                             voice->envelopeStartTime = currentTime;
                             timeInState = 0.0;
                         }
                         voice->envelopeValue = 1.0f - (1.0f - voice->envelope.sustainLevel) * (timeInState / voice->envelope.decayTime);
                         break;
                     
                     case Voice::EnvelopeState::Sustain:
                         voice->envelopeValue = voice->envelope.sustainLevel;
                         break;
                     
                     case Voice::EnvelopeState::Release:
                         if (timeInState >= voice->envelope.releaseTime) {
                             voice->envelopeState = Voice::EnvelopeState::Idle;
                             voice->active.store(false);
                             voice->envelopeValue = 0.0f;
                         } else {
                             voice->envelopeValue = voice->envelope.sustainLevel * (1.0f - timeInState / voice->envelope.releaseTime);
                         }
                         break;
                     
                     case Voice::EnvelopeState::Idle:
                     default:
                         voice->envelopeValue = 0.0f;
                         break;
                 }
                 
                 // Play back actual sample data if available
                 if (voice->sampleLayer && !voice->sampleLayer->rawSampleData.empty()) {
                     // Simple linear interpolation for resampling (could be improved)
                     // Note: This is a simplified implementation - a proper resampler would be better
                     double sampleIndex = voice->phase * voice->sampleLayer->sampleCount;
                     uint64_t index0 = static_cast<uint64_t>(sampleIndex);
                     uint64_t index1 = index0 + 1;
                     float frac = static_cast<float>(sampleIndex - index0);
                     
                     // Handle looping
                     if (voice->sampleLayer->loop) {
                         if (index0 >= voice->sampleLayer->loopEnd) {
                             index0 = voice->sampleLayer->loopStart + 
                                     (static_cast<uint64_t>((index0 - voice->sampleLayer->loopStart) % 
                                                              (voice->sampleLayer->loopEnd - voice->sampleLayer->loopStart)));
                             index1 = voice->sampleLayer->loopStart + 
                                     (static_cast<uint64_t>((index1 - voice->sampleLayer->loopStart) % 
                                                              (voice->sampleLayer->loopEnd - voice->sampleLayer->loopStart)));
                         }
                     }
                     
                     // Get sample values (assuming 16-bit signed integer format for now)
                     if (index0 < voice->sampleLayer->sampleCount && 
                         index1 < voice->sampleLayer->sampleCount) {
                         // Convert 16-bit samples to float (-1.0 to 1.0 range)
                         int16_t sample0 = *reinterpret_cast<int16_t*>(&voice->sampleLayer->rawSampleData[index0 * 2]);
                         int16_t sample1 = *reinterpret_cast<int16_t*>(&voice->sampleLayer->rawSampleData[index1 * 2]);
                         float floatSample0 = static_cast<float>(sample0) / 32768.0f;
                         float floatSample1 = static_cast<float>(sample1) / 32768.0f;
                         
                         // Linear interpolation
                         sample = floatSample0 + frac * (floatSample1 - floatSample0);
                         
                         // Apply voice gain, layer volume, and envelope
                         sample *= voice->gain * voice->sampleLayer->volume * voice->envelopeValue;
                     }
                 } else {
// Fallback to sine wave if no sample data (for testing)
                     const float frequency = 440.0f * std::pow(2.0f, (voice->note - 69) / 12.0f);
                     const float phaseIncrement = frequency / m_sampleRate;
                     double phase = voice->phase.load();
                     float waveSample = std::sin(2.0 * M_PI * phase) * 0.1f;
                     sample = waveSample * voice->gain * voice->envelopeValue;
                 }
                
// Mix into buffer with simple panning
                 if (buffer.getChannelCount() >= 2) {
                     // Stereo panning
                     buffer.getChannelData(0)[i] += sample * (1.0f - 0.5f * voice->panPosition); // Left channel
                     buffer.getChannelData(1)[i] += sample * (0.5f + 0.5f * voice->panPosition); // Right channel
                 } else {
                     // Mono or other channel configurations
                     for (uint32_t ch = 0; ch < buffer.getChannelCount(); ++ch) {
                         buffer.getChannelData(ch)[i] += sample;
                     }
                 }
                
// Update phase
                 double phaseIncrement = 0.0;
                 if (voice->sampleLayer && !voice->sampleLayer->rawSampleData.empty()) {
                     // Calculate phase increment based on sample rate ratio and pitch
                     double sampleRateRatio = static_cast<double>(voice->sampleLayer->quality.sampleRate) / m_sampleRate;
                     double pitchFactor = std::pow(2.0, (voice->note - voice->sampleLayer->rootKey) / 12.0) * voice->sampleLayer->pitch;
                     phaseIncrement = sampleRateRatio * pitchFactor;
                 } else {
                     // Fallback for sine wave
                     const float frequency = 440.0f * std::pow(2.0f, (voice->note - 69) / 12.0f);
                     phaseIncrement = frequency / m_sampleRate;
                 }
                
                voice->phase += phaseIncrement;
                if (voice->phase >= 1.0) voice->phase -= 1.0;
                
voice->samplesProcessed += 1;
             
             // Check if voice should be deactivated (envelope has finished)
             if (voice->envelopeValue <= 0.001f && voice->envelopeState == Voice::EnvelopeState::Release) {
                 voice->active.store(false);
             }
            
            ++it;
        }
    }
    
    void updateStats() {
        m_stats.totalActiveVoices = m_activeRAMVoices.size() + 
                                   m_activeStreamingVoices.size() + 
                                   m_activeVirtualVoices.size();
        
        // Calculate CPU usage (simplified)
        float totalCpu = 0.0f;
        for (auto voice : m_activeRAMVoices) totalCpu += voice->cpuUsage;
        for (auto voice : m_activeStreamingVoices) totalCpu += voice->cpuUsage;
        for (auto voice : m_activeVirtualVoices) totalCpu += voice->cpuUsage;
        
        m_stats.totalCpuUsage = totalCpu;
        m_stats.allocationEfficiency = m_stats.totalActiveVoices > 0 ? 
            (float)m_stats.ramVoices / m_stats.totalActiveVoices : 1.0f;
    }
    
    void voiceManagementLoop() {
        // TODO: Replace this polling loop with a more efficient mechanism
        // such as condition variables or event-driven notifications
        while (!m_shouldStop.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            // Perform voice tier optimization
            optimizeVoiceTiers();
        }
    }
    
    void optimizeVoiceTiers() {
        std::lock_guard<std::mutex> lock(m_voiceMutex);
        
        // Promote high-priority virtual voices to streaming/RAM
        promoteVirtualVoices();
        
        // Demote low-priority RAM voices to streaming/virtual
        demoteRAMVoices();
    }
    
    void promoteVirtualVoices() {
        // Sort virtual voices by suitability for promotion (priority, age, etc.)
        std::vector<std::pair<float, Voice*>> scoredVoices;
        double now = std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        for (auto voice : m_activeVirtualVoices) {
            // Calculate a score for promotion - higher is better
            // Factors: priority (higher=better), age (newer=better for stealing), envelope (sustained=better)
            double age = now - voice->startTime / 1e9; // Convert nanoseconds to seconds
            double ageFactor = 1.0 / (1.0 + age * 0.1); // Newer voices get higher score
            
            // Envelope factor - voices in sustain or release are better candidates
            double envelopeFactor = 1.0;
            if (voice->envelopeState == Voice::EnvelopeState::Sustain) {
                envelopeFactor = 1.5;
            } else if (voice->envelopeState == Voice::EnvelopeState::Release) {
                envelopeFactor = 1.2;
            }
            
            float score = static_cast<float>(voice->priority) * static_cast<float>(ageFactor) * static_cast<float>(envelopeFactor);
            scoredVoices.emplace_back(score, voice);
        }
        
        // Sort by score descending (highest priority first)
        std::sort(scoredVoices.begin(), scoredVoices.end(), 
                  [](const auto& a, const auto& b) { return a.first > b.first; });
        
        // Promote voices based on score until we run out of space
        for (const auto& [score, voice] : scoredVoices) {
            // Try to promote to streaming tier first
            if (m_activeStreamingVoices.size() < m_maxStreamingVoices) {
                voice->currentTier = VoiceTier::Streaming;
                // Move from virtual to streaming list
                auto it = std::find(m_activeVirtualVoices.begin(), m_activeVirtualVoices.end(), voice);
                if (it != m_activeVirtualVoices.end()) {
                    m_activeVirtualVoices.erase(it);
                }
                m_activeStreamingVoices.push_back(voice);
                m_stats.voicesPromoted++;
            } 
            // If streaming is full, try to promote to RAM (steal if needed)
            else if (m_activeRAMVoices.size() < m_maxRAMVoices) {
                voice->currentTier = VoiceTier::RAM;
                // Move from virtual to RAM list
                auto it = std::find(m_activeVirtualVoices.begin(), m_activeVirtualVoices.end(), voice);
                if (it != m_activeVirtualVoices.end()) {
                    m_activeVirtualVoices.erase(it);
                }
                m_activeRAMVoices.push_back(voice);
                m_stats.voicesPromoted++;
            }
        }
    }
    
    void demoteRAMVoices() {
        // Only demote if we're over capacity
        if (m_activeRAMVoices.size() <= m_maxRAMVoices * 0.8f) {
            return;
        }
        
        // Sort RAM voices by suitability for demotion (lower priority first, older first, etc.)
        std::vector<std::pair<float, Voice*>> scoredVoices;
        double now = std::chrono::duration<double>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
        
        for (auto voice : m_activeRAMVoices) {
            // Calculate a score for demotion - lower is worse (more likely to be demoted)
            // Factors: priority (lower=worse), age (older=worse for demotion), envelope (released=worse)
            double age = now - voice->startTime / 1e9; // Convert nanoseconds to seconds
            double ageFactor = 1.0 + (age * 0.1); // Older voices get higher score (worse for demotion)
            
            // Envelope factor - voices that are released are worse candidates to keep
            double envelopeFactor = 1.0;
            if (voice->envelopeState == Voice::EnvelopeState::Release) {
                envelopeFactor = 2.0; // Released voices are much worse to keep
            } else if (voice->envelopeState == Voice::EnvelopeState::Sustain) {
                envelopeFactor = 0.8; // Sustain voices are slightly better to keep
            }
            
            float score = static_cast<float>(voice->priority) * static_cast<float>(ageFactor) * static_cast<float>(envelopeFactor);
            scoredVoices.emplace_back(score, voice);
        }
        
        // Sort by score ascending (lowest priority first - most likely to be demoted)
        std::sort(scoredVoices.begin(), scoredVoices.end(), 
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        
        // Demote voices until we're under the threshold
        size_t targetSize = static_cast<size_t>(m_maxRAMVoices * 0.8f);
        size_t demoteCount = m_activeRAMVoices.size() - targetSize;
        
        for (size_t i = 0; i < demoteCount && i < scoredVoices.size(); ++i) {
            Voice* voice = scoredVoices[i].second;
            
            // Try to demote to streaming tier first
            if (m_activeStreamingVoices.size() < m_maxStreamingVoices) {
                voice->currentTier = VoiceTier::Streaming;
                // Move from RAM to streaming list
                auto it = std::find(m_activeRAMVoices.begin(), m_activeRAMVoices.end(), voice);
                if (it != m_activeRAMVoices.end()) {
                    m_activeRAMVoices.erase(it);
                }
                m_activeStreamingVoices.push_back(voice);
                m_stats.voicesDemoted++;
            } 
            // If streaming is full, demote to virtual
            else {
                voice->currentTier = VoiceTier::Virtual;
                // Move from RAM to virtual list
                auto it = std::find(m_activeRAMVoices.begin(), m_activeRAMVoices.end(), voice);
                if (it != m_activeRAMVoices.end()) {
                    m_activeRAMVoices.erase(it);
                }
                m_activeVirtualVoices.push_back(voice);
                m_stats.voicesDemoted++;
            }
        }
    }
};

// VoiceManager implementation
VoiceManager::VoiceManager(uint32_t maxRAMVoices, uint32_t maxStreamingVoices, uint32_t maxVirtualVoices)
    : m_impl(std::make_unique<Impl>(maxRAMVoices, maxStreamingVoices, maxVirtualVoices)) {
}

VoiceManager::~VoiceManager() = default;

Voice* VoiceManager::allocateVoice(uint8_t note, uint8_t velocity, uint8_t channel,
                                  const SampleLayer* sampleLayer, VoicePriority priority) {
    return m_impl->allocateVoice(note, velocity, channel, sampleLayer, priority);
}

void VoiceManager::deallocateVoice(Voice* voice) {
    m_impl->deallocateVoice(voice);
}

void VoiceManager::deallocateAllVoices() {
    m_impl->deallocateAllVoices();
}

void VoiceManager::noteOff(uint8_t note, uint8_t channel) {
    m_impl->noteOff(note, channel);
}

void VoiceManager::allNotesOff(uint8_t channel) {
    m_impl->allNotesOff(channel);
}

void VoiceManager::processAllTiers(AudioBuffer& buffer) {
    m_impl->processAllTiers(buffer);
}

void VoiceManager::setAllocationStrategy(AllocationStrategy strategy) {
    m_impl->m_strategy = strategy;
}

void VoiceManager::setSampleRate(uint32_t sampleRate) {
    m_impl->m_sampleRate = sampleRate;
}

VoiceStats VoiceManager::getStats() const {
    return m_impl->m_stats;
}

uint32_t VoiceManager::getTotalVoiceCapacity() const {
    return m_impl->m_maxRAMVoices + m_impl->m_maxStreamingVoices + m_impl->m_maxVirtualVoices;
}

uint32_t VoiceManager::getVoiceCountByTier(VoiceTier tier) const {
    switch (tier) {
        case VoiceTier::RAM: return m_impl->m_stats.ramVoices;
        case VoiceTier::Streaming: return m_impl->m_stats.streamingVoices;
        case VoiceTier::Virtual: return m_impl->m_stats.virtualVoices;
    }
    return 0;
}

} // namespace core
} // namespace dawg

