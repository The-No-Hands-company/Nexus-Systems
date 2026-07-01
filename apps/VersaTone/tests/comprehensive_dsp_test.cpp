#include "../engine/include/dawg/dsp/dynamics/Compressor.h"
#include "../engine/include/dawg/dsp/dynamics/Limiter.h"
#include "../engine/include/dawg/dsp/eq/ParametricEQ.h"
#include "../engine/include/dawg/dsp/time/AlgorithmicReverb.h"
#include "../engine/include/dawg/dsp/time/Delay.h"
#include "../engine/include/dawg/dsp/modulation/Chorus.h"
#include "../engine/include/dawg/core/AudioBuffer.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <cassert>
#include <functional>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace dawg::dsp::dynamics;
using namespace dawg::dsp::eq;
using namespace dawg::dsp::time;
using namespace dawg::dsp::modulation;
using namespace dawg::core;

// ========================================================================
// COMPREHENSIVE TESTING FRAMEWORK
// ========================================================================

class DSPTestFramework {
public:
    struct TestResult {
        std::string name;
        bool passed;
        std::string details;
        double performanceMs;
        
        TestResult(const std::string& n, bool p, const std::string& d, double perf = 0.0)
            : name(n), passed(p), details(d), performanceMs(perf) {}
    };
    
    std::vector<TestResult> results;
    
    void runTest(const std::string& name, std::function<TestResult()> testFunc) {
        std::cout << "Running test: " << name << "..." << std::endl;
        TestResult result = testFunc();
        results.push_back(result);
        
        std::cout << "  " << (result.passed ? "PASS" : "FAIL") << " - " << result.details;
        if (result.performanceMs > 0) {
            std::cout << " (Performance: " << std::fixed << std::setprecision(3) 
                      << result.performanceMs << "ms)";
        }
        std::cout << std::endl;
    }
    
    void printSummary() {
        int passed = 0;
        int failed = 0;
        double totalTime = 0.0;
        
        for (const auto& result : results) {
            if (result.passed) passed++;
            else failed++;
            totalTime += result.performanceMs;
        }
        
        std::cout << "\n========================================" << std::endl;
        std::cout << "TEST SUMMARY" << std::endl;
        std::cout << "========================================" << std::endl;
        std::cout << "Passed: " << passed << std::endl;
        std::cout << "Failed: " << failed << std::endl;
        std::cout << "Total: " << (passed + failed) << std::endl;
        std::cout << "Success Rate: " << std::fixed << std::setprecision(1) 
                  << (100.0 * passed / (passed + failed)) << "%" << std::endl;
        std::cout << "Total Test Time: " << std::fixed << std::setprecision(3) 
                  << totalTime << "ms" << std::endl;
        std::cout << "========================================" << std::endl;
    }
};

// ========================================================================
// AUDIO SIGNAL GENERATORS FOR TESTING
// ========================================================================

class TestSignalGenerator {
public:
    static void generateSine(AudioBuffer& buffer, double frequency, double amplitude, uint32_t sampleRate) {
        for (uint32_t channel = 0; channel < buffer.getChannelCount(); ++channel) {
            float* data = buffer.getChannelData(channel);
            for (uint32_t sample = 0; sample < buffer.getSampleCount(); ++sample) {
                double time = static_cast<double>(sample) / sampleRate;
                data[sample] = static_cast<float>(amplitude * std::sin(2.0 * M_PI * frequency * time));
            }
        }
    }
    
    static void generateNoise(AudioBuffer& buffer, double amplitude) {
        for (uint32_t channel = 0; channel < buffer.getChannelCount(); ++channel) {
            float* data = buffer.getChannelData(channel);
            for (uint32_t sample = 0; sample < buffer.getSampleCount(); ++sample) {
                // Simple pseudo-random noise
                static uint32_t seed = 12345;
                seed = seed * 1103515245 + 12345;
                double noise = (static_cast<double>(seed) / 0x7FFFFFFF) - 1.0;
                data[sample] = static_cast<float>(amplitude * noise);
            }
        }
    }
    
    static double getRMSLevel(const AudioBuffer& buffer, uint32_t channel = 0) {
        if (channel >= buffer.getChannelCount()) return 0.0;
        
        const float* data = buffer.getChannelData(channel);
        double sum = 0.0;
        
        for (uint32_t sample = 0; sample < buffer.getSampleCount(); ++sample) {
            double value = static_cast<double>(data[sample]);
            sum += value * value;
        }
        
        return std::sqrt(sum / buffer.getSampleCount());
    }
    
    static double getPeakLevel(const AudioBuffer& buffer, uint32_t channel = 0) {
        if (channel >= buffer.getChannelCount()) return 0.0;
        
        const float* data = buffer.getChannelData(channel);
        double peak = 0.0;
        
        for (uint32_t sample = 0; sample < buffer.getSampleCount(); ++sample) {
            double absValue = std::abs(static_cast<double>(data[sample]));
            if (absValue > peak) peak = absValue;
        }
        
        return peak;
    }
    
