<!-- Generated: 2025-07-10 00:00:00 UTC -->

# Testing

All DSP and core logic is covered by assert-based unit tests in the `tests/` directory. Tests are built and run via CMake.

## Test Types
- DSP correctness: `tests/test_sine.cpp`

## Running Tests
- Build: `cmake --build build`
- Run: `./build/test_sine.exe`
- Output: "All DSP tests passed."

## Reference
- Test file: `tests/test_sine.cpp`
- Build config: `CMakeLists.txt`
