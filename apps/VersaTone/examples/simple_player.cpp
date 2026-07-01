#include <iostream>
#include "dawg/core/VoiceManager.h"

/**
 * Simple Player Example
 * Demonstrates how to integrate DAWG Audio Engine into a basic audio player
 * Shows engine independence from any GUI framework
 */

int main() {
    std::cout << "=== DAWG Engine Integration Example: Simple Player ===\n\n";
    
    // Create engine instance - no GUI dependencies!
    dawg::core::VoiceManager engine(1024, 8192, 32000);
    
    std::cout << "Engine initialized successfully!\n";
    std::cout << "Total voice capacity: " << engine.getTotalVoiceCapacity() << " voices\n";
    std::cout << "Engine is completely GUI-independent and ready for integration.\n\n";
    
    std::cout << "This example shows how the DAWG Audio Engine can be:\n";
    std::cout << "- Integrated into any C++ project\n";
    std::cout << "- Used in command-line tools\n";
    std::cout << "- Embedded in game engines\n";
    std::cout << "- Used in mobile applications\n";
    std::cout << "- Integrated with any GUI framework (Qt, Dear ImGui, etc.)\n";
    std::cout << "- Used in web applications via WASM\n";
    std::cout << "- Embedded in plugins (VST, AU, etc.)\n\n";
    
    std::cout << "The engine requires ZERO external dependencies!\n";
    
    return 0;
}
