# Test: AIFF Import/Export

This test verifies that the AIFF writer and reader can round-trip a generated stereo sine wave.

- File: `tests/test_aiff.cpp`
- Builds as: `test_aiff`

## Test Steps
1. Generate a 1-second stereo 440Hz sine wave.
2. Export to `test_out.aiff` using `exportAiff`.
3. Import from `test_out.aiff` using `importAiff`.
4. Print the first sample value for verification.

## Code
```cpp
#include "AiffWriter.h"
#include "AudioBuffer.h"
#include <iostream>
#include <cmath>

int main() {
    size_t sampleRate = 44100;
    size_t channels = 2;
    size_t samples = sampleRate;
    dawg::AudioBuffer buffer(channels, samples);
    for (size_t i = 0; i < samples; ++i) {
        double t = i / static_cast<double>(sampleRate);
        double s = std::sin(2.0 * 3.141592653589793 * 440.0 * t);
        buffer.sample(0, i) = s;
        buffer.sample(1, i) = s;
    }
    if (dawg::exportAiff("test_out.aiff", buffer)) {
        std::cout << "Exported test_out.aiff successfully!\n";
    } else {
        std::cout << "Failed to export AIFF.\n";
    }
    dawg::AudioBuffer loaded(2, 1);
    if (dawg::importAiff("test_out.aiff", loaded)) {
        std::cout << "Imported test_out.aiff successfully!\n";
        std::cout << "First sample left: " << loaded.sample(0, 0) << "\n";
    } else {
        std::cout << "Failed to import AIFF.\n";
    }
    return 0;
}
```

## Run
```sh
./build/test_aiff.exe
```
