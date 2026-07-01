/*
 * ██████╗  █████╗ ██╗    ██╗ ██████╗ 
 * ██╔══██╗██╔══██╗██║    ██║██╔════╝ 
 * ██║  ██║███████║██║ █╗ ██║██║  ███╗
 * ██║  ██║██╔══██║██║███╗██║██║   ██║
 * ██████╔╝██║  ██║╚███╔███╔╝╚██████╔╝
 * ╚═════╝ ╚═╝  ╚═╝ ╚══╝╚══╝  ╚═════╝ 
 * 
 * Digital Audio Workstation Engine
 * High-Performance C++ Audio Processing Framework
 * 
 * Copyright (c) 2025 The No-Hands Company
 * Licensed under MIT License
 * 
 * File: RingModulator.cpp
 * Purpose: Implementation of ring modulation effect with DSP processing
 * Author: DAWg Development Team
 * Created: 2025-08-14
 */

#include "dawg/dsp/RingModulator.h"
#include <algorithm>
#include <cstring>

namespace dawg {
namespace dsp {

constexpr float TWO_PI = 2.0f * 3.14159265359f;
constexpr float PI = 3.14159265359f;
constexpr float EPSILON = 1e-8f;
constexpr float DC_BLOCK_COEFF = 0.995f;  // High-pass cutoff ~3Hz at 44.1kHz

RingModulator::RingModulator() 
    : RingModulator(44100.0f) {
}

RingModulator::RingModulator(float sampleRate)
    : sampleRate_(sampleRate)
    , carrierPhase_(0.0f)
    , phaseIncrement_(0.0f)
    , dcBlockX1_(0.0f), dcBlockY1_(0.0f)
    , dcBlockX2_(0.0f), dcBlockY2_(0.0f)
    , noiseState_(1) {
    
    updatePhaseIncrement();
}

void RingModulator::setSampleRate(float sampleRate) {
    sampleRate_ = std::max(sampleRate, 1.0f);
    updatePhaseIncrement();
    reset();
}

void RingModulator::reset() {
    carrierPhase_ = 0.0f;
    dcBlockX1_ = dcBlockY1_ = 0.0f;
    dcBlockX2_ = dcBlockY2_ = 0.0f;
    noiseState_ = 1;
}

void RingModulator::setParameters(const Parameters& params) {
    params_ = params;
    
    // Clamp parameters to valid ranges
    params_.frequency = std::clamp(params_.frequency, 20.0f, 2000.0f);
    params_.amplitude = std::clamp(params_.amplitude, 0.0f, 1.0f);
    params_.mix = std::clamp(params_.mix, 0.0f, 1.0f);
    params_.phaseOffset = std::fmod(params_.phaseOffset, 360.0f);
    params_.fineTune = std::clamp(params_.fineTune, -100.0f, 100.0f);
    
    updatePhaseIncrement();
}

void RingModulator::setFrequency(float frequency) {
    params_.frequency = std::clamp(frequency, 20.0f, 2000.0f);
    updatePhaseIncrement();
}

void RingModulator::setAmplitude(float amplitude) {
    params_.amplitude = std::clamp(amplitude, 0.0f, 1.0f);
}

void RingModulator::setMix(float mix) {
    params_.mix = std::clamp(mix, 0.0f, 1.0f);
}

void RingModulator::setWaveType(WaveType type) {
    params_.waveType = type;
}

void RingModulator::setPhaseOffset(float degrees) {
    params_.phaseOffset = std::fmod(degrees, 360.0f);
}

void RingModulator::setDCBlock(bool enable) {
    params_.dcBlock = enable;
}

void RingModulator::setFineTune(float cents) {
    params_.fineTune = std::clamp(cents, -100.0f, 100.0f);
    updatePhaseIncrement();
}

void RingModulator::updatePhaseIncrement() {
    float actualFreq = params_.frequency * centsToRatio(params_.fineTune);
    phaseIncrement_ = TWO_PI * actualFreq / sampleRate_;
}

void RingModulator::process(const float* input, float* output, size_t numSamples) {
    for (size_t i = 0; i < numSamples; ++i) {
        // Generate carrier wave
        float carrier = generateCarrierSample() * params_.amplitude;
        
        // Ring modulate
        float modulated = input[i] * carrier;
        
        // Apply DC blocking if enabled
        if (params_.dcBlock) {
            modulated = applyDCBlock(modulated, dcBlockX1_, dcBlockY1_);
        }
        
        // Mix dry and wet signals
        output[i] = input[i] * (1.0f - params_.mix) + modulated * params_.mix;
        
        // Advance carrier phase
        carrierPhase_ += phaseIncrement_;
        if (carrierPhase_ >= TWO_PI) {
            carrierPhase_ -= TWO_PI;
        }
    }
}

void RingModulator::processStereo(const float* inputL, const float* inputR,
                                 float* outputL, float* outputR, size_t numSamples) {
    float phaseOffsetRad = params_.phaseOffset * PI / 180.0f;
    
    for (size_t i = 0; i < numSamples; ++i) {
        // Generate carrier waves with phase offset for stereo
        float carrierL = generateCarrierSample() * params_.amplitude;
        
        // Calculate right channel phase
        float rightPhase = carrierPhase_ + phaseOffsetRad;
        if (rightPhase >= TWO_PI) rightPhase -= TWO_PI;
        
        float carrierR;
        switch (params_.waveType) {
            case WaveType::SINE:
                carrierR = generateSine(rightPhase) * params_.amplitude;
                break;
            case WaveType::SQUARE:
                carrierR = generateSquare(rightPhase) * params_.amplitude;
                break;
            case WaveType::TRIANGLE:
                carrierR = generateTriangle(rightPhase) * params_.amplitude;
                break;
            case WaveType::SAWTOOTH:
                carrierR = generateSawtooth(rightPhase) * params_.amplitude;
                break;
            case WaveType::NOISE:
                carrierR = generateNoise() * params_.amplitude;
                break;
        }
        
        // Ring modulate both channels
        float modulatedL = inputL[i] * carrierL;
        float modulatedR = inputR[i] * carrierR;
        
        // Apply DC blocking if enabled
        if (params_.dcBlock) {
            modulatedL = applyDCBlock(modulatedL, dcBlockX1_, dcBlockY1_);
            modulatedR = applyDCBlock(modulatedR, dcBlockX2_, dcBlockY2_);
        }
        
        // Mix dry and wet signals
        outputL[i] = inputL[i] * (1.0f - params_.mix) + modulatedL * params_.mix;
        outputR[i] = inputR[i] * (1.0f - params_.mix) + modulatedR * params_.mix;
        
        // Advance carrier phase
        carrierPhase_ += phaseIncrement_;
        if (carrierPhase_ >= TWO_PI) {
            carrierPhase_ -= TWO_PI;
        }
    }
}

float RingModulator::generateCarrierSample() {
    switch (params_.waveType) {
        case WaveType::SINE:
            return generateSine(carrierPhase_);
        case WaveType::SQUARE:
            return generateSquare(carrierPhase_);
        case WaveType::TRIANGLE:
            return generateTriangle(carrierPhase_);
        case WaveType::SAWTOOTH:
            return generateSawtooth(carrierPhase_);
        case WaveType::NOISE:
            return generateNoise();
        default:
            return generateSine(carrierPhase_);
    }
}

float RingModulator::generateSine(float phase) {
    return std::sin(phase);
}

float RingModulator::generateSquare(float phase) {
    return (phase < PI) ? 1.0f : -1.0f;
}

float RingModulator::generateTriangle(float phase) {
    if (phase < PI) {
        return (2.0f * phase / PI) - 1.0f;
    } else {
        return 3.0f - (2.0f * phase / PI);
    }
}

float RingModulator::generateSawtooth(float phase) {
    return (2.0f * phase / TWO_PI) - 1.0f;
}

float RingModulator::generateNoise() {
    // Simple linear congruential generator for white noise
    noiseState_ = noiseState_ * 1103515245 + 12345;
    return (static_cast<float>(noiseState_ & 0x7FFFFFFF) / static_cast<float>(0x7FFFFFFF)) * 2.0f - 1.0f;
}

float RingModulator::applyDCBlock(float input, float& x1, float& y1) {
    // Simple DC blocking high-pass filter: y[n] = x[n] - x[n-1] + 0.995 * y[n-1]
    float output = input - x1 + DC_BLOCK_COEFF * y1;
    x1 = input;
    y1 = output;
    return output;
}

float RingModulator::centsToRatio(float cents) {
    return std::pow(2.0f, cents / 1200.0f);
}

float RingModulator::getCarrierAmplitude() const {
    return std::abs(generateCarrierSample()) * params_.amplitude;
}

} // namespace dsp
} // namespace dawg
