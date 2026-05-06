# CI and Test Hardening Plan (Draft)

## Goals

- Increase signal: broader Python coverage, LSP checks, and perf guardrails.
- Reduce noise: flake controls, retry strategy, consistent logging and artifacts.
- Improve visibility: coverage reporting, quick failure triage, actionable alerts.

## Scope

- GitHub Actions (primary), optional local parity via `scripts/ci_local.sh`.
- Targets: parser/interpreter tests, stdlib tests, LSP smoke, perf smoke, benchmarks (sampling), docs lint, workflow lint.

## Matrix and Runners

- Python versions: 3.10, 3.11, 3.12, 3.13, 3.14-dev (allow-fail on dev until stable).
- OS: ubuntu-latest primary; weekly cron add macos-latest for portability.
- Cache: pip cache + repo-level `.cache/nlpl` guarded by key (python-version + lock hash).
- Concurrency: `concurrency: ci-${{ github.ref }}` to avoid stale runs per ref.

## Test Suite

- `pytest tests/` with `-q --maxfail=1` for fast fail; full `-v` on nightly.
- LSP smoke: start language server, open sample files, run hover/definition/references; ensure clean shutdown.
- Examples smoke: run top 5 examples (short list) to catch runtime regressions.
- Benchmarks smoke: run `benchmarks/benchmark_simple.py --quick` (or a new `--smoke` flag) to guard perf footprint.
- Docs lint: `markdownlint` (or `mdformat --check`) over `docs/`, `README.md`.
- Workflow lint: `actionlint` to catch YAML issues.

## Coverage and Reporting

- Use `coverage.py` with branch coverage; upload XML to GitHub summary + artifact.
- Optional Codecov upload (flagged, not blocking) with status only on default branch.
- Track coverage threshold locally (e.g., `--fail-under=75` in main job; nightly can be higher).

## Performance Guardrails

- Keep a small perf baseline: store last successful run numbers as artifact `perf-baseline.json`.
- Compare current smoke metrics (parse time for 5 files, interpreter microbench) with tolerance (e.g., +15%).
- If regression exceeds tolerance, mark job as failed (or warn-only on non-default branches initially).

## LSP Checks

- Start server in CI with `--no-persistent-cache --no-incremental` to isolate core correctness.
- Script to issue hover/definition/references on sample workspace and assert success and latency (<500ms per request budget for smoke).
- Verify shutdown and no lingering processes.

## Flake Management

- Mark known flaky tests with `@pytest.mark.flaky(reruns=2, reruns_delay=1)` sparingly; track in `tests/FLAKY.md`.
- Use `pytest-rerunfailures` only in targeted jobs; never hide deterministic failures.
- Add `timeout-minutes` per job (e.g., 15) and per-test timeouts via `--timeout=30` if using `pytest-timeout`.

## Artifacts and Logs

- Always upload: coverage XML, `pytest` junit XML, LSP logs, perf-smoke JSON, and minimal server logs.
- On failure: capture last 200 lines of logs in GitHub summary for quick triage.

## Security and Hygiene

- Pin Actions versions; enable `actions/dependency-review-action` on PRs.
- Use `python -m pip install --upgrade pip` and hash-locked dependencies when possible.

## Rollout Plan

- Phase 1: add matrix + pytest + coverage + markdownlint + actionlint (no perf/LSP yet); gate merges on main.
- Phase 2: add LSP smoke and perf smoke as warn-only for 1 week.
- Phase 3: flip LSP/perf checks to required; add weekly macOS cron.
- Phase 4: tune thresholds; revisit allow-fail on 3.14-dev once stable.

## Ownership

- CI config owners: @maintainers.
- Flake/allow-fail list owners: same group; review weekly.
- Perf baseline maintenance: team running `benchmarks/` owns updating tolerances.
