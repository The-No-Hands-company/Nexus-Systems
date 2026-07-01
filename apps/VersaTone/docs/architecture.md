<!-- Generated: 2025-07-10 00:00:00 UTC -->

# Architecture

DAWG is organized into modular C++ components for audio processing, file I/O, and testing. Each module is implemented as a header/source pair, with clear separation between DSP, file I/O, and test logic.

## Component Map
- AudioBuffer: `include/AudioBuffer.h`, `src/AudioBuffer.cpp`
- SineGen: `include/SineGen.h`, `src/SineGen.cpp`
- WavWriter: `include/WavWriter.h`, `src/WavWriter.cpp`
- Main: `src/main.cpp`
- Tests: `tests/test_sine.cpp`

## Key Files
- `include/AudioBuffer.h`: Double-precision audio buffer class
- `include/SineGen.h`: Sine wave generator interface
- `include/WavWriter.h`: WAV file writer interface
- `src/main.cpp`: Main entry point
- `tests/test_sine.cpp`: Unit test for DSP

## Data Flow
- Audio data is generated (e.g., sine wave) into `AudioBuffer`
- Buffer is written to disk via `WavWriter`
- Tests validate DSP output using asserts