    static double getStereoWidth(const AudioBuffer& buffer) {
        if (buffer.getChannelCount() < 2) return 0.0;
        
        const float* leftData = buffer.getChannelData(0);
        const float* rightData = buffer.getChannelData(1);
        
        double correlation = 0.0;
        double leftEnergy = 0.0;
        double rightEnergy = 0.0;
        
        for (uint32_t sample = 0; sample < buffer.getSampleCount(); ++sample) {
            double left = static_cast<double>(leftData[sample]);
            double right = static_cast<double>(rightData[sample]);
            
            correlation += left * right;
            leftEnergy += left * left;
            rightEnergy += right * right;
        }
        
        // Normalize correlation
        double normalization = std::sqrt(leftEnergy * rightEnergy);
        if (normalization > 0.0) {
            correlation /= normalization;
        }
        
        // Return width as 1 - correlation (0 = mono, 1 = anti-correlated)
        return 1.0 - std::abs(correlation);
    }
};

// ========================================================================
// BENCHMARK SYSTEM
// ========================================================================

class AudioBenchmark {
public:
    static double benchmarkEffect(std::function<void()> processor, int iterations = 1000) {
        auto start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            processor();
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        return static_cast<double>(duration.count()) / 1000.0; // Convert to milliseconds
    }
};

// ========================================================================
// COMPREHENSIVE COMPRESSOR TESTS
// ========================================================================

DSPTestFramework::TestResult testCompressorGainReduction() {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Create test signal above threshold
    AudioBuffer buffer(2, 1024);
    TestSignalGenerator::generateSine(buffer, 1000.0, 0.8, 48000); // -2dB signal
    
    double inputRMS = TestSignalGenerator::getRMSLevel(buffer);
    
    // Create and configure compressor
    Compressor compressor;
    compressor.setSampleRate(48000);
    compressor.setParameter("threshold", -6.0);  // -6dB threshold (about 0.5 linear)
    compressor.setParameter("ratio", 4.0);       // 4:1 ratio
    compressor.setParameter("makeup", 0.0);      // 0dB makeup gain
    
    // Process signal
    compressor.process(buffer);
    
    double outputRMS = TestSignalGenerator::getRMSLevel(buffer);
    double gainReduction = outputRMS / inputRMS;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    // Expect significant gain reduction (should be around 0.7-0.8 for this setup)
    bool passed = (gainReduction < 0.85 && gainReduction > 0.5);
    
    std::string details = "Input RMS: " + std::to_string(inputRMS) + 
                         ", Output RMS: " + std::to_string(outputRMS) + 
                         ", Gain Reduction: " + std::to_string(gainReduction);
    
    return DSPTestFramework::TestResult("Compressor Gain Reduction", passed, details, timeMs);
}

DSPTestFramework::TestResult testCompressorBypass() {
    auto start = std::chrono::high_resolution_clock::now();
    
    AudioBuffer buffer(2, 1024);
    TestSignalGenerator::generateSine(buffer, 1000.0, 0.8, 48000);
    
    // Create backup of original signal
    AudioBuffer original(2, 1024);
    original.copyFrom(buffer);
    
    Compressor compressor;
    compressor.setSampleRate(48000);
    compressor.setBypassed(true);  // Bypass mode
    
    compressor.process(buffer);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    // Compare signals - should be identical when bypassed
    bool passed = true;
    const float* originalData = original.getChannelData(0);
    const float* processedData = buffer.getChannelData(0);
    
    for (uint32_t i = 0; i < buffer.getSampleCount(); ++i) {
        if (std::abs(originalData[i] - processedData[i]) > 1e-6f) {
            passed = false;
            break;
        }
    }
    
    std::string details = passed ? "Signal unchanged when bypassed" : "Signal modified when bypassed";
    
    return DSPTestFramework::TestResult("Compressor Bypass", passed, details, timeMs);
}

// ========================================================================
// COMPREHENSIVE PARAMETRIC EQ TESTS
// ========================================================================

DSPTestFramework::TestResult testParametricEQBasicProcessing() {
    auto start = std::chrono::high_resolution_clock::now();
    
    AudioBuffer buffer(2, 1024);
    TestSignalGenerator::generateSine(buffer, 1000.0, 0.5, 48000);
    
    double inputRMS = TestSignalGenerator::getRMSLevel(buffer);
    
    ParametricEQ eq;
    eq.setSampleRate(48000);
    
    // Boost 1kHz by 6dB (should increase signal level)
    eq.setParameter("band4_freq", 1000.0);  // Band 4 is close to 1kHz
    eq.setParameter("band4_gain", 6.0);     // +6dB boost
    eq.setParameter("band4_q", 2.0);        // Narrow Q
    
    eq.process(buffer);
    
    double outputRMS = TestSignalGenerator::getRMSLevel(buffer);
    double gainChange = outputRMS / inputRMS;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    // With 6dB boost, we should see some increase in level
    bool passed = (gainChange > 1.0 && gainChange < 3.0);  // Reasonable range
    
    std::string details = "Input RMS: " + std::to_string(inputRMS) + 
                         ", Output RMS: " + std::to_string(outputRMS) + 
                         ", Gain Change: " + std::to_string(gainChange);
    
    return DSPTestFramework::TestResult("ParametricEQ Basic Processing", passed, details, timeMs);
}

