# Example: Exporting and Importing AIFF

This example demonstrates how to use the DAWg AIFF writer and reader to save and load a stereo sine wave.

```cpp
#include "AiffWriter.h"
#include "AudioBuffer.h"
#include <cmath>
#include <iostream>

int main() {
    // Generate 0.5s stereo 220Hz sine wave
    size_t sampleRate = 44100;
    size_t channels = 2;
    size_t samples = sampleRate / 2;
    dawg::AudioBuffer buffer(channels, samples);
    for (size_t i = 0; i < samples; ++i) {
        double t = i / static_cast<double>(sampleRate);
        double s = std::sin(2.0 * 3.141592653589793 * 220.0 * t);
        buffer.sample(0, i) = s;
        buffer.sample(1, i) = -s;
    }
    // Export
    if (!dawg::exportAiff("example_out.aiff", buffer)) {
        std::cerr << "Failed to export AIFF!\n";
        return 1;
    }
    // Import
    dawg::AudioBuffer loaded(2, 1);
    if (!dawg::importAiff("example_out.aiff", loaded)) {
        std::cerr << "Failed to import AIFF!\n";
        return 2;
    }
    std::cout << "First sample left: " << loaded.sample(0, 0) << "\n";
    std::cout << "First sample right: " << loaded.sample(1, 0) << "\n";
    return 0;
}
```

- See `tests/test_aiff.cpp` for a full test.
- Output: `example_out.aiff` will be created in the working directory.
