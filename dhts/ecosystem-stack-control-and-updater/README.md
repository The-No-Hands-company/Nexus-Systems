# Ecosystem Stack Control and Updater

This helper tracks stack freshness and security posture across the Nexus Systems ecosystem.

## What it does

- Scans the workspace for:
  - Node projects (`package.json`)
  - Python requirements (`requirements*.txt`)
  - Rust crates (`Cargo.toml`)
  - Container bases (`Dockerfile*`)
- Builds a normalized stack inventory report.
- Checks runtime drift, including Node.js current and LTS channels.
- Checks dependency freshness against:
  - npm registry
  - PyPI
  - crates.io
- Performs advisory checks via OSV for exact (pinned) versions.
- Writes a machine-readable JSON report and a Markdown summary.

## Why this exists

The goal is to keep the ecosystem continuously modern, patchable, and secure.

This tool is meant to run regularly (for example daily in CI and on-demand locally), so updates and vulnerabilities are visible early.

## Usage

Run from repo root:

```bash
python dhts/ecosystem-stack-control-and-updater/stack_control.py \
  --workspace . \
  --config dhts/ecosystem-stack-control-and-updater/stack-control.config.json \
  --json-out dhts/ecosystem-stack-control-and-updater/reports/latest.json \
  --md-out dhts/ecosystem-stack-control-and-updater/reports/latest.md
```

## Outputs

- JSON: full structured inventory and findings.
- Markdown: operator-friendly summary for dashboards/issues.

## Policy model

Policy is in `stack-control.config.json`:

- scan exclusions
- runtime channels and desired standards
- severity thresholds
- optional allowed drift exemptions

## Recommended automation

- Run once per day in CI.
- Open/update one "stack freshness" issue automatically when findings are non-empty.
- Treat critical security findings as a release blocker.

## Notes

- Advisory checks are strongest when dependencies are pinned to exact versions.
- Range-based specs are still inventoried and freshness-scored, but security matching is less precise.
- The tool is read-only; it reports what should be updated and why.