DSPTestFramework::TestResult testParametricEQParameterControl() {
    auto start = std::chrono::high_resolution_clock::now();
    
    ParametricEQ eq;
    eq.setSampleRate(48000);
    
    // Test parameter setting and getting
    eq.setParameter("band1_freq", 440.0);
    eq.setParameter("band1_gain", 3.0);
    eq.setParameter("band1_q", 1.5);
    
    double freq = eq.getParameter("band1_freq");
    double gain = eq.getParameter("band1_gain");
    double q = eq.getParameter("band1_q");
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    bool passed = (std::abs(freq - 440.0) < 0.1) && 
                  (std::abs(gain - 3.0) < 0.1) && 
                  (std::abs(q - 1.5) < 0.1);
    
    std::string details = "Freq: " + std::to_string(freq) + 
                         ", Gain: " + std::to_string(gain) + 
                         ", Q: " + std::to_string(q);
    
    return DSPTestFramework::TestResult("ParametricEQ Parameter Control", passed, details, timeMs);
}

// ========================================================================
// COMPREHENSIVE ALGORITHMIC REVERB TESTS
// ========================================================================

DSPTestFramework::TestResult testAlgorithmicReverbProcessing() {
    auto start = std::chrono::high_resolution_clock::now();
    
    AudioBuffer buffer(2, 2048);  // Larger buffer for reverb tail
    TestSignalGenerator::generateSine(buffer, 440.0, 0.5, 48000);  // A440 test tone
    
    double inputRMS = TestSignalGenerator::getRMSLevel(buffer);
    
    AlgorithmicReverb reverb;
    reverb.setSampleRate(48000);
    
    // Configure reverb for noticeable effect
    reverb.setParameter("roomSize", 0.8);    // Large room
    reverb.setParameter("wetLevel", 1.0);    // 100% wet for clear effect
    reverb.setParameter("damping", 0.3);     // Low damping for brightness
    reverb.setParameter("predelay", 0.02);   // 20ms predelay
    
    reverb.process(buffer);
    
    double outputRMS = TestSignalGenerator::getRMSLevel(buffer);
    
    // Check last quarter of buffer where reverb tail should be present
    double tailRMS = 0.0;
    uint32_t tailStart = buffer.getSampleCount() * 3 / 4;
    for (uint32_t i = tailStart; i < buffer.getSampleCount(); ++i) {
        tailRMS += buffer.getChannelData(0)[i] * buffer.getChannelData(0)[i];
    }
    tailRMS = std::sqrt(tailRMS / (buffer.getSampleCount() - tailStart));
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    // Reverb should have generated reverb tail (non-zero output in tail section)
    bool passed = (tailRMS > 0.001);  // Should have some reverb tail energy
    
    std::string details = "Input RMS: " + std::to_string(inputRMS) + 
                         ", Output RMS: " + std::to_string(outputRMS) + 
                         ", Tail RMS: " + std::to_string(tailRMS);
    
    return DSPTestFramework::TestResult("AlgorithmicReverb Processing", passed, details, timeMs);
}

DSPTestFramework::TestResult testAlgorithmicReverbParameterControl() {
    auto start = std::chrono::high_resolution_clock::now();
    
    AlgorithmicReverb reverb;
    reverb.setSampleRate(48000);
    
    // Test parameter setting and getting
    reverb.setParameter("roomSize", 0.75);
    reverb.setParameter("damping", 0.4);
    reverb.setParameter("wetLevel", 0.6);
    reverb.setParameter("predelay", 0.05);
    
    double roomSize = reverb.getParameter("roomSize");
    double damping = reverb.getParameter("damping");
    double wetLevel = reverb.getParameter("wetLevel");
    double predelay = reverb.getParameter("predelay");
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    bool passed = (std::abs(roomSize - 0.75) < 0.1) && 
                  (std::abs(damping - 0.4) < 0.1) && 
                  (std::abs(wetLevel - 0.6) < 0.1) &&
                  (std::abs(predelay - 0.05) < 0.01);
    
    std::string details = "RoomSize: " + std::to_string(roomSize) + 
                         ", Damping: " + std::to_string(damping) + 
                         ", WetLevel: " + std::to_string(wetLevel) +
                         ", Predelay: " + std::to_string(predelay);
    
    return DSPTestFramework::TestResult("AlgorithmicReverb Parameter Control", passed, details, timeMs);
}

