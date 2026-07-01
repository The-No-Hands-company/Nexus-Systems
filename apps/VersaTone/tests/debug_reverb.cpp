#include "../engine/include/dawg/dsp/time/AlgorithmicReverb.h"
#include "../engine/include/dawg/core/AudioBuffer.h"
#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace dawg::dsp::time;
using namespace dawg::core;

int main() {
    std::cout << "Testing AlgorithmicReverb directly..." << std::endl;
    
    // Create simple test
    AudioBuffer buffer(2, 1024);
    
    // Generate sine wave
    for (uint32_t i = 0; i < 1024; ++i) {
        float value = 0.5f * std::sin(2.0f * M_PI * 440.0f * i / 48000.0f);
        buffer.getChannelData(0)[i] = value;
        buffer.getChannelData(1)[i] = value;
    }
    
    // Calculate input RMS
    double inputRMS = 0.0;
    for (uint32_t i = 0; i < 1024; ++i) {
        inputRMS += buffer.getChannelData(0)[i] * buffer.getChannelData(0)[i];
    }
    inputRMS = std::sqrt(inputRMS / 1024.0);
    std::cout << "Input RMS: " << inputRMS << std::endl;
    
    // Create and configure reverb
    AlgorithmicReverb reverb;
    reverb.setSampleRate(48000);
    reverb.setParameter("wetLevel", 1.0);
    reverb.setParameter("roomSize", 0.8);
    
    // Process
    reverb.process(buffer);
    
    // Calculate output RMS
    double outputRMS = 0.0;
    for (uint32_t i = 0; i < 1024; ++i) {
        outputRMS += buffer.getChannelData(0)[i] * buffer.getChannelData(0)[i];
    }
    outputRMS = std::sqrt(outputRMS / 1024.0);
    std::cout << "Output RMS: " << outputRMS << std::endl;
    
    // Print first few samples to debug
    std::cout << "\nFirst 10 input samples: ";
    AudioBuffer inputCopy(2, 1024);
    for (uint32_t i = 0; i < 10; ++i) {
        float value = 0.5f * std::sin(2.0f * M_PI * 440.0f * i / 48000.0f);
        std::cout << value << " ";
    }
    std::cout << std::endl;
    
    std::cout << "First 10 output samples: ";
    for (uint32_t i = 0; i < 10; ++i) {
        std::cout << buffer.getChannelData(0)[i] << " ";
    }
    std::cout << std::endl;
    
    std::cout << "Middle 10 output samples (500-509): ";
    for (uint32_t i = 500; i < 510; ++i) {
        std::cout << buffer.getChannelData(0)[i] << " ";
    }
    std::cout << std::endl;
    
    std::cout << "Last 10 output samples: ";
    for (uint32_t i = 1014; i < 1024; ++i) {
        std::cout << buffer.getChannelData(0)[i] << " ";
    }
    std::cout << std::endl;
    
    return 0;
}
