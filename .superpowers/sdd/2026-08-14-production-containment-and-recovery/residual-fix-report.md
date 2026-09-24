# Residual Recovery Security Fix Report

Date: 2026-08-16

No live production action was performed. The work used isolated fixtures only: no credential file, running service, container, API, or production log was changed.

## Vulnerable paths and restored invariants

### Failure-atomic storage rollback

- Vulnerable path: `rollback-storage` and the pre-health `on_exit` recovery path in `deploy/production/rotate-production-secrets.sh`.
- Prior failure: credential keys were validated and rewritten sequentially. A later mismatch could leave root, Cloud, and deployed Chat on a partial old/new credential set while MinIO stayed on the new pair.
- Restored invariant: all affected files and all six credential assignments are snapshotted, uniqueness-checked, conditionally validated, rendered, permissioned, and post-validated before the first replacement. Rotation also uniqueness-checks both current and prepared files before its first mutation. Complete files preserve unrelated current lines. Stable adjacent lockfiles serialize repository-owned writers across snapshot, comparison, replacement, compensation, and MinIO health; the `with-storage-locks` phase is the documented mandatory cooperative edit path. A commit failure restores already-replaced files; failed compensation is reported and its mode-0600 recovery artifacts are retained.
- Fail-closed cases covered: a later-file credential mismatch, divergent duplicate assignments before rotation and rollback, staged-file permission failure, cooperative writer-lock contention, commit rename failure, and compensation rename failure.

### Fresh bounded S3 cleanup

- Vulnerable path: the error cleanup branch in `deploy/production/storage-probe.ts`, invoked by the Task 5 Cloud and deployed-Chat probes.
- Prior failure: cleanup DELETE reused the primary operation's already-aborted signal, so an ambiguous timed-out PUT could skip deletion immediately.
- Restored invariant: the primary timer is cleared before cleanup, DELETE receives a fresh `AbortController`, and cleanup has its own bounded deadline. The outer runbook guard is 300 seconds, exceeding the 120-second primary plus 120-second cleanup contract.

### Authoritative Nexus Chat registration environment

- Vulnerable path: fresh-shell `deploy/production/deploy.sh restart nexus-chat` preflight/start dispatch.
- Prior failure: selective restart did not adopt the protected Cloud URL/key required by Nexus's startup registration path.
- Restored invariant: preflight requires both protected values, and selective/full startup source the protected Cloud file with the same Bash last-assignment, quoting, and expansion semantics. The protected URL/key overlay occurs after Chat's environment is loaded, overriding stale inherited or Chat copies without putting the key in launcher arguments.

## Files changed

- `deploy/production/rotate-production-secrets.sh`
- `deploy/production/tests/rotation.test.sh`
- `deploy/production/storage-probe.ts`
- `deploy/production/tests/storage-probe.test.ts`
- `deploy/production/deploy.sh`
- `deploy/production/tests/processes.test.sh`
- `.superpowers/sdd/2026-08-14-production-containment-and-recovery/task-5-brief.md`
- `docs/superpowers/plans/2026-08-14-production-containment-and-recovery.md`
- `.superpowers/sdd/2026-08-14-production-containment-and-recovery/residual-fix-report.md`

No application submodule source changed, so there is no submodule commit.

## Verification

The original three issues were first reproduced with focused failing regressions. From the final tree:

- `timeout 90s bash deploy/production/tests/rotation.test.sh` — PASS, 23/23.
- `timeout 90s bash deploy/production/tests/processes.test.sh` — PASS, all 22 process/restart tests.
- `timeout 30s bun test deploy/production/tests/storage-probe.test.ts` — PASS, 4/4.
- `timeout 180s bun test deploy/production/tests` — PASS, 41/41.
- `apps/Nexus-Cloud: timeout 180s bun test src` — PASS, 88/88.
- `apps/Nexus-Cloud: timeout 180s bun run typecheck` — PASS.
- `apps/Nexus: timeout 240s cargo check -p nexus-server` — PASS.
- `apps/Nexus-Cloud/node_modules/.bin/tsc --noEmit --project deploy/production/tsconfig.json` — PASS. This direct local compiler invocation replaced `bunx`, which could not access its temporary directory in the sandbox.
- `bash -n` over changed production/test shell scripts — PASS.
- Extracted Bash blocks from both Task 5 runbook copies piped to `bash -n` — PASS.
- `git diff --check` — PASS.

Additional unavailable/non-owning checks:

- `shellcheck` is not installed in the workspace.
- `cargo fmt --all --check` in `apps/Nexus` reports extensive pre-existing formatting drift; that submodule is unchanged by this fix.
- A live Task 5 probe/restart/rollback was deliberately not run.

## Remaining uncertainty

- The stable per-environment locks protect repository-owned/cooperating writers. A manual editor that ignores the lock protocol can still race after the final comparison; the script preserves edits observed before commit and refuses mismatched credential values, but POSIX does not provide a transaction spanning three independent files.
- If the filesystem prevents both a commit rename and its compensating rename, automatic all-file atomicity is impossible. The phase fails loudly, retains protected recovery artifacts, keeps the storage checkpoint, and requires immediate operator recovery. This path is regression-tested without exercising a real filesystem failure.
- S3 cleanup behavior is verified with deterministic mocks; provider/network behavior remains for the operator-run Task 5 recovery probe.
