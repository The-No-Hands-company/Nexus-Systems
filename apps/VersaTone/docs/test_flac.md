# Test: FLAC Import/Export

This test verifies that the FLAC writer and reader can round-trip a generated mono sine wave.

- File: `tests/test_flac.cpp`
- Builds as: `test_flac`

## Test Steps
1. Generate a 1-second mono 330Hz sine wave.
2. Export to `test_out.flac` using `exportFlac`.
3. Import from `test_out.flac` using `importFlac`.
4. Print the first sample value for verification.

## Code
```cpp
#include "FlacWriter.h"
#include "AudioBuffer.h"
#include <iostream>
#include <cmath>

int main() {
    size_t sampleRate = 44100;
    size_t channels = 1;
    size_t samples = sampleRate;
    dawg::AudioBuffer buffer(channels, samples);
    for (size_t i = 0; i < samples; ++i) {
        double t = i / static_cast<double>(sampleRate);
        double s = std::sin(2.0 * 3.141592653589793 * 330.0 * t);
        buffer.sample(0, i) = s;
    }
    if (dawg::exportFlac("test_out.flac", buffer)) {
        std::cout << "Exported test_out.flac successfully!\n";
    } else {
        std::cout << "Failed to export FLAC.\n";
    }
    dawg::AudioBuffer loaded(1, 1);
    if (dawg::importFlac("test_out.flac", loaded)) {
        std::cout << "Imported test_out.flac successfully!\n";
        std::cout << "First sample: " << loaded.sample(0, 0) << "\n";
    } else {
        std::cout << "Failed to import FLAC.\n";
    }
    return 0;
}
```

## Run
```sh
./build/test_flac.exe
```