DSPTestFramework::TestResult benchmarkAlgorithmicReverbPerformance() {
    auto start = std::chrono::high_resolution_clock::now();
    
    AudioBuffer buffer(2, 1024);
    TestSignalGenerator::generateSine(buffer, 440.0, 0.5, 48000);
    
    AlgorithmicReverb reverb;
    reverb.setSampleRate(48000);
    reverb.setParameter("roomSize", 0.7);
    reverb.setParameter("wetLevel", 0.5);
    
    // Benchmark 1000 iterations
    const int iterations = 1000;
    auto processStart = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        // Reset buffer for consistent testing
        TestSignalGenerator::generateSine(buffer, 440.0, 0.5, 48000);
        reverb.process(buffer);
    }
    
    auto processEnd = std::chrono::high_resolution_clock::now();
    auto processDuration = std::chrono::duration_cast<std::chrono::microseconds>(processEnd - processStart);
    double processTimeMs = static_cast<double>(processDuration.count()) / 1000.0;
    double avgTimePerBuffer = processTimeMs / iterations;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    bool passed = (avgTimePerBuffer < 2.0);  // Should process 1024 samples in under 2ms
    
    std::string details = std::to_string(iterations) + " iterations in " + 
                         std::to_string(processTimeMs) + "ms (" + 
                         std::to_string(avgTimePerBuffer) + "ms per buffer)";
    
    return DSPTestFramework::TestResult("AlgorithmicReverb Performance", passed, details, timeMs);
}

// ========================================================================
// COMPREHENSIVE LIMITER TESTS
// ========================================================================

DSPTestFramework::TestResult testLimiterProcessing() {
    auto start = std::chrono::high_resolution_clock::now();
    
    AudioBuffer buffer(2, 2048);  // Larger buffer for look-ahead to work
    // Generate loud signal that will trigger limiting
    TestSignalGenerator::generateSine(buffer, 440.0, 0.9, 48000);  // High level signal
    
    double inputRMS = TestSignalGenerator::getRMSLevel(buffer);
    
    Limiter limiter;
    limiter.setSampleRate(48000);
    
    // Configure limiter for noticeable effect
    limiter.setParameter("ceiling", -3.0);      // -3dB ceiling (more headroom)
    limiter.setParameter("release", 50.0);      // 50ms release
    limiter.setParameter("lookAhead", 2.0);     // 2ms lookahead (shorter)
    limiter.setParameter("inputGain", 9.0);     // +9dB input gain to strongly trigger limiting
    
    limiter.process(buffer);
    
    double outputRMS = TestSignalGenerator::getRMSLevel(buffer);
    double levelChange = outputRMS / inputRMS;
    
    // Check peak level in second half of buffer (after look-ahead has effect)
    double outputPeak = 0.0;
    uint32_t startSample = buffer.getSampleCount() / 2; // Check second half
    for (uint32_t ch = 0; ch < buffer.getChannelCount(); ++ch) {
        for (uint32_t i = startSample; i < buffer.getSampleCount(); ++i) {
            outputPeak = std::max(outputPeak, static_cast<double>(std::abs(buffer.getChannelData(ch)[i])));
        }
    }
    double outputPeakdB = outputPeak > 0.0 ? 20.0 * std::log10(outputPeak) : -96.0;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    // Limiter should process signal (gain reduction should occur with heavy input gain)
    bool passed = (outputPeakdB <= -2.5);  // Below -3dB ceiling with margin
    
    std::string details = "Input RMS: " + std::to_string(inputRMS) + 
                         ", Output RMS: " + std::to_string(outputRMS) + 
                         ", Level Change: " + std::to_string(levelChange) +
                         ", Peak Level: " + std::to_string(outputPeakdB) + "dB";
    
    return DSPTestFramework::TestResult("Limiter Processing", passed, details, timeMs);
}

DSPTestFramework::TestResult testLimiterParameterControl() {
    auto start = std::chrono::high_resolution_clock::now();
    
    Limiter limiter;
    limiter.setSampleRate(48000);
    
    // Test parameter setting and getting
    limiter.setParameter("ceiling", -2.0);
    limiter.setParameter("release", 100.0);
    limiter.setParameter("lookAhead", 3.0);
    limiter.setParameter("inputGain", 3.0);
    limiter.setParameter("outputGain", -1.0);
    
    double ceiling = limiter.getParameter("ceiling");
    double release = limiter.getParameter("release");
    double lookAhead = limiter.getParameter("lookAhead");
    double inputGain = limiter.getParameter("inputGain");
    double outputGain = limiter.getParameter("outputGain");
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    bool passed = (std::abs(ceiling - (-2.0)) < 0.1) && 
                  (std::abs(release - 100.0) < 0.1) && 
                  (std::abs(lookAhead - 3.0) < 0.1) &&
                  (std::abs(inputGain - 3.0) < 0.1) &&
                  (std::abs(outputGain - (-1.0)) < 0.1);
    
    std::string details = "Ceiling: " + std::to_string(ceiling) + 
                         ", Release: " + std::to_string(release) + 
                         ", LookAhead: " + std::to_string(lookAhead) +
                         ", InputGain: " + std::to_string(inputGain) +
                         ", OutputGain: " + std::to_string(outputGain);
    
    return DSPTestFramework::TestResult("Limiter Parameter Control", passed, details, timeMs);
}

