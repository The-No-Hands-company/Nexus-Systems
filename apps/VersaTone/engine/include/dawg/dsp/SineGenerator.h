#pragma once
#include <cstdint>

// Forward declaration
namespace dawg { namespace core { class AudioBuffer; } }

namespace dawg {
namespace dsp {

/**
 * @brief Professional sine wave generator
 */
class SineGenerator {
public:
    SineGenerator();
    ~SineGenerator();
    
    void setFrequency(double frequency);
    void setSampleRate(uint32_t sampleRate);
    void setAmplitude(double amplitude);
    
    void generate(core::AudioBuffer& buffer, uint32_t numSamples);
    void reset();
    
private:
    double frequency = 440.0;
    uint32_t sampleRate = 48000;
    double amplitude = 0.5;
    double phase = 0.0;
};

} // namespace dsp
} // namespace dawg
