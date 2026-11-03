# Mail Object Gateway Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make externally stored Mail bodies readable and deletable only through
recorded immutable object versions.

**Architecture:** `nexus-mailstore` becomes the authority for durable version
evidence and lifecycle eligibility. A Mail-owned gateway in `nexus-mailsmtp`
owns the S3 client and is injected into SMTP, IMAP/API, and outbound readers;
callers authorize a message before requesting its generation. Deployment uses
a maintenance marker and startup capability probe so old writers cannot mix
with the migration.

**Tech Stack:** Rust, SQLx/PostgreSQL, Tokio, aws-sdk-s3/MinIO, Bash.

**Spec:** `docs/superpowers/specs/2026-11-01-nexus-mail-lifecycle-safety-design.md`

## Global Constraints

- Never accept a caller-selected object key or expose static object-store credentials.
- Store and read the exact storage-returned version ID and matching generation tag.
- Cleanup is version-targeted, durable, and begins only 15 minutes after upload authority expiry.
- A legacy `body_object_key` without verified version evidence is never auto-deleted.
- External-body failures are transient; inline bodies continue to work.
- Lock message, then generation, then mailboxes in ascending UUID order.
- Run Rust commands with `CARGO_BUILD_JOBS=1` inside the 2-GiB/1.25-core scope.

---

### Task 1: Persist immutable external-body version evidence

**Files:**
- Create: `apps/Nexus-Email/crates/nexus-mailstore/migrations/20261107000015_external_body_versions.sql`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/models.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/store.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/error.rs`
- Test: `apps/Nexus-Email/crates/nexus-mailstore/tests/store.rs`

**Interfaces:**
- Produces `RecordedExternalBody { generation_id: Uuid, object_key: String, version_id: String }`.
- Produces `record_external_body_upload(reservation, version_id, generation_tag, now)` and
  `external_body_for_message(message_id)`.
- Changes deletion jobs to contain `version_id`; no job is claimable without it.

- [ ] **Step 1: Write failing store tests**

```rust
#[tokio::test]
async fn recorded_upload_requires_matching_generation_tag_and_version() {
    let reservation = store().await.reserve_external_body(Utc::now()).await.unwrap();
    assert!(store().await.record_external_body_upload(
        &reservation, "v1", "wrong-generation", Utc::now()
    ).await.is_err());
}

#[tokio::test]
async fn legacy_key_without_version_is_never_claimed_for_deletion() {
    let job = legacy_key_only_deletion_job().await;
    assert!(store().await.claim_object_deletion_jobs(Utc::now(), 1).await.unwrap()
        .iter().all(|claimed| claimed.id != job));
}
```

- [ ] **Step 2: Verify RED**

Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailstore recorded_upload_requires_matching_generation_tag_and_version -- --exact`

Expected: FAIL because version/tag evidence is not persisted.

- [ ] **Step 3: Add additive schema and minimal store implementation**

```sql
ALTER TABLE object_generations
  ADD COLUMN uploaded_version_id TEXT,
  ADD COLUMN uploaded_generation_tag TEXT,
  ADD COLUMN upload_recorded_at TIMESTAMPTZ,
  ADD COLUMN cleanup_eligible_at TIMESTAMPTZ;
ALTER TABLE object_deletion_queue ADD COLUMN version_id TEXT;
```

Require `uploaded_generation_tag = generation_id::text`, set
`cleanup_eligible_at = reservation_expires_at + interval '15 minutes'`, and
return an explicit store error for missing/mismatched evidence. Select only
jobs with a non-null version ID and elapsed eligibility time.

- [ ] **Step 4: Verify GREEN**

Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailstore --test store`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Email/crates/nexus-mailstore
git commit -m "feat: record mail object versions"
```

### Task 2: Add the Mail-owned object gateway and reader integration

**Files:**
- Create: `apps/Nexus-Email/crates/nexus-mailsmtp/src/object_gateway.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/main.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/object_cleanup.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailout/src/worker.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailapi/src/lib.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailimap/src/server.rs`
- Test: `apps/Nexus-Email/crates/nexus-mailsmtp/tests/object_gateway.rs`
- Test: `apps/Nexus-Email/crates/nexus-mailout/tests/worker_routes.rs`

**Interfaces:**
- Produces `ExternalBodyGateway: upload, read_version, delete_version, verify_capabilities`.
- Consumes `MailStore::external_body_for_message`.
- Every external read accepts a message/generation record, never a key or version from a client.

- [ ] **Step 1: Write failing gateway tests**

