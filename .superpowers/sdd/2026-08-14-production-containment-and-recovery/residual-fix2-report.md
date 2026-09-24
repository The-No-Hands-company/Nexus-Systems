# Residual Recovery Fix 2 Report

Date: 2026-08-16

No live production action was performed. All rotation behavior was exercised against isolated temporary fixtures; no production environment file, credential, service, container, API, or log was read or mutated.

## Restored invariants

### Coherent prepare snapshots under the shared writer protocol

- `prepare` now acquires the stable root, Cloud, and conditionally deployed Chat environment locks before reading or copying any credential file.
- The locks remain held while the live files are uniqueness/coherence-validated, copied into protected rollback files, and the prepared copies are validated again.
- Root `MINIO_ROOT_USER`/`MINIO_ROOT_PASSWORD`, Cloud `NEXUS_STORAGE_S3_ACCESS_KEY`/`NEXUS_STORAGE_S3_SECRET_KEY`, and deployed Chat `NEXUS__STORAGE__ACCESS_KEY`/`NEXUS__STORAGE__SECRET_KEY` must resolve to one access/secret pair before active state is published.
- A prepare process blocked at the first environment snapshot excludes a concurrent `with-storage-locks` writer. Incoherence in any Cloud or Chat half of the pair is rejected without publishing `rotation.current`.

### Bash-effective assignment handling

- Canonical assignments are recognized with optional indentation, optional `export`, and either `KEY=` or Bash's valid `KEY+=` form, rather than only a column-zero `KEY=` prefix.
- Effective values are obtained by sourcing the protected file in an isolated child Bash after unsetting the requested key, matching the production launchers' quoting, expansion, and last-assignment behavior without printing the value.
- Uniqueness checks count indented/exported canonical assignments, so alternate single-quoted, double-quoted, or unquoted duplicates fail closed.
- Rotation and rollback renderers use the same matcher, normalize the one accepted assignment, and cannot leave a recognized alternate assignment behind while post-validation reads a different line.

## Bounded TDD evidence

- Baseline: `timeout 120s bash deploy/production/tests/rotation.test.sh` passed 23/23.
- Parsing RED: valid indented/exported/quoted assignments were rejected by the exact-prefix parser.
- Parsing GREEN: the focused suite passed 25/25 after the shared Bash-effective matcher/reader/renderer change.
- Prepare RED: `prepare` accepted an incoherent cross-file pair, and the snapshot boundary did not exclude a cooperative writer.
- Prepare GREEN: the focused suite passed 27/27 after lock acquisition moved ahead of validation/snapshot and both live/prepared coherence checks were added.
- Review RED: coherent-looking cross-file `KEY+=` suffix assignments remained invisible to uniqueness checks and allowed `prepare` to succeed.
- Review GREEN: the shared matcher now counts and renders `KEY+=`; a lone append assignment is normalized, base-plus-append duplicates are rejected, and the focused suite passes 28/28.

## Files changed

- `deploy/production/rotate-production-secrets.sh`
- `deploy/production/tests/rotation.test.sh`
- `.superpowers/sdd/2026-08-14-production-containment-and-recovery/residual-fix2-report.md`

No application submodule source changed.

## Final verification

- `timeout 150s bash deploy/production/tests/rotation.test.sh` — PASS, 28/28.
- `timeout 150s bash deploy/production/tests/processes.test.sh` — PASS, 22/22.
- `timeout 60s bun test deploy/production/tests/storage-probe.test.ts` — PASS, 4/4.
- `timeout 240s bun test deploy/production/tests` — PASS, 41/41.
- `apps/Nexus-Cloud: timeout 240s bun test src` — PASS, 88/88.
- `apps/Nexus-Cloud: timeout 240s bun run typecheck` — PASS.
- `timeout 240s apps/Nexus-Cloud/node_modules/.bin/tsc --noEmit --project deploy/production/tsconfig.json` — PASS.
- `bash -n` over the production/test shell scripts — PASS.
- `git diff --check` — PASS.

`apps/Nexus-Cloud: timeout 240s bun run check` passed its TypeScript phase and then failed on 113 existing Biome lint/format diagnostics in the unchanged Cloud submodule. None are introduced or modified by this root-only fix.

## Remaining uncertainty

- Stable environment locks remain cooperative advisory locks. Repository-owned writers use `with-storage-locks`, but a manual or external writer that ignores the protocol can still race a multi-file snapshot; POSIX does not provide one transaction across three independent files.
- The parser intentionally counts canonical Bash assignment forms. Other Bash commands that can mutate a variable, such as `readonly`, `declare`, or arbitrary shell code, are outside the environment-file contract. Effective-value evaluation follows the trusted launchers and therefore assumes these protected files remain operator-controlled assignment files.
- No live recovery phase, service restart, storage probe, or credential operation was run. Runtime/provider behavior remains for the operator-controlled recovery procedure.
