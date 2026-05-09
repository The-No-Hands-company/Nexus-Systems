# Showcase Workflow: Capability Demo App

This directory provides a polished, reproducible end-to-end workflow demo for NexusLang.

It includes:
- A multi-module sample app in `src/`
- Stable input fixtures in `fixtures/`
- Runtime sanitizer sample logs in `fixtures/runtime/`
- Output and benchmark snapshots in `snapshots/`
- One-command workflow and benchmark scripts in `scripts/`

## Project Layout

- `src/main.nxl`: Main report-focused workflow output
- `src/stage_prepare.nxl`: Preparation stage module
- `src/stage_transform.nxl`: Transformation stage module
- `src/stage_report.nxl`: Reporting stage module
- `fixtures/input_records.csv`: Reproducible input fixture
- `fixtures/runtime/sanitizer_sample.log`: Sanitizer parsing fixture
- `snapshots/expected_stdout.txt`: Golden output for main workflow
- `snapshots/benchmark_baseline.json`: Reference benchmark metrics

## End-to-End Pipeline

Run from repository root:

```bash
bash examples/showcase_workflow/scripts/run_pipeline.sh
```

Pipeline stages:
1. `lint` (static analyzer)
2. `check` (type/check pass)
3. `build`
4. `run`
5. `profile`
6. `run --analyze-runtime --sanitize address`

Runtime analysis artifacts are produced under:

```text
examples/showcase_workflow/build/runtime-analysis/
```

## Snapshot Validation

Reference output file:

```text
examples/showcase_workflow/snapshots/expected_stdout.txt
```

Compare current run output manually:

```bash
PYTHONPATH=src python -m nexuslang.main examples/showcase_workflow/src/main.nxl
```

## Benchmark Snapshot Workflow

Generate a fresh benchmark snapshot (default 15 iterations):

```bash
bash examples/showcase_workflow/scripts/benchmark_snapshot.sh
```

Output file:

```text
examples/showcase_workflow/snapshots/benchmark_current.json
```

Compare against baseline:

```text
examples/showcase_workflow/snapshots/benchmark_baseline.json
```
