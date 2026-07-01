/*
   _____       __          __    _____                                      
  / ____ \    /\ \        /\ \  / ___/_______________________________     
 / /\_/\ \    \ \ \  _   / / / / /   ___________________/\____________/\    
 \ \/ \ \ \    \ \ \_/\  / / /  \ \  \____________/\____\/___     ___\/    
  \  __\ \ \____\ \____/ / /    \ \_______      \ \___\_____\   \_____    
   \_\ \ \ ____/\_______/ /      \/_____/_______/_/_____________/_____/    
      \_\/__/  \/_______/                                              
                                                                            
██████╗  █████╗ ██╗    ██╗ ██████╗       ███████╗███╗   ██╗ ██████╗ 
██╔══██╗██╔══██╗██║    ██║██╔════╝       ██╔════╝████╗  ██║██╔════╝ 
██║  ██║███████║██║ █╗ ██║██║  ███╗█████╗█████╗  ██╔██╗ ██║██║  ███╗
██║  ██║██╔══██║██║███╗██║██║   ██║╚════╝██╔══╝  ██║╚██╗██║██║   ██║
██████╔╝██║  ██║╚███╔███╔╝╚██████╔╝      ███████╗██║ ╚████║╚██████╔╝
╚═════╝ ╚═╝  ╚═╝ ╚══╝╚══╝  ╚═════╝       ╚══════╝╚═╝  ╚═══╝ ╚═════╝ 
                                                                      
THE NO-HANDS COMPANY: Automated Excellence in Digital Audio Workstations

Effect: Gate
Category: dynamics
File: dawg/dsp/dynamics/Gate.cpp
Purpose: Professional noise gate with adjustable threshold and timing

Created: 2025-08-14
License: Private - All rights reserved
*/

