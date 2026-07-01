# Using VersaTone Audio Engine in Nexus Applications

The VersaTone audio engine (DAWG) is designed to be easily integrated into other C++/CMake applications within the Nexus Systems ecosystem.

## Installation

The engine is built and installed as part of the VersaTone application. To install it for use by other projects:

```bash
# From the VersaTone/b>From the VersaTone directory
mkdir build && cd build
cmake .. -G Ninja
ninja install
```

This will install the headers, libraries, and CMake configuration files to your system's default installation prefix (usually `/usr/local` on Unix or `C:\Program Files` on Windows).

## Using in Your CMake Project

Once installed, you can find and use the VersaTone engine in your CMake project with `find_package()`:

### Basic Usage

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyNexusApp)

# Find the VersaTone audio engine
find_package(DawgEngine REQUIRED)

# Your executable
add_executable(my_app main.cpp)

# Link against the engine
target_link_libraries(my_app PRIVATE Dawg::dawg_engine)
```

### Using in Nexus-Modeling

To integrate with Nexus-Modeling, add the following to `apps/Nexus-Modeling/src/kernel/CMakeLists.txt`:

1. Find the package:
```cmake
# Find DawgEngine (VersaTone audio engine)
find_package(DawgEngine QUIET)  # QUIET so it's optional
if(DAWGENGINE_FOUND)
    # Add compile definition if found
    target_compile_definitions(nexus_gfx_core PRIVATE NEXUS_ENABLE_AUDIO)
endif()
```

2. Link against it (if found):
```cmake
if(DAWGENGINE_FOUND)
    target_link_libraries(nexus_gfx_core PRIVATE Dawg::dawg_engine)
endif()
```

3. Initialize the engine in your application (e.g., in `main.cpp`):
```cpp
#ifdef NEXUS_ENABLE_AUDIO
#include <dawg/dawg.h>
// Initialize audio engine
if (!dawg::initialize()) {
    // Handle initialization failure
}
// ... later in your shutdown code ...
dawg::shutdown();
#endif
```

## API Overview

The main entry point is through the `dawg` namespace:

```cpp
#include <dawg/dawg.h>

// Initialize the engine
bool success = dawg::initialize();
if (!success) {
    // Handle error
}

// Get the audio engine instance
dawg::core::AudioEngine& engine = dawg::getAudioEngine();

// Configure and start the engine
dawg::core::EngineConfig config = dawg::core::EngineConfig::defaultConfig();
engine.initialize(config);
engine.start();

// Use the engine...
engine.processAudio(outputBuffer);

// Shutdown when done
dawg::shutdown();
```

See the header files in `engine/include/dawg/` for the full API documentation.

## Configuration Options

When building VersaTone for integration, you can control how the library is built:

- `DAWG_ENGINE_BUILD_SHARED`: Build as shared library (OFF = static, default)
- `DAWG_ENGINE_ENABLE_SIMD`: Enable SIMD optimizations (ON = default)
- `DAWG_ENGINE_ENABLE_LTO`: Enable Link Time Optimization (ON = default)

Example:
```bash
cmake .. -DDAWG_ENGINE_BUILD_SHARED=ON  # Build as .so/.dll instead of .a/.lib
```

## Notes

- The engine is designed to be GUI-independent and can be used in headless applications
- Audio processing should be done on a real-time appropriate thread
- The engine uses C++23 and requires a compatible compiler
- For detailed API documentation, see the header files in `engine/include/dawg/`