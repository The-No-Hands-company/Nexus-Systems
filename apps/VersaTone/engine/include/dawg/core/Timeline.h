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
 * File: Timeline.h
 * Purpose: Header file for audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */
#pragma once
#include <vector>
#include <memory>
#include <string>
#include <cstdint>

namespace dawg {
namespace core {

// Forward declarations
class AudioClip;
class MidiClip;

// Base class for timeline clips
class Clip {
public:
    double startTime = 0.0; // seconds or beats
    double length = 0.0;    // seconds or beats
    virtual ~Clip() = default;
    virtual std::string type() const = 0;
};

class AudioClip : public Clip {
public:
    std::string audioFile;
    double fadeIn = 0.0;   // seconds
    double fadeOut = 0.0;  // seconds
    double crossfade = 0.0; // seconds
    double slip = 0.0;     // offset in source file
    std::vector<std::string> takes; // comping: alternate takes
    int activeTake = 0;
    std::string type() const override { return "audio"; }
};

struct MidiEvent {
    double time; // seconds or beats
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
};

class MidiClip : public Clip {
public:
    std::string midiFile;
    std::vector<MidiEvent> events;
    std::string type() const override { return "midi"; }
    // Editing API
    void addEvent(const MidiEvent& ev) { events.push_back(ev); }
    void removeEvent(size_t idx) { if (idx < events.size()) events.erase(events.begin() + idx); }
};

// Timeline/arrangement
class Timeline {
public:
    std::vector<std::shared_ptr<Clip>> clips;
    double loopStart = 0.0;
    double loopEnd = 0.0;
    double tempo = 120.0;
    int timeSigNum = 4, timeSigDen = 4;
    void addClip(const std::shared_ptr<Clip>& clip) { clips.push_back(clip); }
    void removeClip(const std::shared_ptr<Clip>& clip);
};

} // namespace core
} // namespace dawg

