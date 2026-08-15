# Final Recovery Fix Report

Date: 2026-08-15

Status: `DONE_WITH_CONCERNS` — all requested recovery-correctness fixes and scoped gates are green. No live rotation, environment edit, service restart, log deletion, or production mutation was performed.

## Commits

- Nexus: `2409d2ca8e165e55be69cfc79282f956a150a289` (`fix(security): bind reaction moderation to targets`)
- Nexus-Cloud: `faa4e45b26debc3295a4a2c2e133c579650b27f7` (`fix(security): keep storage credentials rotation-safe`)
- Root implementation and gitlinks: `f489efbd6bb6b0c5d3df49205b813eea8693629e` (`fix(security): close production recovery review gaps`)

## Findings Mapping

1. Bulk reaction deletes now resolve the target message, require its real channel to match the URL, and authorize the real server/channel or DM participants before deletion. Both DELETE routes have cross-channel negative tests.
2. Storage rotation retains an explicit post-checkpoint `rollback-storage` phase. Task 5 invokes it automatically on Cloud restart, semantic health, authenticated S3, Chat restart, or Chat storage-probe failure, then restarts every deployed consumer against restored credentials.
3. Tunnel recovery now bootstraps a missing current token from a protected exact-container argv snapshot without printing it, requires an operator dashboard rotation and `NEXUS_ROTATION_TUNNEL_VERIFY_URL`, adopts the dashboard token via GET, and verifies changed/adopted/remote/installed hashes. Canonical documents no longer instruct PATCH rotation. Cloudflare calls are deadline-bounded.
4. Reaction SQL is dialect-aware: PostgreSQL casts bound parameters to UUID while leaving indexed UUID columns unchanged; SQLite keeps directly comparable text parameters; selected UUID output may cast to text. PostgreSQL coverage exercises lifecycle, batch counts, batch user lookups, and moderation deletes.
5. Cloud, Nexus Chat, and nexus-chat-web/Caddy have validated per-service restart paths. Starts append logs. Checkpoint-gated cleanup replaces only the exact compromised Cloud/Caddy log with a protected empty file after both rotations, then restarts the validated service.
6. Cleanup requires both tunnel and storage checkpoints; PID ownership rejects mismatches and can safely reconcile a delayed exact listener; HTTP health uses `curl --fail` plus an exact semantic healthy result.
7. Cloud launch now clears stale inherited canonical and alias storage variables, loads one protected environment source, and never passes Cloud API, Cloudflare, or storage secret values as launcher argv assignments.
8. The authenticated S3 probe has internal abort and outer Task 5 deadlines and attempts DELETE even when a PUT response is ambiguous.
9. Deployed Nexus Chat storage credentials participate in the atomic rotation/rollback set and the runbook restarts, health-checks, and probes Chat's configured bucket.
10. Cloud shared-storage pools no longer persist or reuse raw legacy access/secret values; operations resolve current protected S3 credentials. State restoration includes sanitized pool records.
11. Service startup uses a bounded reconciliation loop and safely terminates an exact launched process that never becomes managed; stop can adopt a delayed exact listener. Regression tests cover both paths.
12. Storage rollback restores only prepared credential keys, preserves unrelated file edits, and refuses to overwrite a concurrent credential-key edit.

## Verification Evidence

- `timeout 180s cargo test -p nexus-db -p nexus-api --lib`: PASS — Nexus API 61/61, Nexus DB 26/26, including both cross-channel route negatives and SQLite batch/moderation execution.
- `timeout 180s cargo check -p nexus-db -p nexus-api`: PASS.
- PostgreSQL scratch gate with `NEXUS_TEST_DATABASE_URL` and `cargo test -p nexus-db --test reactions_postgres -- --ignored --nocapture`: PASS — 1/1 in 1m56s; batch counts, batch user lookups, and moderation delete lifecycle executed against PostgreSQL.
- `timeout 120s bun test src` in Nexus-Cloud: final PASS — 88/88.
- `timeout 120s bun run typecheck` in Nexus-Cloud: PASS.
- Targeted Cloud storage/API tests: PASS — 19/19.
- `timeout 120s bun test deploy/production/tests`: PASS — 41/41.
- `timeout 30s bun test deploy/production/tests/storage-probe.test.ts`: PASS — 4/4, including a hanging request deadline and cleanup attempt.
- `timeout 120s bash deploy/production/tests/processes.test.sh`: PASS — 20/20.
- `timeout 60s bash deploy/production/tests/rotation.test.sh`: PASS — 17/17.
- `bash -n` on deploy, process, rotation, Cloud wrapper, and Bash test scripts: PASS.
- Both Task 5 executable storage-recovery blocks extracted and checked with `bash -n`: PASS.
- Deploy TypeScript `bunx tsc --noEmit`: PASS.
- `caddy validate --config deploy/production/nexus-chat.Caddyfile`: PASS (`Valid configuration`).
- Docker Compose merged configuration with non-secret validation placeholders: PASS.
- Root, Nexus, and Nexus-Cloud `git diff --check`: PASS before commit; staged diff checks also passed.
- Secret-scanning commit hook: PASS after marking one literal test sentinel as an explicit allowlist false positive; no credential material was committed.

## Non-Code Failures and Concerns

- One earlier cold Rust build reached its external 180-second timeout before test execution. It emitted no compiler/test failure; subsequent warm full library, check, and PostgreSQL runs passed.
- One Nexus-Cloud full run transiently returned 200 instead of 304 in the pre-existing conditional-ETag test (87/88). The isolated test immediately passed 1/1 and the subsequent full suite passed 88/88. This appears order/timing-sensitive and is outside the storage-recovery change, but remains worth tracking.
- Compose validation without required environment values failed closed as designed; the same configuration passed with non-secret validation-only placeholders.
- Caddy reported the pre-existing formatting warning at line 20 while validating the configuration successfully.
- Endpoint authorization is covered on migrated SQLite, while PostgreSQL integration coverage is at the repository batch/moderation layer requested by this fix wave.
- Task 5 still requires the operator's Cloudflare dashboard action, a real public verification URL, and live production evidence. Those actions were intentionally not performed in this code-only fix wave.