// ========================================================================
// COMPREHENSIVE DELAY TESTS
// ========================================================================

DSPTestFramework::TestResult testDelayProcessing() {
    auto start = std::chrono::high_resolution_clock::now();
    
    AudioBuffer buffer(2, 2048);  // Larger buffer for delay effects
    TestSignalGenerator::generateSine(buffer, 440.0, 0.5, 48000);  // A440 test tone
    
    double inputRMS = TestSignalGenerator::getRMSLevel(buffer);
    
    Delay delay;
    delay.setSampleRate(48000);
    
    // Configure delay for noticeable effect
    delay.setParameter("delayTime", 100.0);     // 100ms delay
    delay.setParameter("feedback", 0.3);        // 30% feedback
    delay.setParameter("mix", 0.5);             // 50% wet/dry mix
    delay.setParameter("mode", 1.0);            // Stereo mode
    
    delay.process(buffer);
    
    double outputRMS = TestSignalGenerator::getRMSLevel(buffer);
    double levelChange = outputRMS / inputRMS;
    
    // Check that delay has added content (should be slightly higher due to feedback)
    bool hasDelayContent = false;
    uint32_t delayStart = buffer.getSampleCount() / 2; // Check second half for delay buildup
    for (uint32_t ch = 0; ch < buffer.getChannelCount(); ++ch) {
        for (uint32_t i = delayStart; i < buffer.getSampleCount(); ++i) {
            if (std::abs(buffer.getChannelData(ch)[i]) > 0.1f) {
                hasDelayContent = true;
                break;
            }
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    // Delay should process signal and maintain reasonable level
    bool passed = hasDelayContent && (levelChange > 0.4) && (levelChange < 2.0);
    
    std::string details = "Input RMS: " + std::to_string(inputRMS) + 
                         ", Output RMS: " + std::to_string(outputRMS) + 
                         ", Level Change: " + std::to_string(levelChange) +
                         ", Has Content: " + (hasDelayContent ? "Yes" : "No");
    
    return DSPTestFramework::TestResult("Delay Processing", passed, details, timeMs);
}

DSPTestFramework::TestResult testDelayParameterControl() {
    auto start = std::chrono::high_resolution_clock::now();
    
    Delay delay;
    delay.setSampleRate(48000);
    
    // Test parameter setting and getting
    delay.setParameter("delayTime", 250.0);
    delay.setParameter("feedback", 0.4);
    delay.setParameter("mix", 0.6);
    delay.setParameter("lowCut", 100.0);
    delay.setParameter("highCut", 5000.0);
    delay.setParameter("modDepth", 0.2);
    delay.setParameter("modRate", 1.5);
    
    double delayTime = delay.getParameter("delayTime");
    double feedback = delay.getParameter("feedback");
    double mix = delay.getParameter("mix");
    double lowCut = delay.getParameter("lowCut");
    double highCut = delay.getParameter("highCut");
    double modDepth = delay.getParameter("modDepth");
    double modRate = delay.getParameter("modRate");
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    bool passed = (std::abs(delayTime - 250.0) < 0.1) && 
                  (std::abs(feedback - 0.4) < 0.1) && 
                  (std::abs(mix - 0.6) < 0.1) &&
                  (std::abs(lowCut - 100.0) < 0.1) &&
                  (std::abs(highCut - 5000.0) < 0.1) &&
                  (std::abs(modDepth - 0.2) < 0.1) &&
                  (std::abs(modRate - 1.5) < 0.1);
    
    std::string details = "DelayTime: " + std::to_string(delayTime) + 
                         ", Feedback: " + std::to_string(feedback) + 
                         ", Mix: " + std::to_string(mix) +
                         ", LowCut: " + std::to_string(lowCut) +
                         ", HighCut: " + std::to_string(highCut);
    
    return DSPTestFramework::TestResult("Delay Parameter Control", passed, details, timeMs);
}

DSPTestFramework::TestResult benchmarkDelayPerformance() {
    auto start = std::chrono::high_resolution_clock::now();
    
    AudioBuffer buffer(2, 1024);
    TestSignalGenerator::generateSine(buffer, 440.0, 0.5, 48000);
    
    Delay delay;
    delay.setSampleRate(48000);
    delay.setParameter("delayTime", 150.0);
    delay.setParameter("feedback", 0.3);
    delay.setParameter("mix", 0.4);
    
    // Benchmark processing
    const int iterations = 1000;
    auto benchStart = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        // Reset buffer for each iteration
        TestSignalGenerator::generateSine(buffer, 440.0, 0.5, 48000);
        delay.process(buffer);
    }
    
    auto benchEnd = std::chrono::high_resolution_clock::now();
    auto benchDuration = std::chrono::duration_cast<std::chrono::microseconds>(benchEnd - benchStart);
    double benchTimeMs = static_cast<double>(benchDuration.count()) / 1000.0;
    double averageTimeMs = benchTimeMs / iterations;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    // Check that we didn't crash and processing completed within reasonable time
    bool passed = (averageTimeMs < 1.0) && (benchTimeMs > 0.0);  // Should be fast
    
    std::string details = std::to_string(iterations) + " iterations in " + 
                         std::to_string(benchTimeMs) + "ms (" + 
                         std::to_string(averageTimeMs) + "ms per buffer)";
    
    return DSPTestFramework::TestResult("Delay Performance", passed, details, timeMs);
}

DSPTestFramework::TestResult benchmarkCompressorPerformance() {
    AudioBuffer buffer(2, 1024);
    TestSignalGenerator::generateSine(buffer, 1000.0, 0.8, 48000);
    
    Compressor compressor;
    compressor.setSampleRate(48000);
    compressor.setParameter("threshold", -6.0);
    compressor.setParameter("ratio", 4.0);
    
    double timeMs = AudioBenchmark::benchmarkEffect([&]() {
        AudioBuffer testBuffer(2, 1024);
        testBuffer.copyFrom(buffer);
        compressor.process(testBuffer);
    }, 1000);
    
    // Performance target: should process 1000 iterations in under 100ms
    bool passed = (timeMs < 100.0);
    
    std::string details = "1000 iterations in " + std::to_string(timeMs) + "ms" +
                         " (" + std::to_string(timeMs / 1000.0) + "ms per buffer)";
    
    return DSPTestFramework::TestResult("Compressor Performance", passed, details, timeMs);
}

DSPTestFramework::TestResult benchmarkParametricEQPerformance() {
    AudioBuffer buffer(2, 1024);
    TestSignalGenerator::generateSine(buffer, 1000.0, 0.5, 48000);
    
    ParametricEQ eq;
    eq.setSampleRate(48000);
    eq.setParameter("band1_gain", 3.0);
    eq.setParameter("band4_gain", -2.0);
    eq.setParameter("band8_gain", 4.0);
    
    double timeMs = AudioBenchmark::benchmarkEffect([&]() {
        AudioBuffer testBuffer(2, 1024);
        testBuffer.copyFrom(buffer);
        eq.process(testBuffer);
    }, 1000);
    
    // Performance target: should process 1000 iterations in under 150ms (EQ is more complex)
    bool passed = (timeMs < 150.0);
    
    std::string details = "1000 iterations in " + std::to_string(timeMs) + "ms" +
                         " (" + std::to_string(timeMs / 1000.0) + "ms per buffer)";
    
    return DSPTestFramework::TestResult("ParametricEQ Performance", passed, details, timeMs);
}

DSPTestFramework::TestResult benchmarkLimiterPerformance() {
    auto start = std::chrono::high_resolution_clock::now();
    
    AudioBuffer buffer(2, 1024);
    TestSignalGenerator::generateSine(buffer, 440.0, 0.8, 48000);
    
    Limiter limiter;
    limiter.setSampleRate(48000);
    limiter.setParameter("ceiling", -1.0);
    limiter.setParameter("inputGain", 3.0);
    
    // Benchmark processing
    const int iterations = 1000;
    auto benchStart = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        // Reset buffer for each iteration
        TestSignalGenerator::generateSine(buffer, 440.0, 0.8, 48000);
        limiter.process(buffer);
    }
    
    auto benchEnd = std::chrono::high_resolution_clock::now();
    auto benchDuration = std::chrono::duration_cast<std::chrono::microseconds>(benchEnd - benchStart);
    double benchTimeMs = static_cast<double>(benchDuration.count()) / 1000.0;
    double averageTimeMs = benchTimeMs / iterations;
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    // Check that we didn't crash and processing completed within reasonable time
    bool passed = (averageTimeMs < 1.0) && (benchTimeMs > 0.0);  // Should be fast
    
    std::string details = std::to_string(iterations) + " iterations in " + 
                         std::to_string(benchTimeMs) + "ms (" + 
                         std::to_string(averageTimeMs) + "ms per buffer)";
    
    return DSPTestFramework::TestResult("Limiter Performance", passed, details, timeMs);
}

