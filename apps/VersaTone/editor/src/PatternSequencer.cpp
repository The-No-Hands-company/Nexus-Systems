/*
 * â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•—  â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•— â–ˆâ–ˆâ•—    â–ˆâ–ˆâ•— â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•— 
 * â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘    â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•”â•â•â•â•â• 
 * â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘ â–ˆâ•— â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ–ˆâ•—
 * â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•”â•â•â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘â–ˆâ–ˆâ–ˆâ•—â–ˆâ–ˆâ•‘â–ˆâ–ˆâ•‘   â–ˆâ–ˆâ•‘
 * â–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•â–ˆâ–ˆâ•‘  â–ˆâ–ˆâ•‘â•šâ–ˆâ–ˆâ–ˆâ•”â–ˆâ–ˆâ–ˆâ•”â•šâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ–ˆâ•”â•
 * â•šâ•â•â•â•â•â• â•šâ•â•  â•šâ•â• â•šâ•â•â•â•šâ•â•â•  â•šâ•â•â•â•â•â• 
 * 
 * Digital Audio Workstation Engine
 * High-Performance C++ Audio Processing Framework
 * 
 * Copyright (c) 2025 The No-Hands Company
 * Licensed under MIT License
 * 
 * File: PatternSequencer.cpp
 * Purpose: Implementation of audio processing component
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */

#include "PatternSequencer.h"

namespace daq {
namespace editor:

// Pattern implementation
Pattern::Pattern(int trackCount, int stepCount)
    : m_trackCount(trackCount)
    , m_stepCount(stepCount)
{
    m_steps.resize(m_trackCount);
    for (int i = 0; i < m_trackCount; ++i) {
        m_steps[i].resize(m_stepCount);
    }
}

Pattern::~Pattern() = default;

const Step& Pattern::getStep(int track, int step) const {
    if (track >= 0 && track < m_trackCount && step >= 0 && step < m_stepCount) {
        return m_steps[track][step];
    }
    // Return a default-constructed step if out of bounds
    static Step defaultStep;
    return defaultStep;
}

Step& Pattern::getStep(int track, int step) {
    if (track >= 0 && track < m_trackCount && step >= 0 && step < m_stepCount) {
        return m_steps[track][step];
    }
    // Return a static reference to a default-constructed step if out of bounds
    // Note: This is not ideal but prevents crashes; better to throw an exception or use assert
    static Step defaultStep;
    return defaultStep;
}

void Pattern::setStep(int track, int step, const Step& stepValue) {
    if (track >= 0 && track < m_trackCount && step >= 0 && step < m_stepCount) {
        m_steps[track][step] = stepValue;
    }
}

void Pattern::setTrackCount(int trackCount) {
    if (trackCount <= 0) return;
    
    // Resize the outer vector
    m_steps.resize(trackCount);
    m_trackCount = trackCount;
    
    // Resize each inner vector to match step count
    for (int i = 0; i < m_trackCount; ++i) {
        if (static_cast<int>(m_steps[i].size()) < m_stepCount) {
            m_steps[i].resize(m_stepCount);
        }
    }
}

void Pattern::setStepCount(int stepCount) {
    if (stepCount <= 0) return;
    
    // Resize each inner vector
    m_stepCount = stepCount;
    for (int i = 0; i < m_trackCount; ++i) {
        m_steps[i].resize(m_stepCount);
    }
}

void Pattern::clear() {
    for (int i = 0; i < m_trackCount; ++i) {
        for (int j = 0; j < m_stepCount; ++j) {
            m_steps[i][j] = Step(); // Reset to default (disabled)
        }
    }
}

// PatternSequencer implementation
PatternSequencer::PatternSequencer() 
    : m_pattern(std::make_shared<Pattern>()) {
}

PatternSequencer::~PatternSequencer() = default;

void PatternSequencer::setTempo(float bpm) {
    m_tempo = bpm;
}

float PatternSequencer::getTempo() const {
    return m_tempo;
}

void PatternSequencer::setTimeSignature(int numerator, int denominator) {
    m_timeSignatureNumerator = numerator;
    m_timeSignatureDenominator = denominator;
}

void PatternSequencer::getTimeSignature(int& numerator, int& denominator) const {
    numerator = m_timeSignatureNumerator;
    denominator = m_timeSignatureDenominator;
}

void PatternSequencer::setPatternLength(int steps) {
    m_patternLength = steps;
    // Note: This doesn't actually change the pattern size, just the logical length for playback
    // The actual pattern size is determined by the Pattern object's step count
}

int PatternSequencer::getPatternLength() const {
    return m_patternLength;
}

void PatternSequencer::setStepEnabled(int track, int step, bool enabled) {
    if (m_pattern) {
        Step& stepRef = m_pattern->getStep(track, step);
        stepRef.enabled = enabled;
    }
}

bool PatternSequencer::isStepEnabled(int track, int step) const {
    if (m_pattern) {
        const Step& stepRef = m_pattern->getStep(track, step);
        return stepRef.enabled;
    }
    return false;
}

void PatternSequencer::setPattern(std::shared_ptr<Pattern> pattern) {
    m_pattern = pattern;
}

std::shared_ptr<Pattern> PatternSequencer::getPattern() const {
    return m_pattern;
}

std::shared_ptr<Pattern> PatternSequencer::createPattern(int trackCount, int stepCount) {
    return std::make_shared<Pattern>(trackCount, stepCount);
}

void PatternSequencer::clear() {
    if (m_pattern) {
        m_pattern->clear();
    }
}

} // namespace editor
} // namespace daq