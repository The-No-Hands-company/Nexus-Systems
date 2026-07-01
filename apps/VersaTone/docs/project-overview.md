<!-- Generated: 2025-07-10 00:00:00 UTC -->

# Project Overview

DAWG (Digital Audio Workstation Groundwork) is a C++ project for building a high-end, modular DAW focused on professional sound quality. It features double-precision DSP, modular architecture, and a CMake-based build system. The project is designed for extensibility and testability, with all DSP and core logic covered by unit tests.

## Key Files
- Main entry: `src/main.cpp`
- Audio buffer: `include/AudioBuffer.h`, `src/AudioBuffer.cpp`
- Sine generator: `include/SineGen.h`, `src/SineGen.cpp`
- WAV writer: `include/WavWriter.h`, `src/WavWriter.cpp`
- Build config: `CMakeLists.txt`
- Unit test: `tests/test_sine.cpp`

## Technology Stack
- C++23 (modern C++)
- CMake (build system)
- Assert-based unit tests

## Platform Support
- Windows, Linux (CMake-based)