// ========================================================================
// STRESS TESTS
// ========================================================================

DSPTestFramework::TestResult stressTestLargeBuffers() {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Test with very large buffer (8192 samples, like some high-latency setups)
    AudioBuffer buffer(8, 8192);  // 8 channels, 8192 samples
    TestSignalGenerator::generateNoise(buffer, 0.5);
    
    Compressor compressor;
    compressor.setSampleRate(48000);
    compressor.setParameter("threshold", -12.0);  // Lower threshold for noise
    compressor.setParameter("ratio", 6.0);
    
    ParametricEQ eq;
    eq.setSampleRate(48000);
    eq.setParameter("band2_gain", 4.0);
    eq.setParameter("band6_gain", -3.0);
    
    // Process through both effects
    compressor.process(buffer);
    eq.process(buffer);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    // Check that we didn't crash and processing completed
    double finalRMS = TestSignalGenerator::getRMSLevel(buffer);
    bool passed = (finalRMS > 0.0 && timeMs < 50.0);  // Should complete quickly
    
    std::string details = "8ch x 8192 samples processed in " + std::to_string(timeMs) + 
                         "ms, Final RMS: " + std::to_string(finalRMS);
    
    return DSPTestFramework::TestResult("Large Buffer Stress Test", passed, details, timeMs);
}

