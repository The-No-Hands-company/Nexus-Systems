#include <iostream>
#include "dawg/core/VoiceManager.h"

/**
 * MIDI Processor Example
 * Shows how to use DAWG Engine for MIDI processing in any application
 */

int main() {
    std::cout << "=== DAWG Engine: MIDI Processing Example ===\n\n";
    
    // Initialize engine for MIDI processing
    dawg::core::VoiceManager midiProcessor(512, 4096, 16000);
    
    std::cout << "MIDI processor ready with " << midiProcessor.getTotalVoiceCapacity() << " voices\n";
    std::cout << "Perfect for integration into:\n";
    std::cout << "- Digital Audio Workstations\n";
    std::cout << "- Live performance software\n";
    std::cout << "- Hardware MIDI controllers\n";
    std::cout << "- Educational software\n";
    std::cout << "- Game audio systems\n\n";
    
    return 0;
}
