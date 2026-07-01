#!/bin/bash
# Build script for NexusLang Rc runtime library

set -e  # Exit on error

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
RUNTIME_DIR="$PROJECT_ROOT/src/nlpl/runtime"
BUILD_DIR="$PROJECT_ROOT/build/runtime"

# Create build directory
mkdir -p "$BUILD_DIR"

echo "Building NexusLang Rc runtime library..."
echo "Source: $RUNTIME_DIR/rc_runtime.c"
echo "Output: $BUILD_DIR/librc_runtime.a"

# Compile the runtime library
gcc -c \
    -std=c11 \
    -O2 \
    -fPIC \
    -Wall \
    -Wextra \
    -I"$RUNTIME_DIR" \
    "$RUNTIME_DIR/rc_runtime.c" \
    -o "$BUILD_DIR/rc_runtime.o"

# Create static library
ar rcs "$BUILD_DIR/librc_runtime.a" "$BUILD_DIR/rc_runtime.o"

# Also create a shared library for dynamic linking
gcc -shared \
    -O2 \
    -fPIC \
    "$RUNTIME_DIR/rc_runtime.c" \
    -o "$BUILD_DIR/librc_runtime.so"

echo "✓ Build successful!"
echo "  Static library:  $BUILD_DIR/librc_runtime.a"
echo "  Shared library:  $BUILD_DIR/librc_runtime.so"
echo ""
echo "To use in NexusLang programs:"
echo "  Link with: -L$BUILD_DIR -lrc_runtime"
