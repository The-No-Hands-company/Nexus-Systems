#!/bin/bash
D="$(dirname "$0")/build"
cmake --build "$D" -j"$(nproc)" >/dev/null 2>&1 || { echo "BUILD FAILED"; exit 1; }
ctest --test-dir "$D" >/tmp/ctest.out 2>&1
grep -q "tests passed" /tmp/ctest.out && echo "PASS" || { echo "FAIL"; exit 1; }
