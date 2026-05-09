#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
REPORT_PATH="${1:-${BUILD_DIR}/release_signoff_1.0-alpha.txt}"

mkdir -p "$(dirname "${REPORT_PATH}")"

timestamp="$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
commit="$(git -C "${ROOT_DIR}" rev-parse --short HEAD)"
branch="$(git -C "${ROOT_DIR}" rev-parse --abbrev-ref HEAD)"

log_tmp="$(mktemp)"
trap 'rm -f "${log_tmp}"' EXIT

run_and_capture() {
    local label="$1"
    shift
    echo "[gate] ${label}" >&2
    "$@" >"${log_tmp}" 2>&1
}

# 1) Build gate
run_and_capture "build" cmake --build "${BUILD_DIR}" -j"$(nproc)"
build_tail="$(tail -n 6 "${log_tmp}")"

# 2) API freeze audit gate
run_and_capture "api-freeze-audit" "${BUILD_DIR}/tests/nexus_kernel_tests" --gtest_filter="ApiFreezeAudit.*"
audit_summary="$(grep -E "\[  PASSED  \]|\[  FAILED  \]" "${log_tmp}" | tail -n 2 | tr '\n' '; ')"

# 3) Full suite gate
run_and_capture "full-test-suite" "${BUILD_DIR}/tests/nexus_kernel_tests"
passed_line="$(grep -E "\[  PASSED  \] [0-9]+ tests\." "${log_tmp}" | tail -n 1 || true)"
skipped_line="$(grep -E "\[  SKIPPED \] [0-9]+ tests" "${log_tmp}" | tail -n 1 || true)"
failed_line="$(grep -E "\[  FAILED  \] [0-9]+ tests" "${log_tmp}" | tail -n 1 || true)"

if [[ -z "${passed_line}" ]]; then
    echo "ERROR: Could not parse full suite pass line" >&2
    tail -n 40 "${log_tmp}" >&2
    exit 1
fi

if [[ -z "${skipped_line}" ]]; then
    skipped_line="[  SKIPPED ] 0 tests"
fi

if [[ -z "${failed_line}" ]]; then
    failed_line="[  FAILED  ] 0 tests"
fi

# 4) Perf determinism gate
run_and_capture "perf-determinism" "${BUILD_DIR}/tests/nexus_kernel_perf_smoke" --frames 64 --determinism-runs 3
perf_report="$(cat "${log_tmp}")"

if ! grep -q "determinism_consistent=true" "${log_tmp}"; then
    echo "ERROR: perf determinism gate failed" >&2
    cat "${log_tmp}" >&2
    exit 1
fi

{
    echo "NEXUS MODELING 1.0-ALPHA RELEASE GATE SIGNOFF"
    echo "timestamp_utc=${timestamp}"
    echo "branch=${branch}"
    echo "commit=${commit}"
    echo ""
    echo "build_status=PASS"
    echo "build_tail:"
    echo "${build_tail}"
    echo ""
    echo "api_freeze_audit_status=PASS"
    echo "api_freeze_audit_summary=${audit_summary}"
    echo ""
    echo "full_suite_status=PASS"
    echo "full_suite_passed_line=${passed_line}"
    echo "full_suite_skipped_line=${skipped_line}"
    echo "full_suite_failed_line=${failed_line}"
    echo ""
    echo "perf_determinism_status=PASS"
    echo "perf_determinism_report:"
    echo "${perf_report}"
    echo ""
    echo "overall_signoff=PASS"
} >"${REPORT_PATH}"

echo "Release gate signoff report written: ${REPORT_PATH}"
