# Example: Exporting and Importing FLAC

This example demonstrates how to use the DAWG FLAC writer and reader to save and load a mono sine wave.

```cpp
#include "FlacWriter.h"
#include "AudioBuffer.h"
#include <cmath>
#include <iostream>

int main() {
    // Generate 0.5s mono 220Hz sine wave
    size_t sampleRate = 44100;
    size_t channels = 1;
    size_t samples = sampleRate / 2;
    dawg::AudioBuffer buffer(channels, samples);
    for (size_t i = 0; i < samples; ++i) {
        double t = i / static_cast<double>(sampleRate);
        double s = std::sin(2.0 * 3.141592653589793 * 220.0 * t);
        buffer.sample(0, i) = s;
    }
    // Export
    if (!dawg::exportFlac("example_out.flac", buffer)) {
        std::cerr << "Failed to export FLAC!\n";
        return 1;
    }
    // Import
    dawg::AudioBuffer loaded(1, 1);
    if (!dawg::importFlac("example_out.flac", loaded)) {
        std::cerr << "Failed to import FLAC!\n";
        return 2;
    }
    std::cout << "First sample: " << loaded.sample(0, 0) << "\n";
    return 0;
}
```

- See `tests/test_flac.cpp` for a full test.
- Output: `example_out.flac` will be created in the working directory.
