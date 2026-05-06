# CI Tooling Versions

## Overview

This document pins versions of external tools used in CI to ensure reproducible builds and avoid breaking changes from tool updates.

## Pinned Versions

### Markdown Linting

- **Tool**: `markdownlint-cli`
- **Version**: `0.39.0`
- **Usage**: Lint markdown files (`README.md`, `docs/**/*.md`)
- **Installation**: `npm install -g markdownlint-cli@0.39.0`
- **CI Reference**: `.github/workflows/ci.yml` (lint job)
- **Last Updated**: 2026-02-04

### Workflow Linting

- **Tool**: `actionlint`
- **Version**: `1.6.27`
- **Usage**: Validate GitHub Actions workflow YAML syntax
- **Container**: `ghcr.io/rhysd/actionlint:1.6.27`
- **CI Reference**: `.github/workflows/ci.yml` (lint job)
- **Last Updated**: 2026-02-04

### Python Testing

- **Tool**: `pytest`
- **Version**: Managed via `requirements.txt` (not pinned in CI)
- **Extensions**:
  - `pytest-cov`: Coverage reporting
  - `pytest-timeout`: Test timeouts (30s default)
  - `pytest-rerunfailures`: Flake management (use sparingly)

### GitHub Actions

- **checkout**: `actions/checkout@v4`
- **setup-python**: `actions/setup-python@v5`
- **setup-node**: `actions/setup-node@v4` (Node 20)
- **upload-artifact**: `actions/upload-artifact@v4`

## Update Policy

- **Patch updates**: Apply automatically (e.g., `0.39.0` → `0.39.1`)
- **Minor updates**: Review release notes, test locally, then update (e.g., `0.39.x` → `0.40.x`)
- **Major updates**: Requires team approval and full CI validation (e.g., `1.x` → `2.x`)

## Local Testing

To run CI checks locally with pinned versions:

```bash
# Markdown lint
npm install -g markdownlint-cli@0.39.0
markdownlint README.md docs/**/*.md

# Workflow lint (Docker)
docker run --rm -v "$PWD:/repo" -w /repo \
  ghcr.io/rhysd/actionlint:1.6.27 -color

# Python tests
pip install pytest pytest-cov pytest-timeout pytest-rerunfailures
pytest tests/ -q --maxfail=1 --cov=src/nexuslang --cov-fail-under=75 --timeout=30
```

## Version Update Checklist

When updating a tool version:

1. Update version number in this document
2. Update CI workflow YAML (`.github/workflows/ci.yml`)
3. Test locally with new version
4. Run full CI pipeline on a feature branch
5. Update "Last Updated" date in this document
6. Commit with message: `chore: update <tool> to <version>`

## Rationale

- **markdownlint-cli 0.39.0**: Current stable release with MD040, MD047 rules active
- **actionlint 1.6.27**: Latest stable with YAML schema validation and expression checks
- **Actions v4/v5**: Latest major versions; v3 deprecated

## References

- [markdownlint-cli releases](https://github.com/igorshubovych/markdownlint-cli/releases)
- [actionlint releases](https://github.com/rhysd/actionlint/releases)
- [GitHub Actions versions](https://github.com/actions/)

