/**
 * @file standalone_integration_demo.cpp
 * @brief Demonstrates how easy it is to integrate DAWG engine into any project
 * 
 * This example shows:
 * 1. Zero GUI dependencies in the engine
 * 2. Simple integration with just a few includes
 * 3. Revolutionary 292,864+ voice capacity available anywhere
 * 4. Can be used in games, plugins, embedded systems, etc.
 */

#include <iostream>
#include "dawg/core/VoiceManager.h"  // Core engine component

int main() {
    std::cout << "=== DAWG Engine Standalone Integration Demo ===\n";
    std::cout << "Revolutionary 292,864+ Voice System\n\n";
    
    // Create voice manager with maximum capacity - NO GUI CODE NEEDED!
    dawg::core::VoiceManager voiceManager(4096, 32768, 256000);
    
    std::cout << "✅ Engine initialized successfully!\n";
    std::cout << "🚀 Voice Capacity: " << voiceManager.getTotalVoiceCapacity() << " voices\n\n";
    
    std::cout << "🎵 Ready for integration into ANY project type:\n";
    std::cout << "   - Game engines (Unity, Unreal, custom)\n";
    std::cout << "   - Audio plugins (VST, AU, AAX)\n";
    std::cout << "   - Desktop applications\n";
    std::cout << "   - Mobile apps\n";
    std::cout << "   - Embedded systems\n";
    std::cout << "   - Web applications (via WASM)\n";
    std::cout << "   - Scientific applications\n\n";
    
    // Demonstrate engine capabilities without ANY GUI code
    auto stats = voiceManager.getStats();
    std::cout << "📊 Engine Stats (ZERO GUI dependencies):\n";
    std::cout << "   Total Voices: " << stats.totalActiveVoices << "\n";
    std::cout << "   RAM Voices: " << stats.ramVoices << "\n";
    std::cout << "   Streaming Voices: " << stats.streamingVoices << "\n";
    std::cout << "   Virtual Voices: " << stats.virtualVoices << "\n";
    std::cout << "   CPU Usage: " << stats.totalCpuUsage << "%\n\n";
    
    std::cout << "💡 Integration is as simple as:\n";
    std::cout << "   1. #include \"dawg/core/VoiceManager.h\"\n";
    std::cout << "   2. link with libdawg_engine.a\n";
    std::cout << "   3. Create VoiceManager instance\n";
    std::cout << "   4. Use 292,864+ voices in your project!\n\n";
    
    std::cout << "🔥 ZERO dependencies on:\n";
    std::cout << "   ❌ Qt, GTK, or any GUI framework\n";
    std::cout << "   ❌ Graphics libraries\n";
    std::cout << "   ❌ Window managers\n";
    std::cout << "   ❌ Display systems\n";
    std::cout << "   ✅ Pure C++23 audio engine!\n\n";
    
    std::cout << "🎉 Engine ready! Perfect for industry domination!\n";
    return 0;
}
