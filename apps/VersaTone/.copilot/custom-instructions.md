---
description: Workspace rules and workflows for DAWg C++ digital audio workstation project
globs: '**/*.{cpp,h,md,cmake}'
---

# DAWg Project Copilot Instructions

## Project Context
- This is a C++ digital audio workstation (DAW) project focused on high-end sound quality.
- All audio/DSP code is written in C++ (not C), with double-precision floating point for internal processing.
- File I/O uses 32-bit float WAV as initial export format.
- Unit tests are required for all DSP and core logic (see `tests/`).
- Build system is CMake (see `CMakeLists.txt`).
- Documentation must reference concrete file paths and code examples.

## Coding Standards
- Use modern C++ (C++23 or later), RAII, and clear class/namespace structure.
- All new code must be documented with Doxygen-style comments.
- Prefer `#pragma once` in headers.
- Use `AudioBuffer` for all audio data (see `include/AudioBuffer.h`).
- All DSP code must be tested (see `tests/test_sine.cpp`).

## Common Workflows
- Add new DSP: Create header/source in `include/` and `src/`, add test in `tests/`.
- Add new file format: Implement in `src/`, add usage in `main.cpp`, update docs.
- Update documentation: Reference actual file paths and code snippets.
- Build: Use CMake (`CMakeLists.txt`), support Windows and Linux.

## Commit/Changelog Rules
- Use conventional commit format with emoji (see below).
- Update `CHANGELOG.md` for all user-facing changes, following Keep a Changelog format.
- Example: `/add-to-changelog 0.2.0 added "High-precision AudioBuffer class"`

## Commit Types
- ✨ feat: New features
- 🐛 fix: Bug fixes
- 📝 docs: Documentation changes
- ♻️ refactor: Code restructuring
- 🎨 style: Formatting
- ⚡️ perf: Performance improvements
- ✅ test: Tests
- 🧑‍💻 chore: Tooling/maintenance

## Testing
- All DSP and core logic must have unit tests in `tests/`.
- Use assert-based tests (see `tests/test_sine.cpp`).
- Run tests after every change.

## Documentation
- All docs must be LLM-optimized: concise, reference-heavy, and token-efficient.
- Always include file paths and code examples.
- See `README.md` and `docs/` for structure.

## Example File Reference
```cpp
// From include/AudioBuffer.h:10-20
class AudioBuffer {
    // ...
};
```

## Architecture/Patterns
- Modular, testable C++ code
- All audio processing via `AudioBuffer`
- File I/O and DSP in separate modules
- Use CMake for all builds

## LLM/AI Agent Instructions
- Always reference specific files and line numbers where possible
- Avoid duplication in documentation
- Use practical code examples from the codebase