// ========================================================================
// CHORUS TESTS
// ========================================================================

DSPTestFramework::TestResult testChorusProcessing() {
    // Create test buffer with sine wave
    AudioBuffer buffer(2, 1024);
    TestSignalGenerator::generateSine(buffer, 440.0, 0.5, 48000);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Create and configure Chorus
    Chorus chorus;
    chorus.setSampleRate(48000);
    chorus.setDepth(0.7);       // Strong modulation
    chorus.setRate(0.5);        // 0.5 Hz LFO
    chorus.setDelay(15.0);      // 15ms base delay
    chorus.setFeedback(0.3);    // Moderate feedback
    chorus.setMix(0.6);         // 60% wet
    chorus.setVoices(4);        // 4 chorus voices
    
    // Process audio
    chorus.process(buffer);
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    // Verify processing
    double outputRMS = TestSignalGenerator::getRMSLevel(buffer);
    double stereoWidth = TestSignalGenerator::getStereoWidth(buffer);
    
    // Chorus should maintain energy and add stereo width
    bool passed = (outputRMS > 0.3 && outputRMS < 0.8 &&  // Energy maintained
                   stereoWidth > 0.1 &&                    // Stereo spread added
                   timeMs < 5.0);                           // Performance target
    
    std::string details = "Output RMS: " + std::to_string(outputRMS) + 
                         ", Stereo width: " + std::to_string(stereoWidth) + 
                         ", Latency: " + std::to_string(chorus.getLatency()) + " samples";
    
    return DSPTestFramework::TestResult("Chorus Processing", passed, details, timeMs);
}

DSPTestFramework::TestResult testChorusParameterControl() {
    AudioBuffer buffer(2, 512);
    TestSignalGenerator::generateSine(buffer, 1000.0, 0.5, 48000);
    
    Chorus chorus;
    chorus.setSampleRate(48000);
    
    // Test parameter validation and control
    bool paramsValid = true;
    std::string paramDetails;
    
    // Test depth control
    chorus.setDepth(0.8);
    if (chorus.getParameter("depth") != 0.8) {
        paramsValid = false;
        paramDetails += "Depth control failed. ";
    }
    
    // Test rate control
    chorus.setRate(1.5);
    if (std::abs(chorus.getParameter("rate") - 1.5) > 0.001) {
        paramsValid = false;
        paramDetails += "Rate control failed. ";
    }
    
    // Test delay control
    chorus.setDelay(25.0);
    if (std::abs(chorus.getParameter("delay") - 25.0) > 0.001) {
        paramsValid = false;
        paramDetails += "Delay control failed. ";
    }
    
    // Test voice count
    chorus.setVoices(6);
    if (chorus.getParameter("voices") != 6.0) {
        paramsValid = false;
        paramDetails += "Voice count control failed. ";
    }
    
    // Test mix control
    chorus.setMix(0.75);
    if (std::abs(chorus.getParameter("mix") - 0.75) > 0.001) {
        paramsValid = false;
        paramDetails += "Mix control failed. ";
    }
    
    // Test stereo width
    chorus.setStereoWidth(1.5);
    if (std::abs(chorus.getParameter("stereoWidth") - 1.5) > 0.001) {
        paramsValid = false;
        paramDetails += "Stereo width control failed. ";
    }
    
    // Test chorus modes
    chorus.setChorusMode(Chorus::ChorusMode::Ensemble);
    chorus.setChorusMode(Chorus::ChorusMode::Wide);
    chorus.setChorusMode(Chorus::ChorusMode::Vintage);
    
    // Test LFO waveforms
    chorus.setLFOWaveform(Chorus::LFOWaveform::Triangle);
    chorus.setLFOWaveform(Chorus::LFOWaveform::Sawtooth);
    chorus.setLFOWaveform(Chorus::LFOWaveform::Random);
    
    // Verify the effect is active
    bool isActive = chorus.isActive();
    if (!isActive) {
        paramsValid = false;
        paramDetails += "Chorus not active with mix > 0. ";
    }
    
    // Process to ensure no crashes with various settings
    try {
        chorus.process(buffer);
    } catch (...) {
        paramsValid = false;
        paramDetails += "Processing crashed with configured parameters. ";
    }
    
    if (paramsValid) {
        paramDetails = "All parameter controls working correctly";
    }
    
    return DSPTestFramework::TestResult("Chorus Parameter Control", paramsValid, paramDetails);
}

