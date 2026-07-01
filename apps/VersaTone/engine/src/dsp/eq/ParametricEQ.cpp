#include "../../../include/dawg/dsp/eq/ParametricEQ.h"
#include "../../../include/dawg/core/AudioBuffer.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace dawg {
namespace dsp {
namespace eq {

// ParametricEQ Private Implementation with Biquad Filters
class ParametricEQ::Impl {
public:
    // Biquad Filter Structure for each band
    struct BiquadFilter {
        double b0, b1, b2;  // Feed forward coefficients
        double a1, a2;      // Feed back coefficients
        double x1, x2;      // Input delay line
        double y1, y2;      // Output delay line
        
        BiquadFilter() : b0(1.0), b1(0.0), b2(0.0), a1(0.0), a2(0.0), 
                        x1(0.0), x2(0.0), y1(0.0), y2(0.0) {}
        
        void reset() {
            x1 = x2 = y1 = y2 = 0.0;
        }
        
        double process(double input) {
            double output = b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            
            // Update delay lines
            x2 = x1; x1 = input;
            y2 = y1; y1 = output;
            
            return output;
        }
    };

    // EQ Parameters
    struct EQBand {
        double frequency = 1000.0;  // Hz
        double gain = 0.0;          // dB
        double Q = 0.707;           // Quality factor
        bool enabled = true;
        BiquadFilter filter;
    };

    uint32_t sampleRate = 48000;
    static constexpr uint32_t NUM_BANDS = 8;
    std::array<EQBand, NUM_BANDS> bands;
    
    Impl() {
        // Initialize default frequency bands (typical mixing console layout)
        bands[0].frequency = 60.0;    // Low shelf
        bands[1].frequency = 200.0;   // Low-mid
        bands[2].frequency = 500.0;   // Mid
        bands[3].frequency = 1000.0;  // Mid
        bands[4].frequency = 2000.0;  // High-mid
        bands[5].frequency = 4000.0;  // High-mid
        bands[6].frequency = 8000.0;  // High
        bands[7].frequency = 12000.0; // High shelf
        
        updateAllFilters();
    }
    
    void updateAllFilters() {
        for (auto& band : bands) {
            if (band.enabled) {
                calculateBiquadCoefficients(band);
            }
        }
    }
    
    void calculateBiquadCoefficients(EQBand& band) {
        double omega = 2.0 * M_PI * band.frequency / sampleRate;
        double sin_omega = std::sin(omega);
        double cos_omega = std::cos(omega);
        double alpha = sin_omega / (2.0 * band.Q);
        double A = std::pow(10.0, band.gain / 40.0);  // Convert dB to linear
        
        // Peaking EQ filter coefficients
        double b0 = 1.0 + alpha * A;
        double b1 = -2.0 * cos_omega;
        double b2 = 1.0 - alpha * A;
        double a0 = 1.0 + alpha / A;
        double a1 = -2.0 * cos_omega;
        double a2 = 1.0 - alpha / A;
        
        // Normalize coefficients
        band.filter.b0 = b0 / a0;
        band.filter.b1 = b1 / a0;
        band.filter.b2 = b2 / a0;
        band.filter.a1 = a1 / a0;
        band.filter.a2 = a2 / a0;
    }
    
    void process(dawg::core::AudioBuffer& buffer) {
        if (buffer.getChannelCount() == 0 || buffer.getSampleCount() == 0) return;
        
        // Process each channel
        for (uint32_t channel = 0; channel < buffer.getChannelCount(); ++channel) {
            float* channelData = buffer.getChannelData(channel);
            
            // Process each sample
            for (uint32_t sample = 0; sample < buffer.getSampleCount(); ++sample) {
                double sampleValue = static_cast<double>(channelData[sample]);
                
                // Process through all enabled EQ bands
                for (auto& band : bands) {
                    if (band.enabled) {
                        sampleValue = band.filter.process(sampleValue);
                    }
                }
                
                channelData[sample] = static_cast<float>(sampleValue);
            }
        }
    }
    
