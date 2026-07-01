#include "audio/DAWGAudioEngine.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace dawg::audio;

/**
 * @brief Example showing how to use the sophisticated audio engine
 * This demonstrates complete UI independence
 */
int main() {
    std::cout << "DAWG Audio Engine - Standalone Example\n";
    std::cout << "=====================================\n\n";
    
    // Create and configure the audio engine
    auto engine = AudioEngineFactory::createStepSequencerEngine();
    
    if (!engine) {
        std::cerr << "Failed to create audio engine\n";
        return -1;
    }
    
    // Initialize with specific format
    AudioFormat format;
    format.sampleRate = 44100;
    format.channels = 2;
    format.bitsPerSample = 32;
    format.bufferSize = 512;
    
    if (!engine->initialize(format)) {
        std::cerr << "Failed to initialize audio engine\n";
        return -1;
    }
    
    // Set up step callback to show playback progress
    engine->setStepCallback([](uint32_t step) {
        std::cout << "Step: " << step << "\r" << std::flush;
    });
    
    // Set up transport callback
    engine->setTransportCallback([](Transport::PlayState state) {
        switch (state) {
            case Transport::PlayState::Playing:
                std::cout << "\n[TRANSPORT] Playing\n";
                break;
            case Transport::PlayState::Stopped:
                std::cout << "\n[TRANSPORT] Stopped\n";
                break;
            case Transport::PlayState::Paused:
                std::cout << "\n[TRANSPORT] Paused\n";
                break;
            case Transport::PlayState::Recording:
                std::cout << "\n[TRANSPORT] Recording\n";
                break;
        }
    });
    
    // Create and configure a sample instrument
    auto* kickInstrument = engine->createSampleInstrument("Kick");
    if (kickInstrument) {
        // In a real application, you would load actual sample files
        std::cout << "Created kick instrument\n";
        
        // Load sample to pattern track 0 (kick track)
        // engine->loadSampleToTrack(0, "path/to/kick.wav");
    }
    
    // Program a simple 4/4 kick pattern
    std::cout << "Programming kick pattern...\n";
    engine->setStep(0, 0, true, 127);   // Beat 1
    engine->setStep(0, 4, true, 100);   // Beat 2
    engine->setStep(0, 8, true, 127);   // Beat 3
    engine->setStep(0, 12, true, 100);  // Beat 4
    
    // Set tempo
    engine->setTempo(120.0);
    
    // Configure mixer
    engine->setChannelVolume(0, 0.8f);  // Kick channel volume
    engine->setChannelPan(0, 0.0f);     // Center pan
    
    // Start the audio engine
    if (!engine->start()) {
        std::cerr << "Failed to start audio engine\n";
        return -1;
    }
    
    std::cout << "Audio engine started successfully!\n";
    std::cout << "Format: " << format.sampleRate << "Hz, " 
              << format.channels << " channels, " 
              << format.bufferSize << " samples\n\n";
    
    // Start playback
    std::cout << "Starting pattern playback...\n";
    engine->playPattern();
    
    // Let it play for a few seconds
    std::cout << "Playing for 10 seconds...\n";
    std::this_thread::sleep_for(std::chrono::seconds(10));
    
    // Show performance stats
    auto stats = engine->getPerformanceStats();
    std::cout << "\n\nPerformance Stats:\n";
    std::cout << "CPU Usage: " << (stats.cpuUsage * 100.0) << "%\n";
    std::cout << "Buffer Underruns: " << stats.bufferUnderruns << "\n";
    std::cout << "Buffer Overruns: " << stats.bufferOverruns << "\n";
    std::cout << "Average Latency: " << stats.averageLatency << "ms\n";
    
    // Stop playback
    std::cout << "\nStopping playback...\n";
    engine->stopPattern();
    
    // Demonstrate timeline player
    std::cout << "\nDemonstrating timeline player...\n";
    // engine->loadAudioClip("path/to/audio.wav", 0); // Load at time 0
    
    // Switch to timeline mode
    engine->getTransport().play();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    engine->getTransport().stop();
    
    // Clean shutdown
    std::cout << "\nShutting down audio engine...\n";
    engine->stop();
    engine->shutdown();
    
    std::cout << "Audio engine shutdown complete.\n";
    std::cout << "\nThis example shows:\n";
    std::cout << "1. Complete UI independence\n";
    std::cout << "2. Professional audio engine architecture\n";
    std::cout << "3. Modular design with separate concerns\n";
    std::cout << "4. Real-time audio processing\n";
    std::cout << "5. Performance monitoring\n";
    std::cout << "6. Clean resource management\n";
    
    return 0;
}

/**
 * @brief Example of extending the engine with custom processors
 */
class CustomFilter : public AudioProcessor {
public:
    void process(AudioBuffer& buffer) override {
        // Custom audio processing here
        // This could be any DSP algorithm
        for (uint16_t ch = 0; ch < buffer.getChannelCount(); ++ch) {
            for (uint32_t i = 0; i < buffer.getSampleCount(); ++i) {
                float sample = buffer.sample(ch, i);
                // Apply some filtering
                sample *= m_gain;
                buffer.sample(ch, i) = sample;
            }
        }
    }
    
    void prepare(const AudioFormat& format) override {
        m_sampleRate = format.sampleRate;
    }
    
    void reset() override {
        // Reset filter state
    }
    
    void setGain(float gain) { m_gain = gain; }
    
private:
    float m_gain = 1.0f;
    uint32_t m_sampleRate = 44100;
};

/**
 * @brief Example of creating a custom audio device
 */
class FileAudioDevice : public AudioDevice {
    // This could write audio to a file instead of speakers
    // Useful for rendering/bouncing audio offline
};

/**
 * @brief Example configuration for different use cases
 */
void demonstrateConfigurations() {
    // Configuration for live performance
    AudioEngineConfig liveConfig;
    liveConfig.format.bufferSize = 128;  // Low latency
    liveConfig.enablePatternSequencer = true;
    liveConfig.enableTimelinePlayer = false;
    liveConfig.patternTracks = 8;
    
    // Configuration for studio production
    AudioEngineConfig studioConfig;
    studioConfig.format.bufferSize = 1024;  // Higher latency, more CPU headroom
    studioConfig.enablePatternSequencer = true;
    studioConfig.enableTimelinePlayer = true;
    studioConfig.mixerChannels = 64;
    studioConfig.enableEffects = true;
    
    // Configuration for headless rendering
    AudioEngineConfig renderConfig;
    renderConfig.format.bufferSize = 4096;  // Maximum efficiency
    renderConfig.enablePatternSequencer = true;
    renderConfig.enableTimelinePlayer = true;
    renderConfig.preferredDevice = "file_output";
}
