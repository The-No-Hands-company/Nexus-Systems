
#include "dawg/dsp/SineGenerator.h"
#include "dawg/core/AudioBuffer.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dawg {
namespace dsp {

SineGenerator::SineGenerator() = default;
SineGenerator::~SineGenerator() = default;

void SineGenerator::setFrequency(double freq) {
    frequency = freq;
}

void SineGenerator::setSampleRate(uint32_t sr) {
    sampleRate = sr;
}

void SineGenerator::setAmplitude(double amp) {
    amplitude = amp;
}

void SineGenerator::generate(core::AudioBuffer& buffer, uint32_t numSamples) {
    if (numSamples == 0) return;
    
    double phaseIncrement = 2.0 * M_PI * frequency / sampleRate;
    
    for (uint32_t sample = 0; sample < numSamples; ++sample) {
        float value = static_cast<float>(amplitude * std::sin(phase));
        
        // Write to all channels
        for (uint16_t channel = 0; channel < buffer.getChannels(); ++channel) {
            buffer.setSample(channel, sample, value);
        }
        
        phase += phaseIncrement;
        if (phase >= 2.0 * M_PI) {
            phase -= 2.0 * M_PI;
        }
    }
}

void SineGenerator::reset() {
    phase = 0.0;
}

} // namespace dsp
} // namespace dawg