```rust
#[tokio::test]
async fn gateway_reads_only_the_recorded_version_for_a_message() {
    let gateway = fake_gateway_with_versions([("key", "v1", b"old"), ("key", "v2", b"new")]);
    let body = gateway.read_recorded(&record("generation", "key", "v1")).await.unwrap();
    assert_eq!(body, b"old");
}

#[tokio::test]
async fn outbound_external_body_read_failure_is_deferred_not_bounced() {
    let result = worker_with_unavailable_gateway().tick().await.unwrap();
    assert_eq!(result, 1);
    assert!(queue_is_deferred().await);
}
```

- [ ] **Step 2: Verify RED**

Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailsmtp gateway_reads_only_the_recorded_version_for_a_message -- --exact`

Expected: FAIL because no gateway exists.

- [ ] **Step 3: Implement the gateway and inject it**

```rust
#[async_trait::async_trait]
pub trait ExternalBodyGateway: Send + Sync {
    async fn read_version(&self, body: &RecordedExternalBody) -> Result<Vec<u8>, GatewayError>;
    async fn delete_version(&self, body: &RecordedExternalBody) -> Result<(), GatewayError>;
    async fn verify_capabilities(&self) -> Result<(), GatewayError>;
}
```

Use S3 `GetObject`/`DeleteObject` with `version_id`; upload with
`If-None-Match: *`, then persist its returned version/tag before finalization.
Inject the trait into API/IMAP/outbound constructors. An unavailable gateway
returns a transient external-body error; no endpoint accepts keys or versions.

- [ ] **Step 4: Verify GREEN**

Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailsmtp -p nexus-mailout -p nexus-mailapi -p nexus-mailimap`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Email/crates
git commit -m "feat: add mail object gateway"
```

### Task 3: Fence reconciliation and production rollout

**Files:**
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/object_cleanup.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/main.rs`
- Modify: `deploy/production/deploy.sh`
- Modify: `apps/Nexus-Email/README.md`
- Test: `apps/Nexus-Email/crates/nexus-mailsmtp/tests/object_cleanup.rs`
- Test: `deploy/production/tests/mail-rollout.test.sh`

**Interfaces:**
- Consumes version-bearing deletion jobs and `ExternalBodyGateway`.
- Produces deployment checks for maintenance mode, migration inventory, writer version, and capabilities.

- [ ] **Step 1: Write failing reconciliation and deployment tests**

```rust
#[tokio::test]
async fn cleanup_waits_for_quiescence_and_deletes_only_recorded_version() {
    let pass = run_object_deletion_pass(&store, expiry_plus_14_minutes(), 10, gateway).await.unwrap();
    assert_eq!(pass.deleted, 0);
    let pass = run_object_deletion_pass(&store, expiry_plus_15_minutes(), 10, gateway).await.unwrap();
    assert_eq!(pass.deleted, 1);
}
```

```bash
NEXUS_EMAIL_LEGACY_EXTERNAL_BODY_COUNT=1 bash deploy/production/tests/mail-rollout.test.sh
```

Expected: non-zero because an unresolved legacy inventory blocks startup.

- [ ] **Step 2: Verify RED**

Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailsmtp cleanup_waits_for_quiescence_and_deletes_only_recorded_version -- --exact`

Expected: FAIL because cleanup deletes key-only jobs immediately.

- [ ] **Step 3: Implement reconciliation and fencing**

Use the gateway to list/reconcile only a reservation key and matching
generation tag after quiescence. Delete exact recorded versions only; retain
retry state on every failure. Add deploy commands that stop Mail writers,
apply migrations, require a zero legacy-unmanaged inventory, dry-run
reconciliation/capability verification, start new writer binaries, and only
then resume ingress. Refuse normal startup when the marker is absent or stale.

- [ ] **Step 4: Verify GREEN and quality gate**

Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 bash ./check.sh`

Expected: PASS.

Run: `bash deploy/production/tests/mail-rollout.test.sh`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Email deploy/production
git commit -m "fix: fence mail object storage rollout"
```

## Self-review

- **Spec coverage:** Task 1 persists immutable evidence and blocks legacy
  deletion; Task 2 authorizes all readers through one gateway; Task 3 enforces
  quiescent reconciliation, exact deletion, capability checks, and controlled
  rollout.
- **Placeholder scan:** No unfinished markers or unspecified tests remain.
- **Type consistency:** `RecordedExternalBody` is produced by Task 1 and
  consumed by Task 2 and Task 3; no client-supplied key/version interface is introduced.
