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
 * File: AiffWriter.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "AiffWriter.h"
#include <fstream>
#include <cstdint>
#include <vector>
#include <cstring>
#include <algorithm>

namespace dawg {

// Helper: Write 4-char chunk
static void writeChunk(std::ofstream& out, const char* id, uint32_t size) {
    out.write(id, 4);
    uint32_t be_size = ((size>>24)&0xFF) | ((size>>8)&0xFF00) | ((size<<8)&0xFF0000) | ((size<<24)&0xFF000000);
    out.write(reinterpret_cast<const char*>(&be_size), 4);
}

// Helper: Write 80-bit extended float (AIFF sample rate)
static void writeExtended80(std::ofstream& out, double value) {
    // Minimal: Write 44100.0 as 0x400EAC44000000000000
    // For now, only support 44100.0
    uint8_t ext[10] = {0x40, 0x0E, 0xAC, 0x44, 0,0,0,0,0,0};
    out.write(reinterpret_cast<const char*>(ext), 10);
}

bool exportAiff(const std::string& path, const AudioBuffer& buffer) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    size_t channels = buffer.channels();
    size_t samples = buffer.samples();
    uint32_t sampleFrames = static_cast<uint32_t>(samples);
    uint16_t bitsPerSample = 16;
    uint32_t commChunkSize = 18;
    uint32_t ssndChunkSize = 8 + sampleFrames * channels * 2;
    uint32_t formSize = 4 + (8+commChunkSize) + (8+ssndChunkSize);

    // FORM chunk
    out.write("FORM", 4);
    uint32_t be_formSize = ((formSize>>24)&0xFF) | ((formSize>>8)&0xFF00) | ((formSize<<8)&0xFF0000) | ((formSize<<24)&0xFF000000);
    out.write(reinterpret_cast<const char*>(&be_formSize), 4);
    out.write("AIFF", 4);

    // COMM chunk
    writeChunk(out, "COMM", commChunkSize);
    uint16_t be_channels = (channels>>8) | (channels<<8);
    out.write(reinterpret_cast<const char*>(&be_channels), 2);
    uint32_t be_sampleFrames = ((sampleFrames>>24)&0xFF) | ((sampleFrames>>8)&0xFF00) | ((sampleFrames<<8)&0xFF0000) | ((sampleFrames<<24)&0xFF000000);
    out.write(reinterpret_cast<const char*>(&be_sampleFrames), 4);
    uint16_t be_bits = (bitsPerSample>>8) | (bitsPerSample<<8);
    out.write(reinterpret_cast<const char*>(&be_bits), 2);
    writeExtended80(out, 44100.0);

    // SSND chunk
    writeChunk(out, "SSND", ssndChunkSize);
    uint32_t zero = 0;
    out.write(reinterpret_cast<const char*>(&zero), 4); // offset
    out.write(reinterpret_cast<const char*>(&zero), 4); // block size
    // Write samples (interleaved)
    for (size_t i = 0; i < samples; ++i) {
        for (size_t ch = 0; ch < channels; ++ch) {
            double s = buffer.sample(ch, i);
            int16_t v = static_cast<int16_t>(std::max(-1.0, std::min(1.0, s)) * 32767.0);
            uint16_t be_v = (v>>8) | (v<<8);
            out.write(reinterpret_cast<const char*>(&be_v), 2);
        }
    }
    return true;
}


// Minimal AIFF PCM16 import (mono/stereo, 44100Hz, 16-bit)
bool importAiff(const std::string& path, AudioBuffer& buffer) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    char form[4]; in.read(form, 4);
    if (std::strncmp(form, "FORM", 4) != 0) return false;
    in.ignore(4); // size
    char aiff[4]; in.read(aiff, 4);
    if (std::strncmp(aiff, "AIFF", 4) != 0) return false;
    // Find COMM and SSND chunks
    uint16_t channels = 0, bits = 0;
    uint32_t frames = 0;
    std::streampos ssndPos = 0;
    uint32_t ssndSize = 0;
    while (in) {
        char chunkId[4]; in.read(chunkId, 4);
        if (in.gcount() < 4) break;
        uint32_t chunkSize = 0;
        in.read(reinterpret_cast<char*>(&chunkSize), 4);
        chunkSize = ((chunkSize>>24)&0xFF) | ((chunkSize>>8)&0xFF00) | ((chunkSize<<8)&0xFF0000) | ((chunkSize<<24)&0xFF000000);
        if (std::strncmp(chunkId, "COMM", 4) == 0) {
            uint16_t be_channels; in.read(reinterpret_cast<char*>(&be_channels), 2);
            channels = (be_channels>>8) | (be_channels<<8);
            in.read(reinterpret_cast<char*>(&frames), 4);
            frames = ((frames>>24)&0xFF) | ((frames>>8)&0xFF00) | ((frames<<8)&0xFF0000) | ((frames<<24)&0xFF000000);
            uint16_t be_bits; in.read(reinterpret_cast<char*>(&be_bits), 2);
            bits = (be_bits>>8) | (be_bits<<8);
            in.ignore(10); // sample rate
        } else if (std::strncmp(chunkId, "SSND", 4) == 0) {
            ssndPos = in.tellg();
            ssndSize = chunkSize;
            break;
        } else {
            in.ignore(chunkSize + (chunkSize&1));
        }
    }
    if (channels == 0 || bits != 16 || ssndPos == 0) return false;
    buffer = AudioBuffer(channels, frames);
    in.seekg(ssndPos);
    uint32_t offset = 0, blockSize = 0;
    in.read(reinterpret_cast<char*>(&offset), 4);
    in.read(reinterpret_cast<char*>(&blockSize), 4);
    for (uint32_t i = 0; i < frames; ++i) {
        for (uint16_t ch = 0; ch < channels; ++ch) {
            uint16_t be_v = 0;
            in.read(reinterpret_cast<char*>(&be_v), 2);
            int16_t v = (be_v>>8) | (be_v<<8);
            buffer.sample(ch, i) = v / 32768.0;
        }
    }
    return true;
}

} // namespace dawg


