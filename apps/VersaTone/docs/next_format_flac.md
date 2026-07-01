# TODO: Add FLAC support

- Implement `FlacWriter.h`/`FlacWriter.cpp` for 16-bit/24-bit FLAC export and import.
- Provide test and example usage similar to AIFF and WAV.
- Reference: [FLAC format specification](https://xiph.org/flac/format.html)

---

# Next Format: FLAC

The next recommended format to implement is FLAC, as it is lossless and widely used for high-quality audio archiving.

## Steps
1. Create `include/FlacWriter.h` and `src/FlacWriter.cpp`.
2. Add `exportFlac` and `importFlac` functions with the same interface as AIFF/WAV.
3. Add a test: `tests/test_flac.cpp`.
4. Add documentation and example usage.

---

See also: `docs/example_aiff.md`, `docs/test_aiff.md` for reference structure.
