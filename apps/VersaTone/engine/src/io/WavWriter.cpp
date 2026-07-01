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
 * File: WavWriter.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "WavWriter.h"
#include <fstream>
#include <cstdint>
#include <stdexcept>

static void writeUint32LE(std::ofstream& out, uint32_t value) {
    out.put(value & 0xFF);
    out.put((value >> 8) & 0xFF);
    out.put((value >> 16) & 0xFF);
    out.put((value >> 24) & 0xFF);
}

static void writeUint16LE(std::ofstream& out, uint16_t value) {
    out.put(value & 0xFF);
    out.put((value >> 8) & 0xFF);
}

void writeWavFile(const std::string& filename, const AudioBuffer& buffer, unsigned sampleRate) {
    size_t channels = buffer.channels();
    size_t samples = buffer.samples();
    std::ofstream out(filename, std::ios::binary);
    if (!out) throw std::runtime_error("Failed to open file for writing");

    // WAV header
    out.write("RIFF", 4);
    writeUint32LE(out, 36 + samples * channels * 4); // file size - 8
    out.write("WAVE", 4);
    out.write("fmt ", 4);
    writeUint32LE(out, 16); // PCM chunk size
    writeUint16LE(out, 3); // format: 3 = IEEE float
    writeUint16LE(out, static_cast<uint16_t>(channels));
    writeUint32LE(out, sampleRate);
    writeUint32LE(out, sampleRate * channels * 4); // byte rate
    writeUint16LE(out, static_cast<uint16_t>(channels * 4)); // block align
    writeUint16LE(out, 32); // bits per sample
    out.write("data", 4);
    writeUint32LE(out, samples * channels * 4);

    // Write samples (interleaved)
    for (size_t i = 0; i < samples; ++i) {
        for (size_t ch = 0; ch < channels; ++ch) {
            float s = static_cast<float>(buffer.sample(ch, i));
            out.write(reinterpret_cast<const char*>(&s), sizeof(float));
        }
    }
    if (!out) throw std::runtime_error("Failed to write WAV data");
}

