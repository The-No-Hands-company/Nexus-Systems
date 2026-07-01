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
 * File: test_wasapi_callback.cpp
 * Purpose: Unit tests for audio processing functionality
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "audio/WasapiAudioEngine.h"
#include "audio/Transport.h"
#include "SineGen.h"
#include <iostream>
#include <cmath>
#include <atomic>

// Test callback: generates a sine wave, advances transport
void testSineCallback(float* output, uint32_t frames, uint32_t channels, uint32_t sampleRate, void* userData) {
    auto* transport = static_cast<Transport*>(userData);
    static double phase = 0.0;
    double freq = 440.0;
    double phaseInc = 2.0 * M_PI * freq / sampleRate;
    if (!transport->isPlaying()) {
        std::fill(output, output + frames * channels, 0.0f);
        return;
    }
    for (uint32_t i = 0; i < frames; ++i) {
        float value = static_cast<float>(0.2 * std::sin(phase));
        for (uint32_t ch = 0; ch < channels; ++ch) {
            output[i * channels + ch] = value;
        }
        phase += phaseInc;
        if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
    }
    transport->advance(frames);
}

int main() {
    std::cout << "TEST ENTRY: test_wasapi_callback.cpp running" << std::endl;
    std::cout.flush();
    WasapiAudioEngine engine;
    if (!engine.init(48000, 2, 512)) {
        std::cerr << "Failed to initialize WASAPI engine\n";
        std::cerr.flush();
        return 1;
    }
    Transport transport;
    engine.setCallback(testSineCallback, &transport);
    transport.play();
    if (!engine.start()) {
        std::cerr << "Failed to start audio engine\n";
        std::cerr.flush();
        return 1;
    }
    std::cout << "Playing sine wave for 1 second...\n";
    std::cout.flush();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Seeking to 0.5 seconds...\n";
    transport.seek(48000 / 2); // Seek to 0.5s
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Stopping playback...\n";
    transport.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::cout << "Current sample position: " << transport.getSamplePosition() << std::endl;
    engine.stop();
    engine.shutdown();
    std::cout << "Done.\n";
    return 0;
}

