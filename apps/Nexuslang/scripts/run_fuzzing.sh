#!/usr/bin/env bash
# scripts/run_fuzzing.sh
#
# Convenience script to run NexusLang fuzz targets locally.
#
# Usage:
#   ./scripts/run_fuzzing.sh                       # Sanity-check all targets (no Atheris)
#   ./scripts/run_fuzzing.sh --target lexer        # Fuzz the lexer (requires Atheris)
#   ./scripts/run_fuzzing.sh --target parser       # Fuzz the parser
#   ./scripts/run_fuzzing.sh --target interpreter  # Fuzz the interpreter
#   ./scripts/run_fuzzing.sh --target typechecker  # Fuzz the type checker
#   ./scripts/run_fuzzing.sh --target ffi          # Fuzz the FFI marshal
#   ./scripts/run_fuzzing.sh --target all          # Fuzz all targets sequentially
#   ./scripts/run_fuzzing.sh --sanity              # Sanity mode (all, no Atheris)
#
# Options:
#   --target  <name>    Which harness to run (lexer|parser|interpreter|typechecker|ffi|all)
#   --timeout <secs>    Max wall-clock per target in fuzz mode (default: 60)
#   --sanity            Run --sanity mode on all targets without Atheris
#   --help              Show this message

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FUZZING_DIR="$REPO_ROOT/fuzzing"
CRASH_DIR="$FUZZING_DIR/crashes"

TARGET="all"
TIMEOUT=60
MODE="fuzz"   # fuzz or sanity

# ---------------------------------------------------------------------------
# Argument parsing
# ---------------------------------------------------------------------------
while [[ $# -gt 0 ]]; do
    case "$1" in
        --target)
            TARGET="$2"; shift 2 ;;
        --timeout)
            TIMEOUT="$2"; shift 2 ;;
        --sanity)
            MODE="sanity"; shift ;;
        --help|-h)
            head -30 "$0" | grep "^#" | sed 's/^# \?//'
            exit 0 ;;
        *)
            echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

mkdir -p "$CRASH_DIR"

# ---------------------------------------------------------------------------
# Determine Python
# ---------------------------------------------------------------------------
if [[ -x "$REPO_ROOT/.venv/bin/python" ]]; then
    PYTHON="$REPO_ROOT/.venv/bin/python"
elif command -v python3 &>/dev/null; then
    PYTHON="python3"
else
    PYTHON="python"
fi

echo "Using Python: $PYTHON"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
_sanity() {
    local name="$1"
    echo ""
    echo "=== Sanity: $name ==="
    "$PYTHON" "$FUZZING_DIR/fuzz_${name}.py" --sanity
}

_fuzz() {
    local name="$1"
    local corpus="$FUZZING_DIR/corpus/$name"
    echo ""
    echo "=== Fuzzing: $name (${TIMEOUT}s) ==="

    if ! "$PYTHON" -c "import atheris" 2>/dev/null; then
        echo "  Atheris not installed.  Falling back to --sanity mode."
        echo "  Install with: pip install atheris"
        _sanity "$name"
        return
    fi

    mkdir -p "$corpus"
    timeout "${TIMEOUT}s" \
        "$PYTHON" "$FUZZING_DIR/fuzz_${name}.py" \
            "$corpus" \
            "-max_total_time=$TIMEOUT" \
            "-artifact_prefix=${CRASH_DIR}/${name}_" \
        || true

    # Report crashes
    local crashes
    crashes=$(find "$CRASH_DIR" -name "${name}_*" -type f 2>/dev/null | wc -l)
    if [[ "$crashes" -gt 0 ]]; then
        echo ""
        echo "  CRASHES FOUND: $crashes"
        find "$CRASH_DIR" -name "${name}_*" -type f | while read -r f; do
            echo "    $f  ($(wc -c < "$f") bytes)"
        done
        echo ""
        echo "  Reproduce with:"
        echo "    $PYTHON $FUZZING_DIR/fuzz_${name}.py <crash-file>"
    else
        echo "  No crashes found."
    fi
}

# ---------------------------------------------------------------------------
# Run
# ---------------------------------------------------------------------------
TARGETS=()
case "$TARGET" in
    all)
        TARGETS=(lexer parser interpreter typechecker ffi) ;;
    lexer|parser|interpreter|typechecker|ffi)
        TARGETS=("$TARGET") ;;
    *)
        echo "Unknown target: $TARGET" >&2
        echo "Valid: lexer parser interpreter typechecker ffi all" >&2
        exit 1 ;;
esac

FAILURES=0
for t in "${TARGETS[@]}"; do
    if [[ "$MODE" == "sanity" ]]; then
        _sanity "$t" || (( FAILURES++ )) || true
    else
        _fuzz "$t" || (( FAILURES++ )) || true
    fi
done

echo ""
if [[ "$FAILURES" -gt 0 ]]; then
    echo "Fuzzing run completed with $FAILURES failure(s)."
    exit 1
else
    echo "Fuzzing run completed successfully."
fi
