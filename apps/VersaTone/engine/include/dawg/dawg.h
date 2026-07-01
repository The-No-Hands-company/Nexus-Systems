#pragma once

/**
 * @file dawg.h
 * @brief DAWG Audio Engine - Main public API header
 * @version 1.0.0
 * @date 2025-08-12
 * 
 * DAWG (Digital Audio Workstation with revolutionary multi-tier voice system)
 * Revolutionary audio engine providing 292,864+ simultaneous voices
 * 
 * Features:
 * - Multi-tier voice system (RAM + Streaming + Virtual voices)
 * - Professional mixing console with 32+ channels
 * - Advanced MIDI support with real-time sequencing
 * - Comprehensive plugin system (VST, internal plugins)
 * - High-quality audio I/O with multiple format support
 * - Real-time DSP effects and processing
 * - Modern C++23 architecture for maximum performance
 * 
 * Copyright (c) 2025 The No Hands Company
 * Licensed under MIT License
 */

// Version information
#define DAWG_VERSION_MAJOR 1
#define DAWG_VERSION_MINOR 0
#define DAWG_VERSION_PATCH 0
#define DAWG_VERSION_STRING "1.0.0"

// Core audio engine
#include "dawg/core/AudioEngine.h"
#include "dawg/core/AudioBuffer.h"
#include "dawg/core/VoiceManager.h"
#include "dawg/core/Transport.h"
#include "dawg/core/Timeline.h"

// Digital signal processing
#include "dawg/dsp/SineGenerator.h"
#include "dawg/dsp/Mixer.h"

// MIDI support
#include "dawg/midi/MidiFile.h"

// Audio I/O
#include "dawg/io/AudioFileReader.h"
#include "dawg/io/AudioFileWriter.h"
#include "dawg/io/StreamingEngine.h"
#include "dawg/io/DeviceManager.h"
#include "dawg/io/WavLoader.h"
#include "dawg/io/WavWriter.h"
#include "dawg/io/FlacWriter.h"
#include "dawg/io/Mp3Writer.h"
#include "dawg/io/AiffWriter.h"

// UI components (optional - only if building with UI)
#ifdef DAWG_BUILD_UI
#include "dawg/ui/MainWindow.h"
#include "dawg/ui/MixerConsole.h"
#include "dawg/ui/TimelineView.h"
#include "dawg/ui/PluginEditor.h"
#endif

/**
 * @namespace dawg
 * @brief Main namespace for the DAWG audio engine
 */
namespace dawg {

/**
 * @brief Initialize the DAWG audio engine
 * @return true if initialization successful, false otherwise
 */
bool initialize();

/**
 * @brief Shutdown the DAWG audio engine
 */
void shutdown();

/**
 * @brief Get the version string
 * @return Version string (e.g., "1.0.0")
 */
const char* getVersion();

/**
 * @brief Get the main audio engine instance
 * @return Reference to the global audio engine
 */
core::AudioEngine& getAudioEngine();

} // namespace dawg
