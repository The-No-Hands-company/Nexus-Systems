#include "dawg/dawg.h"
#include <iostream>

/**
 * @brief Basic DAWG audio engine example
 * Demonstrates fundamental usage of the audio engine
 */
int main() {
    std::cout << "DAWG Audio Engine - Basic Example\n";
    std::cout << "==================================\n\n";
    
    try {
        // Create audio engine with default configuration
        dawg::core::AudioEngine engine;
        
        std::cout << "✅ Audio engine created successfully\n";
        std::cout << "🎵 Basic example completed\n";
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
