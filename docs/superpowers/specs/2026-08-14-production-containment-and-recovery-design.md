# Production Containment and Recovery Design

**Date:** 2026-08-14
**Status:** Approved direction; software implementation staged, live execution pending

## Objective

Restore production to a state that can truthfully be declared healthy by fixing the Nexus Chat reaction API, making deployer PID state match the live services, stopping credential and identity-token leakage, rotating the exposed Cloudflare tunnel and object-storage credentials, and verifying the complete public path after coordinated short restarts.

Restricted MinIO service accounts are intentionally deferred to a separate hardening task. This recovery keeps the current root-credential topology while replacing the exposed values.

## Current Evidence

- Nexus Chat is live, but reaction mutations fail because `reactions.message_id` and `reactions.user_id` are PostgreSQL `UUID` columns while the AnyPool repository binds UUIDs as text without PostgreSQL casts.
- Only Auth has a valid deployer PID file. Other healthy listeners were left unrecorded because `start_service` treats an occupied port as success and returns without adopting its process.
- Nexus Cloud logs the complete control-plane snapshot at startup. Storage pool state includes access and secret keys even though the public API has a separate redaction path.
- Caddy's access logger records request headers carrying the proxy-injected identity JWT.
- The live `cloudflared` container receives its remotely managed tunnel token in its command line, exposing it through process inspection.
- The root MinIO credentials and Cloud's S3 credentials are stored separately and must be changed as one coordinated operation.

No secret or token value may be printed, committed, copied into test output, or included in operational notes.

## Design

### 1. Stop Further Leakage Before Rotation

Apply and test containment changes before generating replacement credentials:

1. Replace Cloud's full startup snapshot log with a deliberately safe summary that excludes storage endpoints, access keys, secret keys, API keys, tokens, and other credential-bearing state. Add a regression test that injects sentinel secrets and proves they cannot appear in serialized startup output.
2. Configure both the production Nexus Chat Caddyfile and the app-owned equivalent to omit sensitive request headers from access logs. The log must retain useful request metadata such as host, method, URI, status, duration, and size, but never `Authorization`, `Cookie`, `X-Nexus-Identity`, or other authentication headers. Validate the effective Caddy configuration before restart.
3. Replace the `cloudflared ... --token <value>` launch with a protected credential/config file consumed by the container. The new token must not occur in the container command, host process arguments, Docker labels, or logs. The credential file remains outside Git with owner-only permissions.

Historical logs containing the old object-storage secret or identity JWTs will be removed after the affected processes are restarted and the old credentials are invalid. Rotation makes captured credential values unusable before log cleanup occurs.

Service starts append to their existing runtime logs. Containment restarts therefore
preserve the compromised history until both credential-rotation checkpoints are
complete; only the checkpoint-gated log-cleanup command may replace the two exact
affected logs.

Cloud starts through a protected-file wrapper: inherited API/storage credential
names are cleared, the protected Cloud environment is sourced after the launcher
chain, and no secret-bearing `KEY=value` is placed in process arguments. Startup
uses bounded listener reconciliation and terminates only the exact launched
process if it misses that checkpoint; a late exact listener can be safely adopted
for recovery.

### 2. Repair Reactions

Keep the repository's `sqlx::AnyPool` compatibility. Add a small database-dialect helper that produces UUID-compatible expressions for PostgreSQL while leaving SQLite behavior intact, then use it consistently across every reaction operation:

- add reaction;
- remove one reaction;
- moderation removal operations;
- counts and batch counts;
- per-user reaction lookup;
- existence check; and
- reactor listing.

The regression test must execute add, duplicate add, read/count, user lookup, remove, and post-remove read against PostgreSQL. Existing SQLite tests remain part of the gate so the portability fix cannot regress local development.

PostgreSQL predicates leave indexed UUID columns unchanged and cast bound values
(`message_id = $n::uuid`, `user_id = $n::uuid`); SQLite uses the same numbered
parameters without casts. Batch lists follow the same rule. Selected UUID values
may still be cast to text for `AnyPool` decoding. PostgreSQL coverage includes
batch counts, batch user lookups, and both moderation deletes.

Both bulk reaction DELETE routes first resolve the target message, its actual
channel, and its server. The URL channel must equal that actual channel before
authorization is evaluated against the actual target server/channel. A caller
who controls one channel cannot use its URL with a message ID from another
channel; cross-channel regressions cover both bulk routes and prove no target
reaction is removed.

### 3. Repair PID Bookkeeping

The deployer will reconcile PID files conservatively:

- A live PID file is accepted only when its process owns the service's expected listening port.
- If the expected port is occupied without a valid PID file, resolve the single listener PID and validate it against the service's expected executable/working directory before adopting it.
- Ambiguous, foreign, or unverifiable listeners are reported as conflicts and are never killed or adopted.
- Stale PID files are replaced only after listener validation.
- `status` reports process ownership and HTTP health independently and does not silently claim an unmanaged listener is deployer-managed.
- `stop` targets only validated service PIDs; no broad `pkill`, name matching, or unrelated-process termination is allowed.