DSPTestFramework::TestResult testChorusModulationEffects() {
    AudioBuffer buffer(2, 2048);  // Longer buffer for modulation analysis
    TestSignalGenerator::generateSine(buffer, 800.0, 0.4, 48000);
    
    Chorus chorus;
    chorus.setSampleRate(48000);
    chorus.setDepth(0.9);        // Maximum depth for clear modulation
    chorus.setRate(2.0);         // Faster LFO for analysis
    chorus.setDelay(20.0);       // 20ms base delay
    chorus.setMix(1.0);          // 100% wet for pure effect analysis
    chorus.setVoices(3);         // 3 voices for complexity
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Test different waveforms
    std::vector<Chorus::LFOWaveform> waveforms = {
        Chorus::LFOWaveform::Sine,
        Chorus::LFOWaveform::Triangle,
        Chorus::LFOWaveform::Sawtooth,
        Chorus::LFOWaveform::Random
    };
    
    bool modulationWorking = true;
    std::string details = "";
    
    for (auto waveform : waveforms) {
        // Reset and configure
        AudioBuffer testBuffer(2, 1024);
        TestSignalGenerator::generateSine(testBuffer, 600.0, 0.4, 48000);
        
        chorus.reset();
        chorus.setLFOWaveform(waveform);
        
        // Process and analyze
        chorus.process(testBuffer);
        
        double outputRMS = TestSignalGenerator::getRMSLevel(testBuffer);
        
        // All waveforms should produce audible output
        if (outputRMS < 0.1) {
            modulationWorking = false;
            details += "Waveform failed to produce output. ";
            break;
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double timeMs = static_cast<double>(duration.count()) / 1000.0;
    
    // Test LFO phase tracking
    double lfoPhase = chorus.getLFOPhase();
    double modValue = chorus.getModulationValue();
    
    bool phaseValid = (lfoPhase >= 0.0 && lfoPhase <= 1.0);
    bool modValid = (modValue >= -1.0 && modValue <= 1.0);
    
    if (!phaseValid || !modValid) {
        modulationWorking = false;
        details += "LFO tracking out of range. ";
    }
    
    if (modulationWorking) {
        details = "All LFO waveforms working correctly. Phase: " + 
                 std::to_string(lfoPhase) + ", Mod: " + std::to_string(modValue);
    }
    
    return DSPTestFramework::TestResult("Chorus Modulation Effects", modulationWorking, details, timeMs);
}

// ========================================================================
// MAIN TEST RUNNER
// ========================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "DAWG ENGINE DSP COMPREHENSIVE TEST SUITE" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Testing real audio processing functionality" << std::endl;
    std::cout << "and performance benchmarking..." << std::endl << std::endl;
    
    DSPTestFramework framework;
    
    // Compressor Tests
    std::cout << "COMPRESSOR TESTS:" << std::endl;
    framework.runTest("Compressor Gain Reduction", testCompressorGainReduction);
    framework.runTest("Compressor Bypass", testCompressorBypass);
    
    // Parametric EQ Tests
    std::cout << "\nPARAMETRIC EQ TESTS:" << std::endl;
    framework.runTest("ParametricEQ Basic Processing", testParametricEQBasicProcessing);
    framework.runTest("ParametricEQ Parameter Control", testParametricEQParameterControl);
    
    // AlgorithmicReverb Tests
    std::cout << "\nALGORITHMIC REVERB TESTS:" << std::endl;
    framework.runTest("AlgorithmicReverb Processing", testAlgorithmicReverbProcessing);
    framework.runTest("AlgorithmicReverb Parameter Control", testAlgorithmicReverbParameterControl);
    
    // Limiter Tests
    std::cout << "\nLIMITER TESTS:" << std::endl;
    framework.runTest("Limiter Processing", testLimiterProcessing);
    framework.runTest("Limiter Parameter Control", testLimiterParameterControl);
    
    // Delay Tests
    std::cout << "\nDELAY TESTS:" << std::endl;
    framework.runTest("Delay Processing", testDelayProcessing);
    framework.runTest("Delay Parameter Control", testDelayParameterControl);
    
    // Chorus Tests
    std::cout << "\nCHORUS TESTS:" << std::endl;
    framework.runTest("Chorus Processing", testChorusProcessing);
    framework.runTest("Chorus Parameter Control", testChorusParameterControl);
    framework.runTest("Chorus Modulation Effects", testChorusModulationEffects);
    
    // Performance Benchmarks
    std::cout << "\nPERFORMANCE BENCHMARKS:" << std::endl;
    framework.runTest("Compressor Performance", benchmarkCompressorPerformance);
    framework.runTest("ParametricEQ Performance", benchmarkParametricEQPerformance);
    framework.runTest("AlgorithmicReverb Performance", benchmarkAlgorithmicReverbPerformance);
    framework.runTest("Limiter Performance", benchmarkLimiterPerformance);
    framework.runTest("Delay Performance", benchmarkDelayPerformance);
    
    // Stress Tests
    std::cout << "\nSTRESS TESTS:" << std::endl;
    framework.runTest("Large Buffer Stress Test", stressTestLargeBuffers);
    
    // Print comprehensive summary
    framework.printSummary();
    
    return 0;
}
