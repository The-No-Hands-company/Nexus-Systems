/*
 * ████████  ███████ ███    ███ 
 *    █    █   █ █   ████  ████ 
 *    █    █   █   █ █ █████ █  
 *    █    █   █   █  ██  ██   
 *    █    ███████ █      █    
 * 
 * Digital Audio Workstation Engine
 * High-Performance C++ Audio Processing Framework
 * 
 * Copyright (c) 2025 The No-Hands Company
 * Licensed under MIT License
 * 
 * File: dawg.cpp
 * Purpose: Main API interface for DAWG audio engine
 * Author: DAWg Development Team
 * Created: 2025-06-28
 */

#include "dawg/dawg.h"
#include <dawg/engine/AudioEngine.h>

namespace dawg {
    
    // Global engine instance
    namespace {
        std::unique_ptr<engine::AudioEngine> g_engine;
    }
    
    bool initialize() {
        if (g_engine) {
            // Already initialized
            return true;
        }
        
        try {
            g_engine = std::make_unique<engine::AudioEngine>();
            return true;
        } catch (...) {
            return false;
        }
    }
    
    void shutdown() {
        g_engine.reset();
    }
    
    bool isInitialized() {
        return g_engine != nullptr;
    }
    
    engine::AudioEngine& getAudioEngine() {
        if (!g_engine) {
            throw std::runtime_error("AudioEngine not initialized. Call dawg::initialize() first.");
        }
        return *g_engine;
    }
    
} // namespace dawg