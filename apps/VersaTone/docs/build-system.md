<!-- Generated: 2025-07-10 00:00:00 UTC -->

# Build System

DAWG uses CMake for cross-platform builds. All source, header, and test files are listed in `CMakeLists.txt`.

## Build Workflows
- Configure: `cmake -S . -B build`
- Build: `cmake --build build`
- Run: `./build/dawg.exe` (main), `./build/test_sine.exe` (tests)

## Platform Setup
- Windows: MSYS2/MinGW or Visual Studio
- Linux: GCC/Clang

## Reference
- Main build config: `CMakeLists.txt`
- Output: `build/` directory
- Troubleshooting: Check CMake output for errors
