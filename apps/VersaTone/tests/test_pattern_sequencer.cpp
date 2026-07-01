#include "PatternSequencer.h"
#include "SineGen.h"
#include "AudioBuffer.h"
#include "WavWriter.h"
#include <iostream>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main() {
    std::cout << "Testing Pattern Sequencer...\n";
    
    // Create a pattern sequencer with one pattern
    PatternSequencer sequencer;
    Pattern* pattern = sequencer.addPattern("Test Pattern");
    
    // Set up a simple 16-step pattern
    pattern->setStepCount(16);
    
    // Add some beats - kick on 1, 5, 9, 13 (every 4 steps)
    pattern->setStep(0, 0, 127, 60);  // C4 kick on step 1
    pattern->setStep(0, 4, 127, 60);  // C4 kick on step 5
    pattern->setStep(0, 8, 127, 60);  // C4 kick on step 9
    pattern->setStep(0, 12, 127, 60); // C4 kick on step 13
    
    // Add hi-hats on off-beats (steps 2, 4, 6, 8, 10, 12, 14, 16)
    for (int step = 1; step < 16; step += 2) {
        pattern->setStep(1, step, 100, 72); // C5 hi-hat
    }
    
    // Add snare on steps 4 and 12
    pattern->setStep(2, 3, 127, 65);  // F4 snare on step 4
    pattern->setStep(2, 11, 127, 65); // F4 snare on step 12
    
    std::cout << "Pattern created with " << pattern->stepCount << " steps\n";
    std::cout << "Pattern has " << pattern->tracks.size() << " tracks\n";
    
    // Test pattern functionality
    std::cout << "\nPattern content:\n";
    for (int track = 0; track < (int)pattern->tracks.size(); ++track) {
        std::cout << "Track " << track << " (" << pattern->tracks[track]->name << "): ";
        for (int step = 0; step < pattern->stepCount; ++step) {
            if (pattern->isStepActive(track, step)) {
                Step stepData = pattern->getStepData(track, step);
                std::cout << "[" << step + 1 << ":N" << (int)stepData.note << "V" << (int)stepData.velocity << "] ";
            }
        }
        std::cout << "\n";
    }
    
    // Generate audio from the pattern using sine waves
    std::cout << "\nGenerating audio from pattern...\n";
    
    int sampleRate = 44100;
    double bpm = 120.0;
    double beatsPerSecond = bpm / 60.0;
    double stepLengthSeconds = pattern->stepLength / beatsPerSecond;
    double patternLengthSeconds = pattern->getTotalLength() / beatsPerSecond;
    int totalSamples = (int)(patternLengthSeconds * sampleRate);
    
    AudioBuffer buffer(2, totalSamples); // Stereo
    
    // Clear buffer
    for (size_t ch = 0; ch < buffer.channels(); ++ch) {
        for (size_t i = 0; i < buffer.samples(); ++i) {
            buffer.sample(ch, i) = 0.0;
        }
    }
    
    // Render each active step
    for (int track = 0; track < (int)pattern->tracks.size(); ++track) {
        for (int step = 0; step < pattern->stepCount; ++step) {
            if (pattern->isStepActive(track, step)) {
                Step stepData = pattern->getStepData(track, step);
                
                // Calculate timing
                double stepStartTime = step * stepLengthSeconds;
                int stepStartSample = (int)(stepStartTime * sampleRate);
                int stepLengthSamples = (int)(stepLengthSeconds * sampleRate / 4); // Short note
                
                // Generate frequency based on MIDI note
                double frequency = 440.0 * std::pow(2.0, (stepData.note - 69) / 12.0);
                
                // Generate sine wave for this step
                for (int i = 0; i < stepLengthSamples && (stepStartSample + i) < totalSamples; ++i) {
                    double t = i / (double)sampleRate;
                    double amplitude = (stepData.velocity / 127.0) * 0.3; // Scale volume
                    
                    // Apply simple envelope (attack + decay)
                    double envelope = 1.0;
                    if (i < stepLengthSamples / 4) {
                        envelope = i / (double)(stepLengthSamples / 4); // Attack
                    } else {
                        envelope = 1.0 - (i - stepLengthSamples / 4) / (double)(stepLengthSamples * 3 / 4); // Decay
                    }
                    
                    double sample = amplitude * envelope * std::sin(2.0 * M_PI * frequency * t);
                    
                    // Add to buffer (stereo)
                    buffer.sample(0, stepStartSample + i) += sample; // Left
                    buffer.sample(1, stepStartSample + i) += sample; // Right
                }
            }
        }
    }
    
    // Save to WAV file
    try {
        writeWavFile("pattern_test.wav", buffer, sampleRate);
        std::cout << "Pattern audio saved to 'pattern_test.wav'\n";
        std::cout << "Pattern length: " << patternLengthSeconds << " seconds\n";
        std::cout << "BPM: " << bpm << "\n";
    } catch (const std::exception& e) {
        std::cout << "Failed to save pattern audio: " << e.what() << "\n";
        return 1;
    }
    
    // Test pattern operations
    std::cout << "\nTesting pattern operations...\n";
    
    // Duplicate pattern
    sequencer.duplicatePattern(0);
    std::cout << "Duplicated pattern. Now have " << sequencer.patterns.size() << " patterns\n";
    
    // Test randomization
    Pattern* pattern2 = sequencer.patterns[1].get();
    pattern2->name = "Random Pattern";
    pattern2->randomize(0.4);
    std::cout << "Created randomized pattern\n";
    
    std::cout << "\nPattern Sequencer test completed successfully!\n";
    return 0;
}
