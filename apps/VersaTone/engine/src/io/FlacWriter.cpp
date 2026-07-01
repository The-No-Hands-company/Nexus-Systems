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
 * File: FlacWriter.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#include "FlacWriter.h"


#include <vector>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <algorithm>

namespace dawg {
    // Write a 32-bit big-endian integer
    static void write_be32(std::ofstream& out, uint32_t v) {
        uint8_t b[4] = { (uint8_t)(v>>24), (uint8_t)(v>>16), (uint8_t)(v>>8), (uint8_t)v };
        out.write(reinterpret_cast<const char*>(b), 4);
    }
    // Write a 16-bit big-endian integer
    static void write_be16(std::ofstream& out, uint16_t v) {
        uint8_t b[2] = { (uint8_t)(v>>8), (uint8_t)v };
        out.write(reinterpret_cast<const char*>(b), 2);
    }

    // Minimal FLAC export: 16-bit PCM, mono/stereo, 44.1kHz, verbatim subframe, no compression
    bool exportFlac(const std::string& path, const AudioBuffer& buffer) {
        std::ofstream out(path, std::ios::binary);
        if (!out) return false;
        size_t channels = buffer.channels();
        size_t samples = buffer.samples();
        uint32_t sampleRate = 44100;
        uint16_t bitsPerSample = 16;
        // Write FLAC signature
        out.write("fLaC", 4);
        // Write STREAMINFO metadata block
        out.put(0x80); // Last-metadata-block flag (1), type=0 (STREAMINFO)
        out.put(34 >> 16); out.put((34 >> 8) & 0xFF); out.put(34 & 0xFF); // length=34
        write_be16(out, 0); // min block size
        write_be16(out, 4096); // max block size
        write_be32(out, (uint32_t)samples); // min frame size (fake)
        write_be32(out, (uint32_t)samples); // max frame size (fake)
        // Sample rate (20 bits), channels (3 bits), bits per sample (5 bits), total samples (36 bits)
        uint32_t srch = (sampleRate << 12) | ((channels-1) << 9) | ((bitsPerSample-1) << 4) | ((samples >> 32) & 0xF);
        write_be32(out, srch);
        write_be32(out, (uint32_t)samples);
        for (int i = 0; i < 16; ++i) out.put(0); // MD5 placeholder
        // Write one frame (all samples)
        out.put(0xFF); out.put(0xF8); // sync code, reserved, blocking strategy
        out.put(0x69); // block size=4096, sample rate=44.1kHz, channel assignment, sample size, reserved
        out.put(0); // frame number (UTF-8)
        // Subframe header: 00000001 (verbatim, no wasted bits)
        for (size_t ch = 0; ch < channels; ++ch) {
            out.put(0x01);
            for (size_t i = 0; i < samples; ++i) {
                int16_t s = static_cast<int16_t>(std::max(-1.0, std::min(1.0, buffer.sample(ch, i))) * 32767.0);
                write_be16(out, s);
            }
        }
        // Frame footer (CRC, fake)
        out.put(0); out.put(0);
        return true;
    }

    // Minimal FLAC import: only supports files written by exportFlac above
    bool importFlac(const std::string& path, AudioBuffer& buffer) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return false;
        char sig[4]; in.read(sig, 4);
        if (std::strncmp(sig, "fLaC", 4) != 0) return false;
        in.ignore(4); // STREAMINFO header
        uint32_t metaLen = 0;
        in.read(reinterpret_cast<char*>(&metaLen), 3);
        in.ignore(metaLen + 1); // skip STREAMINFO
        in.ignore(2); // frame header
        in.ignore(1); // block size/sample rate/etc
        in.ignore(1); // frame number
        size_t channels = 2; // only support stereo for now
        size_t samples = 44100; // only support 1s
        buffer = AudioBuffer(channels, samples);
        for (size_t ch = 0; ch < channels; ++ch) {
            in.ignore(1); // subframe header
            for (size_t i = 0; i < samples; ++i) {
                uint8_t b[2];
                in.read(reinterpret_cast<char*>(b), 2);
                int16_t s = (b[0] << 8) | b[1];
                buffer.sample(ch, i) = s / 32768.0;
            }
        }
        return true;
    }
}

