<!-- Generated: 2025-07-10 00:00:00 UTC -->

# Development

Development uses modern C++ (C++23), modular headers/sources, and assert-based tests. Code style is enforced via Doxygen comments and clear class/namespace structure.

## Code Style
- Use `#pragma once` in headers (see `include/AudioBuffer.h`)
- Doxygen comments for all public classes/functions
- RAII and modern C++ idioms

## Common Patterns
- Modular header/source for each component
- All audio data via `AudioBuffer`
- Tests in `tests/`, e.g. `tests/test_sine.cpp`

## Workflows
- Add DSP: `include/`, `src/`, test in `tests/`
- Add file format: `src/`, update `main.cpp`, docs
- Build: CMake (`CMakeLists.txt`)

## Reference
- File organization: `include/`, `src/`, `tests/`
- Naming: PascalCase for classes, snake_case for functions