Shell-level tests will cover valid adoption, stale files, foreign listeners, missing listeners, and refusal to stop a mismatched PID.

### 4. Coordinated Credential Rotation

The maintenance sequence is intentionally ordered:

1. Land and locally verify containment, reaction, and PID changes.
2. Prepare protected rollback state. If the current connector still carries its token in argv, inspect the exact container command into a mode-`0600` file, extract the token directly into the protected token file, hash it, and delete the argv snapshot without printing the token.
3. Restart only Nexus Cloud and Nexus Chat's Caddy front door through their validated per-service restart paths; verify sentinel credentials and identity headers no longer enter fresh logs. Starts append, so historical compromised logs remain present at this point.
4. The operator explicitly rotates the existing tunnel token in the Cloudflare dashboard. The script never mutates the remote tunnel credential. With `NEXUS_ROTATION_TUNNEL_VERIFY_URL` required, it fetches the dashboard-issued token with `GET`, proves its hash differs from the prepared token, adopts it from the protected file, recreates only `cloudflared`, verifies a replacement connector and the public URL, re-fetches the token, and proves the remote, staged, and installed hashes match before disconnecting old connections.
5. Generate new strong MinIO root credentials. Update the root infrastructure environment, Nexus Cloud S3 environment, and the deployed Nexus Chat environment as one conditional atomic credential set without displaying values. Rollback restores only those credential keys, requires each current value to be either the expected replacement or prepared old value, and preserves unrelated concurrent environment edits.
6. Recreate only MinIO and write the storage checkpoint after its health check. Then restart Nexus Cloud and deployed Nexus Chat and run bounded authenticated disposable S3 write/read/delete probes against both the operator probe bucket and Chat's configured bucket. If any restart, semantic health check, deadline, or S3 probe fails, invoke the explicit `rollback-storage` phase automatically; it remains usable after the MinIO checkpoint, restores the paired credential keys, recreates MinIO, removes the invalid storage checkpoint, and restarts both consumers against the restored pair. Do not delete storage data.
7. After both tunnel and storage checkpoints exist and old credentials are proven invalid, run the checkpoint-gated cleanup for the exact Cloud and Nexus Chat Caddy logs. It validates the replacement service, stops only its validated PID, installs an empty mode-`0600` log, and restarts only that service.
8. Reconcile all service PID files through the hardened deployer. Restart a service only when safe adoption is impossible.

The tunnel and storage rotations are separate checkpoints. Failure of the second does not roll back a successful tunnel rotation.

Cloud shared-pool records no longer persist local MinIO credentials. Existing
legacy fields are ignored and scrubbed from in-memory snapshots; shared-volume
provisioning resolves the current protected S3 credentials at operation time, so
environment rotation also covers that path.

### 5. Verification

Completion requires fresh evidence from all layers:

- Nexus database and API tests, including the PostgreSQL reaction regression.
- Nexus Cloud tests and typecheck, including the secret-log regression.
- Production deployer and proxy/Caddy configuration tests.
- `git diff --check` and all scoped app quality gates.
- Docker health for PostgreSQL, Redis, MinIO, Hosting, and cloudflared.
- Accurate PID ownership for Auth, Cloud, Team Chat, Nexus Chat, Nexus Chat Caddy, Dashboard, Draw, and Proxy.
- Local health endpoints for every managed service.
- Public health and expected authentication behavior for `tnhc.dev`, Cloud, Auth, Chat, Dashboard, Draw, and Hosting.
- An authenticated live reaction add/read/remove cycle on a disposable test message.
- An authenticated S3 write/read/delete probe in a disposable object key.
- Process, container, and fresh-log scans proving the replacement tunnel token, object-storage secret, and identity JWT sentinels are absent.
- Explicit confirmation that the previous tunnel and object-storage credentials no longer authenticate.

Production is not declared fully healthy until every required check passes. Any remaining degraded feature, inaccurate PID ownership, or active leakage is reported plainly instead.

## Rollback and Data Safety

- Do not remove Docker volumes, buckets, database rows, or persistent Nexus data.
- Keep previous credential values only in a protected temporary rollback file until their replacements pass verification; securely remove that file immediately afterward.
- Roll back code by restoring the previous service executable/config and restarting only the affected service.
- Roll back MinIO by restoring both MinIO and Cloud credential sources together.
- A tunnel rollback uses a newly issued token, not re-exposure of a revoked token in process arguments.
- Historical log deletion is irreversible but occurs only after credential invalidation; no application data is stored in those runtime logs.
- Protected rotation cleanup refuses to run unless both the tunnel and storage checkpoint files exist.

## Deferred Hardening

Create a separate follow-up design for least-privilege MinIO service accounts. It should give Nexus Cloud only the bucket and object operations it needs, keep root credentials out of application processes, define credential renewal, and test denial outside the assigned bucket prefix.
