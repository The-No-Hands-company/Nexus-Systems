"""
src/nexuslang/stdlib_native/__init__.py -- Python API for the NexusLang native runtime library.

Provides:

    STDLIB_NATIVE_DIR   -- absolute path to this package directory
    INCLUDE_DIR         -- path to include/
    DEFAULT_BUILD_DIR   -- canonical CMake build output directory

    is_built()          -- True if libNLPL.a exists in DEFAULT_BUILD_DIR
    get_library_path()  -- path to libNLPL.a (or None if not built yet)
    get_link_flags()    -- list of linker flags for clang/gcc
    build()             -- build the library (requires cmake + cc)
    build_if_needed()   -- build only when the library is missing or stale

Usage in the compiler:
    from nexuslang.stdlib_native import build_if_needed, get_link_flags
    build_if_needed()
    extra_flags = get_link_flags()   # e.g. ["/path/to/libNLPL.a", "-lm"]
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from typing import List, Optional

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

# The directory that contains this __init__.py file.
STDLIB_NATIVE_DIR: str = os.path.dirname(os.path.abspath(__file__))

# The C include directory (contains nxl_runtime.h).
INCLUDE_DIR: str = os.path.join(STDLIB_NATIVE_DIR, "include")

# Default CMake build output directory.
# Placed inside the package itself so the library travels with the source tree.
DEFAULT_BUILD_DIR: str = os.path.join(STDLIB_NATIVE_DIR, "_cmake_build")

# Conventional library filename.
_LIB_NAME = "libNLPL.a"


# ---------------------------------------------------------------------------
# Availability checks
# ---------------------------------------------------------------------------

def _cmake_available() -> bool:
    """Return True if cmake is on PATH."""
    return shutil.which("cmake") is not None


def _make_available() -> bool:
    """Return True if make is on PATH."""
    return shutil.which("make") is not None


def _cc_available() -> bool:
    """Return True if a C compiler is on PATH."""
    return (
        shutil.which("cc") is not None
        or shutil.which("gcc") is not None
        or shutil.which("clang") is not None
    )


# ---------------------------------------------------------------------------
# State queries
# ---------------------------------------------------------------------------

def is_built(build_dir: str = DEFAULT_BUILD_DIR) -> bool:
    """Return True if libNLPL.a exists in *build_dir*."""
    return os.path.isfile(os.path.join(build_dir, _LIB_NAME))


def get_library_path(build_dir: str = DEFAULT_BUILD_DIR) -> Optional[str]:
    """Return the absolute path to libNLPL.a, or None if not yet built."""
    path = os.path.join(build_dir, _LIB_NAME)
    return path if os.path.isfile(path) else None


def get_link_flags(build_dir: str = DEFAULT_BUILD_DIR) -> List[str]:
    """Return linker flags to use when compiling with the NexusLang runtime.

    Returns the full path to libNLPL.a (so clang/gcc can resolve symbols
    without requiring a -rpath setting) followed by -lm (for libm functions
    used internally).

    Example::

        clang <obj> build/stdlib_native/libNLPL.a -lm -o program

    If the library has not been built yet, returns only ["-lm"] and prints a
    warning to stderr so the compilation can still proceed (functions will be
    resolved from the inline IR definitions in that case).
    """
    lib = get_library_path(build_dir)
    if lib is None:
        print(
            "WARNING: libNLPL.a not found. "
            "Run `python -c \"from nexuslang.stdlib_native import build; build()\"` "
            "to build the native runtime library.",
            file=sys.stderr,
        )
        return ["-lm"]
    return [lib, "-lm"]


# ---------------------------------------------------------------------------
# Build
# ---------------------------------------------------------------------------

def build(
    build_dir: str = DEFAULT_BUILD_DIR,
    build_type: str = "Release",
    verbose: bool = False,
) -> bool:
    """Build libNLPL.a from source using CMake.

    Parameters
    ----------
    build_dir:
        Directory in which to run the CMake build.  Created if absent.
    build_type:
        CMake build type (``Release``, ``Debug``, or ``RelWithDebInfo``).
    verbose:
        If True, forward compiler output to stdout/stderr.

    Returns
    -------
    bool
        True on success, False on failure.
    """
    if not _cmake_available():
        print(
            "ERROR: cmake not found. Install CMake to build the native runtime.",
            file=sys.stderr,
        )
        return _build_with_make(build_dir, verbose=verbose)

    if not _cc_available():
        print(
            "ERROR: No C compiler found (cc / gcc / clang). "
            "Install one to build the native runtime.",
            file=sys.stderr,
        )
        return False

    os.makedirs(build_dir, exist_ok=True)

    capture = {} if verbose else {"stdout": subprocess.DEVNULL, "stderr": subprocess.PIPE}

    # Configure
    configure_cmd = [
        "cmake",
        "-S", STDLIB_NATIVE_DIR,
        "-B", build_dir,
        f"-DCMAKE_BUILD_TYPE={build_type}",
    ]
    try:
        result = subprocess.run(configure_cmd, check=False, **capture)
        if result.returncode != 0:
            if not verbose and result.stderr:
                print(result.stderr.decode(errors="replace"), file=sys.stderr)
            print("ERROR: CMake configuration failed.", file=sys.stderr)
            return False
    except OSError as exc:
        print(f"ERROR: Failed to run cmake: {exc}", file=sys.stderr)
        return False

    # Build
    build_cmd = ["cmake", "--build", build_dir, "--config", build_type]
    try:
        result = subprocess.run(build_cmd, check=False, **capture)
        if result.returncode != 0:
            if not verbose and result.stderr:
                print(result.stderr.decode(errors="replace"), file=sys.stderr)
            print("ERROR: CMake build failed.", file=sys.stderr)
            return False
    except OSError as exc:
        print(f"ERROR: Failed to run cmake --build: {exc}", file=sys.stderr)
        return False

    # Copy archive to the top of build_dir if cmake placed it in a sub-dir.
    for candidate in [
        os.path.join(build_dir, _LIB_NAME),
        os.path.join(build_dir, "lib", _LIB_NAME),
        os.path.join(build_dir, "Release", _LIB_NAME),
        os.path.join(build_dir, "Debug", _LIB_NAME),
    ]:
        if os.path.isfile(candidate):
            target = os.path.join(build_dir, _LIB_NAME)
            if candidate != target:
                import shutil as _shutil
                _shutil.copy2(candidate, target)
            break

    if is_built(build_dir):
        if verbose:
            print(f"libNLPL built successfully: {os.path.join(build_dir, _LIB_NAME)}")
        return True

    print(
        f"ERROR: libNLPL.a not found in {build_dir} after build.",
        file=sys.stderr,
    )
    return False


def _build_with_make(
    build_dir: str = DEFAULT_BUILD_DIR,
    verbose: bool = False,
) -> bool:
    """Fallback build using the provided Makefile (no cmake required)."""
    if not _make_available():
        print("ERROR: neither cmake nor make found.", file=sys.stderr)
        return False

    if not _cc_available():
        print("ERROR: No C compiler found.", file=sys.stderr)
        return False

    capture = {} if verbose else {"stdout": subprocess.DEVNULL, "stderr": subprocess.PIPE}

    make_cmd = ["make", "-C", STDLIB_NATIVE_DIR, f"BUILD_DIR={build_dir}"]
    result = subprocess.run(make_cmd, check=False, **capture)
    if result.returncode != 0:
        if not verbose and result.stderr:
            print(result.stderr.decode(errors="replace"), file=sys.stderr)
        print("ERROR: make build failed.", file=sys.stderr)
        return False

    # The Makefile places libNLPL.a directly in BUILD_DIR.
    return is_built(build_dir)


def build_if_needed(
    build_dir: str = DEFAULT_BUILD_DIR,
    build_type: str = "Release",
    verbose: bool = False,
) -> bool:
    """Build libNLPL.a only if it is absent.

    Suitable for calling at the start of ``compile_to_executable()`` so the
    library is always available without requiring a separate build step.

    Returns True if the library is (or becomes) available.
    """
    if is_built(build_dir):
        return True
    return build(build_dir=build_dir, build_type=build_type, verbose=verbose)


# ---------------------------------------------------------------------------
# Convenience: header path
# ---------------------------------------------------------------------------

def get_include_flags() -> List[str]:
    """Return the -I flag needed to use nxl_runtime.h in C compilation."""
    return [f"-I{INCLUDE_DIR}"]
