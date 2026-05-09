#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEFAULT_REPORT="${ROOT_DIR}/build/release_signoff_1.0-alpha.txt"
DEFAULT_TAG="v1.0.0-alpha"

REPORT_PATH="${DEFAULT_REPORT}"
TAG_NAME="${DEFAULT_TAG}"
DRY_RUN="false"

usage() {
    cat <<'EOF'
Usage: ./tools/create_alpha_tag.sh [--report <path>] [--tag <name>] [--dry-run]

Creates an annotated alpha tag only if the signoff report contains:
  overall_signoff=PASS

Defaults:
  --report build/release_signoff_1.0-alpha.txt
  --tag    v1.0.0-alpha

Options:
  --report <path>  Path to release signoff report
  --tag <name>     Tag name to create (annotated)
  --dry-run        Validate checks and print actions without creating tag
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --report)
            REPORT_PATH="$2"
            shift 2
            ;;
        --tag)
            TAG_NAME="$2"
            shift 2
            ;;
        --dry-run)
            DRY_RUN="true"
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: Unknown argument: $1" >&2
            usage
            exit 2
            ;;
    esac
done

if [[ ! -f "${REPORT_PATH}" ]]; then
    echo "ERROR: Signoff report not found: ${REPORT_PATH}" >&2
    exit 1
fi

signoff_line="$(grep -E '^overall_signoff=' "${REPORT_PATH}" | tail -n 1 || true)"
if [[ "${signoff_line}" != "overall_signoff=PASS" ]]; then
    echo "ERROR: Signoff report does not indicate PASS" >&2
    echo "Found: ${signoff_line:-<missing overall_signoff line>}" >&2
    exit 1
fi

report_commit="$(grep -E '^commit=' "${REPORT_PATH}" | tail -n 1 | cut -d'=' -f2 || true)"
head_commit="$(git -C "${ROOT_DIR}" rev-parse --short HEAD)"

if [[ -n "${report_commit}" && "${report_commit}" != "${head_commit}" ]]; then
    echo "ERROR: Report commit (${report_commit}) does not match HEAD (${head_commit})" >&2
    echo "Generate a fresh signoff report before tagging." >&2
    exit 1
fi

if git -C "${ROOT_DIR}" rev-parse -q --verify "refs/tags/${TAG_NAME}" >/dev/null; then
    echo "ERROR: Tag already exists: ${TAG_NAME}" >&2
    exit 1
fi

annot_msg="Nexus Modeling kernel 1.0-alpha\n\nValidated by release gate report: ${REPORT_PATH}\ncommit=${head_commit}"

if [[ "${DRY_RUN}" == "true" ]]; then
    echo "[dry-run] checks passed"
    echo "[dry-run] would create annotated tag: ${TAG_NAME}"
    echo "[dry-run] report: ${REPORT_PATH}"
    exit 0
fi

git -C "${ROOT_DIR}" tag -a "${TAG_NAME}" -m "${annot_msg}"
echo "Created annotated tag: ${TAG_NAME}"