#include "../engine/include/dawg/dsp/dynamics/Compressor.h"
#include "../engine/include/dawg/core/AudioBuffer.h"
#include <iostream>
#include <cmath>

using namespace dawg::dsp::dynamics;
using namespace dawg::core;

int main() {
    std::cout << "Debug: Testing compressor with simple signal" << std::endl;
    
    // Create a simple buffer with one very high sample
    AudioBuffer buffer(1, 4);  // 1 channel, 4 samples
    float* data = buffer.getChannelData(0);
    
    // Set specific values
    data[0] = 0.3f;  // Below threshold
    data[1] = 0.8f;  // Above threshold
    data[2] = 0.9f;  // Above threshold
    data[3] = 0.2f;  // Below threshold
    
    std::cout << "Input: [" << data[0] << ", " << data[1] << ", " << data[2] << ", " << data[3] << "]" << std::endl;
    
    Compressor compressor;
    compressor.setSampleRate(48000);
    
    // Set threshold to -6dB (which should be around 0.5 linear)
    compressor.setParameter("threshold", -6.0);  // -6dB threshold
    compressor.setParameter("ratio", 4.0);
    compressor.setParameter("makeup", 0.0);  // 0dB makeup gain
    
    std::cout << "Threshold set to: " << compressor.getParameter("threshold") << " dB" << std::endl;
    std::cout << "Ratio set to: " << compressor.getParameter("ratio") << std::endl;
    
    compressor.process(buffer);
    
    std::cout << "Output: [" << data[0] << ", " << data[1] << ", " << data[2] << ", " << data[3] << "]" << std::endl;
    
    // Check if samples above threshold were affected
    bool working = (data[1] < 0.8f) && (data[2] < 0.9f);
    std::cout << "Compressor working: " << (working ? "YES" : "NO") << std::endl;
    
    return 0;
}
