#!/usr/bin/env bash
# Proves the canary's search can fail: it plants a marker and must find it.
set -euo pipefail
bash "$(dirname "$0")/../../../scripts/privacy-canary.sh" --self-test
