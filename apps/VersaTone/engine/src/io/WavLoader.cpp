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
 * File: WavLoader.cpp
 * Purpose: Audio file loading and import functionality
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "WavLoader.h"
#include <fstream>
#include <cstring>

bool WavLoader::load(const std::string& filename, AudioBuffer& buffer, uint32_t& sampleRate) {
    std::ifstream f(filename, std::ios::binary);
    if (!f) return false;
    char riff[4]; f.read(riff, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0) return false;
    f.seekg(22); // Num channels
    uint16_t channels; f.read((char*)&channels, 2);
    f.read((char*)&sampleRate, 4);
    f.seekg(34); // Bits per sample
    uint16_t bits; f.read((char*)&bits, 2);
    f.seekg(40); // Data chunk size
    uint32_t dataSize; f.read((char*)&dataSize, 4);
    std::vector<char> data(dataSize); f.read(data.data(), dataSize);
    size_t samples = dataSize / (channels * (bits/8));
    buffer = AudioBuffer(channels, samples);
    for (size_t i = 0; i < samples; ++i) {
        for (size_t ch = 0; ch < channels; ++ch) {
            size_t offset = (i * channels + ch) * (bits/8);
            int32_t sample = 0;
            if (bits == 16) sample = *(int16_t*)&data[offset];
            else if (bits == 24) sample = (int32_t)((data[offset+2]<<16)|(data[offset+1]<<8)|data[offset]);
            else if (bits == 32) sample = *(int32_t*)&data[offset];
            buffer.sample(ch, i) = sample / (double)(1 << (bits-1));
        }
    }
    return true;
}

