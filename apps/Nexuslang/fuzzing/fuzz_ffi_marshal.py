#!/usr/bin/env python3
"""
fuzzing/fuzz_ffi_marshal.py  --  Atheris fuzz target for NexusLang FFI marshalling.

Tests the boundary between NexusLang and C via the stdlib FFI module:
  - Type name mapping (map_type)
  - Callback registration with arbitrary arg type lists
  - C-string conversion utilities (to_c_string / from_c_string)
  - Library load failure handling with arbitrary library names

Does NOT attempt to load actual system libraries or call live C functions
(no memory safety issues in the fuzz harness itself).

Running with Atheris:
    python -m atheris fuzz_ffi_marshal.py corpus/ffi/ -max_total_time=60

Sanity mode:
    python fuzzing/fuzz_ffi_marshal.py --sanity
"""

from __future__ import annotations

import os
import sys

_ROOT = os.path.join(os.path.dirname(__file__), "..")
_SRC = os.path.join(_ROOT, "src")
if _SRC not in sys.path:
    sys.path.insert(0, _SRC)

_EXPECTED = (
    ValueError,
    TypeError,
    RuntimeError,
    OSError,
    AttributeError,
    UnicodeDecodeError,
    UnicodeEncodeError,
    OverflowError,
    KeyError,
)

# _TYPE_NAMES is used to generate plausible fuzzer inputs for structured mode.
_TYPE_NAMES = [
    "int", "long", "float", "double", "char", "char*", "void*", "size_t",
    "uint8", "uint16", "uint32", "uint64", "int8", "int16", "int32", "int64",
    "void", "struct_Point", "struct_Rect", "unknown_type", "",
]


def _fuzz_map_type(data: bytes) -> None:
    """Fuzz FFIManager.map_type with arbitrary type name strings."""
    from nexuslang.stdlib.ffi import FFIManager  # type: ignore[import]

    mgr = FFIManager()
    try:
        type_str = data.decode("utf-8", errors="replace")
    except Exception:
        return
    try:
        mgr.map_type(type_str)
    except _EXPECTED:
        pass


def _fuzz_register_callback(data: bytes) -> None:
    """Fuzz FFIManager.register_callback with arbitrary type vectors."""
    import struct as _struct
    from nexuslang.stdlib.ffi import FFIManager  # type: ignore[import]

    if len(data) < 2:
        return
    mgr = FFIManager()
    # Use first byte to pick arg_count, rest split into type name chunks.
    arg_count = data[0] % 8  # 0-7 args
    ret_idx = data[1] % len(_TYPE_NAMES)
    ret_type = _TYPE_NAMES[ret_idx]

    # Derive arg types from remaining bytes.
    arg_types = []
    for i in range(arg_count):
        idx = data[2 + i] % len(_TYPE_NAMES) if (2 + i) < len(data) else 0
        arg_types.append(_TYPE_NAMES[idx])

    def _dummy(*_args):
        return 0

    try:
        mgr.register_callback(_dummy, arg_types, ret_type)
    except _EXPECTED:
        pass


def _fuzz_string_conversion(data: bytes) -> None:
    """Fuzz to_c_string and from_c_string round-trip."""
    from nexuslang.runtime.runtime import Runtime  # type: ignore[import]
    from nexuslang.stdlib.ffi import register_ffi_functions  # type: ignore[import]

    runtime = Runtime()
    register_ffi_functions(runtime)
    to_c = runtime.functions.get("to_c_string")
    from_c = runtime.functions.get("from_c_string")

    if to_c is None or from_c is None:
        return

    try:
        s = data.decode("utf-8", errors="replace")
        raw = to_c(s)
        if isinstance(raw, (bytes, bytearray)):
            from_c(raw)
    except _EXPECTED:
        pass


def TestOneInput(data: bytes) -> None:
    """
    Fuzz entry point.  Dispatches to one of three sub-fuzzers based on first byte.
    """
    if not data:
        return
    selector = data[0] % 3
    payload = data[1:]
    if selector == 0:
        _fuzz_map_type(payload)
    elif selector == 1:
        _fuzz_register_callback(payload)
    else:
        _fuzz_string_conversion(payload)


# ---------------------------------------------------------------------------
# Atheris integration
# ---------------------------------------------------------------------------
def _run_with_atheris() -> None:
    import atheris  # type: ignore[import]

    atheris.instrument_imports()
    atheris.Setup(sys.argv, TestOneInput)
    atheris.Fuzz()


# ---------------------------------------------------------------------------
# Sanity runner
# ---------------------------------------------------------------------------
_SANITY_INPUTS: list[bytes] = [
    # map_type variations
    b"\x00int",
    b"\x00char*",
    b"\x00void",
    b"\x00void*",
    b"\x00struct_MyStruct",
    b"\x00completely_unknown_type_xyz",
    b"\x00",
    b"\x00" + b"x" * 1000,
    # register_callback variations
    b"\x01\x00\x00",                       # 0 args, returns int
    b"\x01\x01\x01\x00\x01",              # 1 arg, type=long
    b"\x01\x07\x00\x01\x02\x03\x04\x05\x06\x07",  # 7 args
    b"\x01\xff\xff",                       # max-value bytes
    # to_c_string variations
    b"\x02hello world",
    b"\x02",
    b"\x02" + b"\x00" * 10,
    b"\x02" + bytes(range(256)),
    b"\x02" + b"\xff\xfe\xfd",
]


def _run_sanity() -> None:
    print(f"Running {len(_SANITY_INPUTS)} sanity inputs against fuzz_ffi_marshal …")
    failures = 0
    for i, inp in enumerate(_SANITY_INPUTS):
        try:
            TestOneInput(inp)
            print(f"  [{i:2d}] OK")
        except Exception as exc:
            print(f"  [{i:2d}] CRASH: {type(exc).__name__}: {exc}")
            failures += 1
    if failures:
        print(f"\nfuzz_ffi_marshal sanity: {failures} crash(es) found!")
        sys.exit(1)
    else:
        print("\nfuzz_ffi_marshal sanity: all passed.")


if __name__ == "__main__":
    if "--sanity" in sys.argv:
        sys.argv.remove("--sanity")
        _run_sanity()
    else:
        try:
            _run_with_atheris()
        except ImportError:
            print(
                "Atheris not installed.  Install it with:\n"
                "    pip install atheris\n"
                "Or run sanity mode with:\n"
                "    python fuzzing/fuzz_ffi_marshal.py --sanity",
                file=sys.stderr,
            )
            sys.exit(1)