    void reset() {
        for (auto& band : bands) {
            band.filter.reset();
        }
    }
};

// ========================================================================
// CONSTRUCTION/DESTRUCTION
// ========================================================================

ParametricEQ::ParametricEQ() : pImpl(std::make_unique<Impl>()) {}
ParametricEQ::~ParametricEQ() = default;

// ========================================================================
// CORE PROCESSING
// ========================================================================

void ParametricEQ::process(dawg::core::AudioBuffer& buffer) {
    if (!isBypassed()) {
        pImpl->process(buffer);
    }
}

void ParametricEQ::reset() {
    pImpl->reset();
}

// ========================================================================
// CONFIGURATION
// ========================================================================

void ParametricEQ::setSampleRate(uint32_t sampleRate) {
    this->sampleRate = sampleRate;
    pImpl->sampleRate = sampleRate;
    pImpl->updateAllFilters();
}

void ParametricEQ::setBufferSize(uint32_t bufferSize) {
    this->bufferSize = bufferSize;
}

// ========================================================================
// PARAMETER CONTROL
// ========================================================================

void ParametricEQ::setParameter(const std::string& name, double value) {
    // Parse band-specific parameters like "band1_gain", "band1_freq", "band1_q"
    if (name.find("band") == 0 && name.length() > 5) {
        uint32_t bandIndex = std::stoi(name.substr(4, 1)) - 1;
        if (bandIndex < pImpl->NUM_BANDS) {
            std::string param = name.substr(6);
            
            if (param == "gain") {
                pImpl->bands[bandIndex].gain = value;
                pImpl->calculateBiquadCoefficients(pImpl->bands[bandIndex]);
            } else if (param == "freq") {
                pImpl->bands[bandIndex].frequency = value;
                pImpl->calculateBiquadCoefficients(pImpl->bands[bandIndex]);
            } else if (param == "q") {
                pImpl->bands[bandIndex].Q = value;
                pImpl->calculateBiquadCoefficients(pImpl->bands[bandIndex]);
            }
        }
    }
}

double ParametricEQ::getParameter(const std::string& name) const {
    if (name.find("band") == 0 && name.length() > 5) {
        uint32_t bandIndex = std::stoi(name.substr(4, 1)) - 1;
        if (bandIndex < pImpl->NUM_BANDS) {
            std::string param = name.substr(6);
            
            if (param == "gain") return pImpl->bands[bandIndex].gain;
            if (param == "freq") return pImpl->bands[bandIndex].frequency;
            if (param == "q") return pImpl->bands[bandIndex].Q;
        }
    }
    return 0.0;
}

std::vector<std::string> ParametricEQ::getParameterNames() const {
    std::vector<std::string> names;
    for (uint32_t i = 0; i < pImpl->NUM_BANDS; ++i) {
        std::string bandPrefix = "band" + std::to_string(i + 1) + "_";
        names.push_back(bandPrefix + "gain");
        names.push_back(bandPrefix + "freq");
        names.push_back(bandPrefix + "q");
    }
    return names;
}

// ========================================================================
// EQ-SPECIFIC CONTROLS
// ========================================================================

void ParametricEQ::setBandEnabled(uint32_t bandIndex, bool enabled) {
    if (bandIndex < pImpl->NUM_BANDS) {
        pImpl->bands[bandIndex].enabled = enabled;
    }
}

std::vector<double> ParametricEQ::getFrequencyResponse(const std::vector<double>& frequencies) const {
    std::vector<double> response(frequencies.size());
    
    for (size_t i = 0; i < frequencies.size(); ++i) {
        double freq = frequencies[i];
        double magnitude = 0.0;
        
        // Calculate combined frequency response of all enabled bands
        for (const auto& band : pImpl->bands) {
            if (band.enabled) {
                double omega = 2.0 * M_PI * freq / pImpl->sampleRate;
                std::complex<double> z = std::exp(std::complex<double>(0, omega));
                
                // Calculate H(z) = (b0 + b1*z^-1 + b2*z^-2) / (1 + a1*z^-1 + a2*z^-2)
                std::complex<double> numerator = band.filter.b0 + 
                    band.filter.b1 / z + band.filter.b2 / (z * z);
                std::complex<double> denominator = 1.0 + 
                    band.filter.a1 / z + band.filter.a2 / (z * z);
                
                std::complex<double> H = numerator / denominator;
                magnitude += 20.0 * std::log10(std::abs(H));  // Convert to dB
            }
        }
        
        response[i] = magnitude;
    }
    
    return response;
}

} // namespace eq
} // namespace dsp
} // namespace dawg
