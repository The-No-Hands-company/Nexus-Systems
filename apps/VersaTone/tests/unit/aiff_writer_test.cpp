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
 * File: aiff_writer_test.cpp
 * Purpose: Unit tests for audio processing functionality
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */

#include "AiffWriter.h"
#include "AudioBuffer.h"
#include <iostream>
#include <cmath>

int main() {
    // Create a 1-second stereo 440Hz sine wave
    size_t sampleRate = 44100;
    size_t channels = 2;
    size_t samples = sampleRate;
    AudioBuffer buffer(channels, samples);
    for (size_t i = 0; i < samples; ++i) {
        double t = i / static_cast<double>(sampleRate);
        double s = std::sin(2.0 * 3.141592653589793 * 440.0 * t);
        buffer.sample(0, i) = s; // Left
        buffer.sample(1, i) = s; // Right
    }
    // Export to AIFF
    if (dawg::exportAiff("test_out.aiff", buffer)) {
        std::cout << "Exported test_out.aiff successfully!\n";
    } else {
        std::cout << "Failed to export AIFF.\n";
    }
    // Import back
    AudioBuffer loaded(2, 1);
    if (dawg::importAiff("test_out.aiff", loaded)) {
        std::cout << "Imported test_out.aiff successfully!\n";
        std::cout << "First sample left: " << loaded.sample(0, 0) << "\n";
    } else {
        std::cout << "Failed to import AIFF.\n";
    }
    return 0;
}