#include "dawg/dsp/dynamics/Gate.h"
#include "dawg/core/AudioBuffer.h"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace dawg {
namespace dsp {
namespace dynamics {

// ============================================================================
// CONSTRUCTION/DESTRUCTION
// ============================================================================

Gate::Gate() {
    updateCoefficients();
    reset();
}

Gate::~Gate() = default;

// ============================================================================
// EFFECT INTERFACE
// ============================================================================

void Gate::process(core::AudioBuffer& buffer) {
    if (buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0) {
        return;
    }
    
    const size_t numSamples = buffer.getNumSamples();
    const size_t numChannels = buffer.getNumChannels();
    
    // Get audio data
    float* leftChannel = buffer.getChannelData(0);
    float* rightChannel = numChannels > 1 ? buffer.getChannelData(1) : leftChannel;
    
    // Process look-ahead if enabled
    if (m_lookAheadSamples > 0) {
        processLookAhead(leftChannel, rightChannel, numSamples);
        return;
    }
    
    // Process each sample
    for (size_t i = 0; i < numSamples; ++i) {
        // Calculate input level (RMS)
        double inputSquared = (leftChannel[i] * leftChannel[i] + 
                              rightChannel[i] * rightChannel[i]) * 0.5;
        double inputLevel = std::sqrt(inputSquared);
        
        // Update envelope follower
        double envelope = processEnvelope(inputLevel);
        
        // Convert to dB for threshold comparison
        double envelopeDB = envelope > 0.0 ? 20.0 * std::log10(envelope) : -120.0;
        
        // Update metering
        m_inputMeter = std::max(m_inputMeter * 0.99, envelopeDB);
        
        // Determine if gate should be open
        bool shouldOpen = envelopeDB > m_threshold;
        
        // Handle hold time
        if (shouldOpen) {
            m_gateOpen = true;
            m_holdCounter = m_holdSamples;
        } else if (m_holdCounter > 0) {
            m_holdCounter--;
            shouldOpen = true; // Keep gate open during hold
        } else {
            m_gateOpen = false;
        }
        
        // Calculate target gain
        double targetGain = shouldOpen ? 1.0 : std::pow(10.0, m_reduction / 20.0);
        
        // Apply knee if specified
        if (m_knee > 0.0) {
            double kneeStart = m_threshold - m_knee * 0.5;
            double kneeEnd = m_threshold + m_knee * 0.5;
            
            if (envelopeDB > kneeStart && envelopeDB < kneeEnd) {
                double kneeRatio = (envelopeDB - kneeStart) / m_knee;
                double openGain = 1.0;
                double closedGain = std::pow(10.0, m_reduction / 20.0);
                targetGain = closedGain + (openGain - closedGain) * kneeRatio;
            }
        }
        
        // Smooth gain changes
        double coeff = targetGain > m_smoothedGain ? m_attackCoeff : m_releaseCoeff;
        m_smoothedGain += (targetGain - m_smoothedGain) * coeff;
        
        // Apply gain
        leftChannel[i] *= static_cast<float>(m_smoothedGain);
        rightChannel[i] *= static_cast<float>(m_smoothedGain);
        
        // Update reduction metering
        double reductionDB = 20.0 * std::log10(m_smoothedGain);
        m_reductionMeter = std::min(m_reductionMeter * 0.99, reductionDB);
    }
}

void Gate::reset() {
    m_envelopeFollower = 0.0;
    m_gateState = 0.0;
    m_smoothedGain = 1.0;
    m_holdCounter = 0;
    m_gateOpen = false;
    m_inputMeter = -120.0;
    m_reductionMeter = 0.0;
    
    // Clear delay lines
    std::fill(m_delayLineL.begin(), m_delayLineL.end(), 0.0f);
    std::fill(m_delayLineR.begin(), m_delayLineR.end(), 0.0f);
    m_delayIndex = 0;
}

void Gate::setSampleRate(uint32_t sampleRate) {
    m_sampleRate = sampleRate;
    updateCoefficients();
    
    // Resize delay lines for look-ahead
    size_t maxDelaySize = static_cast<size_t>(m_sampleRate * 0.02); // 20ms max
    m_delayLineL.resize(maxDelaySize, 0.0f);
    m_delayLineR.resize(maxDelaySize, 0.0f);
    
    reset();
}

void Gate::setBufferSize(uint32_t bufferSize) {
    m_bufferSize = bufferSize;
}

void Gate::setParameter(const std::string& name, double value) {
    if (name == "threshold") {
        setThreshold(value);
    } else if (name == "attack") {
        setAttack(value);
    } else if (name == "release") {
        setRelease(value);
    } else if (name == "hold") {
        setHold(value);
    } else if (name == "knee") {
        setKnee(value);
    } else if (name == "reduction") {
        setReduction(value);
    } else if (name == "lookAhead") {
        setLookAhead(value);
    } else if (name == "gateType") {
        setGateType(static_cast<GateType>(static_cast<int>(value)));
    }
}

double Gate::getParameter(const std::string& name) const {
    if (name == "threshold") return m_threshold;
    if (name == "attack") return m_attack;
    if (name == "release") return m_release;
    if (name == "hold") return m_hold;
    if (name == "knee") return m_knee;
    if (name == "reduction") return m_reduction;
    if (name == "lookAhead") return m_lookAhead;
    if (name == "gateType") return static_cast<double>(static_cast<int>(m_gateType));
    if (name == "inputLevel") return getInputLevel();
    if (name == "gateReduction") return getGateReduction();
    if (name == "isOpen") return isGateOpen() ? 1.0 : 0.0;
    return 0.0;
}

std::vector<std::string> Gate::getParameterNames() const {
    return {
        "threshold", "attack", "release", "hold", "knee", 
        "reduction", "lookAhead", "gateType",
        "inputLevel", "gateReduction", "isOpen"
    };
}

uint32_t Gate::getLatency() const {
    return static_cast<uint32_t>(m_lookAheadSamples);
}

// ============================================================================
// GATE-SPECIFIC CONTROL
// ============================================================================

void Gate::setGateType(GateType type) {
    m_gateType = type;
}

void Gate::setThreshold(double threshold) {
    m_threshold = std::clamp(threshold, -60.0, 0.0);
}

void Gate::setAttack(double attack) {
    m_attack = std::clamp(attack, 0.1, 100.0);
    updateCoefficients();
}

void Gate::setRelease(double release) {
    m_release = std::clamp(release, 1.0, 5000.0);
    updateCoefficients();
}

void Gate::setHold(double hold) {
    m_hold = std::clamp(hold, 0.0, 2000.0);
    m_holdSamples = static_cast<uint32_t>(m_hold * m_sampleRate / 1000.0);
}

void Gate::setKnee(double knee) {
    m_knee = std::clamp(knee, 0.0, 10.0);
}

void Gate::setReduction(double reduction) {
    m_reduction = std::clamp(reduction, -60.0, 0.0);
}

void Gate::setLookAhead(double lookAhead) {
    m_lookAhead = std::clamp(lookAhead, 0.0, 20.0);
    m_lookAheadSamples = static_cast<size_t>(m_lookAhead * m_sampleRate / 1000.0);
    m_lookAheadSamples = std::min(m_lookAheadSamples, m_delayLineL.size());
}

void Gate::setSidechainEnabled(bool enabled) {
    m_sidechainEnabled = enabled;
}

// ============================================================================
// REAL-TIME MONITORING
// ============================================================================

double Gate::getInputLevel() const {
    return m_inputMeter;
}

double Gate::getGateReduction() const {
    return m_reductionMeter;
}

bool Gate::isGateOpen() const {
    return m_gateOpen;
}

// ============================================================================
// INTERNAL METHODS
// ============================================================================

void Gate::updateCoefficients() {
    if (m_sampleRate > 0) {
        // Convert attack/release times to filter coefficients
        m_attackCoeff = 1.0 - std::exp(-1.0 / (m_attack * m_sampleRate / 1000.0));
        m_releaseCoeff = 1.0 - std::exp(-1.0 / (m_release * m_sampleRate / 1000.0));
        
        // Update hold samples
        m_holdSamples = static_cast<uint32_t>(m_hold * m_sampleRate / 1000.0);
    }
}

double Gate::processEnvelope(double input) {
    // Simple envelope follower with attack/release
    double target = std::abs(input);
    double coeff = target > m_envelopeFollower ? m_attackCoeff : m_releaseCoeff;
    m_envelopeFollower += (target - m_envelopeFollower) * coeff;
    return m_envelopeFollower;
}

double Gate::computeGain(double envelope) {
    double envelopeDB = envelope > 0.0 ? 20.0 * std::log10(envelope) : -120.0;
    
    if (envelopeDB > m_threshold) {
        return 1.0; // Gate open
    } else {
        return std::pow(10.0, m_reduction / 20.0); // Gate closed
    }
}

void Gate::processLookAhead(float* left, float* right, size_t numSamples) {
    for (size_t i = 0; i < numSamples; ++i) {
        // Store current samples in delay line
        m_delayLineL[m_delayIndex] = left[i];
        m_delayLineR[m_delayIndex] = right[i];
        
        // Calculate delayed output index
        size_t outputIndex = (m_delayIndex + m_delayLineL.size() - m_lookAheadSamples) % m_delayLineL.size();
        
        // Process the look-ahead sample for gain calculation
        double inputLevel = std::sqrt((left[i] * left[i] + right[i] * right[i]) * 0.5);
        double envelope = processEnvelope(inputLevel);
        double gain = computeGain(envelope);
        
        // Apply smoothed gain to delayed samples
        double coeff = gain > m_smoothedGain ? m_attackCoeff : m_releaseCoeff;
        m_smoothedGain += (gain - m_smoothedGain) * coeff;
        
        // Output delayed samples with gain applied
        left[i] = m_delayLineL[outputIndex] * static_cast<float>(m_smoothedGain);
        right[i] = m_delayLineR[outputIndex] * static_cast<float>(m_smoothedGain);
        
        // Advance delay index
        m_delayIndex = (m_delayIndex + 1) % m_delayLineL.size();
    }
}

} // namespace dynamics
} // namespace dsp
} // namespace dawg
