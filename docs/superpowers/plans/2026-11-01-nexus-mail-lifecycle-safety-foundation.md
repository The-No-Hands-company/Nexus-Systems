# Nexus Mail Lifecycle Safety Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Mail deletion, delivery, and external-body cleanup safe under concurrency, crashes, and controlled rollout.

**Architecture:** Serialize all reference changes through the message row and an authoritative durable ledger: submission holds atomically bridge message creation to mailbox/outbound references. Add a version-aware S3/MinIO adapter that owns uploads, persists immutable object-version identities, reconciles unfinished reservations after quiescence, and deletes versions rather than mutable keys.

**Tech Stack:** Rust, SQLx/PostgreSQL, Axum, AWS SDK for Rust/S3-compatible MinIO, Bun/React.

**Spec:** `docs/superpowers/specs/2026-11-01-nexus-mail-lifecycle-safety-design.md`

## Global Constraints

- A message is deleted only after every active durable ledger reference is released and mailbox/queue tables cross-check empty.
- Reference creation, reference removal, and orphan cleanup lock the message row with `SELECT ... FOR UPDATE`.
- External keys are store-issued and never reused; cleanup deletes immutable object versions only.
- External-body delivery fails transiently unless bucket versioning, conditional create, version IDs, and version-targeted deletion are verified.
- Cleanup is eligible no sooner than 15 minutes after upload authority expiry.
- Logs, metrics, and analytics must not contain content, addresses, object keys, version IDs, or account identifiers.
- Migration runs in a controlled maintenance window; do not mix old and new Mail writers.
- Run Rust commands with `CARGO_BUILD_JOBS=1` inside the 2-GiB/1.25-core scope.

---

### Task 1: Serialize message references and final erasure

**Files:**
- Create: `apps/Nexus-Email/crates/nexus-mailstore/migrations/20261104000010_message_reference_safety.sql`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/store.rs`
- Modify: `apps/Nexus-Email/crates/nexus-maildelivery/src/queue.rs`
- Modify: `apps/Nexus-Email/crates/nexus-maildelivery/src/deliver.rs`
- Test: `apps/Nexus-Email/crates/nexus-mailstore/tests/store.rs`
- Test: `apps/Nexus-Email/crates/nexus-maildelivery/tests/queue.rs`

**Interfaces:**
- Produces `MessageReferenceKind::{Submission, Mailbox, Outbound}` and `MailStore::begin_reference_transaction() -> Result<Transaction<'_, Postgres>>`.
- Produces `store_message_in_transaction`, `deliver_in_transaction`, and `Queue::enqueue_in_transaction`; each creates or releases a matching ledger row through the same transaction.
- `Deliverer::submit` parses and routes first, then atomically persists the message plus a submission hold, replaces it with mailbox/outbound holds, and releases it before returning recipient outcomes.
- `MailStore::erase_membership` and `erase_trash_membership` release the matching mailbox hold and orphan-check no active ledger, membership, or active queue reference.

- [ ] **Step 1: Write failing reference-safety tests**
```rust
#[tokio::test]
async fn erasing_final_mailbox_copy_keeps_pending_outbound_delivery() {
    let message = stored_message_with_membership_and_pending_queue().await;
    store.erase_membership(mailbox, message).await.unwrap();
    assert!(message_exists(message).await);
    assert_eq!(active_queue_count(message).await, 1);
}

#[tokio::test]
async fn concurrent_final_membership_erasure_deletes_shared_message_once() {
    let message = stored_message_in_two_mailboxes().await;
    tokio::join!(
        store.erase_membership(first_mailbox, message),
        store.erase_membership(second_mailbox, message),
    );
    assert_eq!(message_count(message).await, 0);
    assert_eq!(cleanup_job_count(message).await, 1);
}

#[tokio::test]
async fn unfinished_submission_hold_prevents_orphan_cleanup_after_restart() {
    let message = store_message_with_submission_hold_only().await;
    assert!(store.erase_membership(mailbox, message).await.is_ok());
    assert!(message_exists(message).await);
    assert_eq!(active_reference_kinds(message).await, vec!["submission"]);
}
```

- [ ] **Step 2: Run the tests to verify RED**
Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailstore erasing_final_mailbox_copy_keeps_pending_outbound_delivery concurrent_final_membership_erasure_deletes_shared_message_once unfinished_submission_hold_prevents_orphan_cleanup_after_restart -- --exact`
Expected: FAIL: no durable submission/mailbox/outbound reference ledger exists.

- [ ] **Step 3: Add the non-cascading queue migration**
```sql
ALTER TABLE outbound_queue DROP CONSTRAINT outbound_queue_message_id_fkey;
ALTER TABLE outbound_queue
  ADD CONSTRAINT outbound_queue_message_id_fkey
  FOREIGN KEY (message_id) REFERENCES messages(id) ON DELETE RESTRICT;
CREATE INDEX outbound_queue_active_message_idx
  ON outbound_queue (message_id) WHERE state IN ('pending', 'delivering');
CREATE TABLE message_references (
  id UUID PRIMARY KEY,
  message_id UUID NOT NULL REFERENCES messages(id) ON DELETE RESTRICT,
  kind TEXT NOT NULL CHECK (kind IN ('submission', 'mailbox', 'outbound')),
  protected_id UUID NOT NULL,
  released_at TIMESTAMPTZ,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
  UNIQUE (kind, protected_id)
);
CREATE INDEX message_references_active_message_idx
  ON message_references (message_id) WHERE released_at IS NULL;
```

- [ ] **Step 4: Implement locked reference transitions**
```rust
sqlx::query("SELECT id FROM messages WHERE id = $1 FOR UPDATE")
    .bind(message_id)
    .execute(&mut *tx)
    .await?;
```
`Deliverer` must call `begin_reference_transaction`, then use only the
transaction-aware store and queue methods until routing is durable. Create a
submission hold in the same transaction as `messages`; replace it with a
mailbox/outbound hold in the transaction that inserts the protected row; release
it only after all recipient outcomes are durable. Queue terminal transitions
begin their own reference transaction and release their outbound hold with the
state change. Change orphan deletion to require:
```sql
NOT EXISTS (
  SELECT 1 FROM message_references
  WHERE message_id = $1 AND released_at IS NULL
)
AND NOT EXISTS (SELECT 1 FROM mailbox_messages WHERE message_id = $1)
AND NOT EXISTS (
  SELECT 1 FROM outbound_queue
  WHERE message_id = $1 AND state IN ('pending', 'delivering')
)
```
Return a database error rather than deleting an active queue reference.

- [ ] **Step 5: Run focused green tests**
Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailstore --test store && CARGO_BUILD_JOBS=1 cargo test -p nexus-maildelivery --test queue`
Expected: PASS.

- [ ] **Step 6: Commit**
```bash
git add apps/Nexus-Email/crates/nexus-mailstore apps/Nexus-Email/crates/nexus-maildelivery
git commit -m "fix: serialize mail message references"
```

### Task 2: Make Trash erase authoritative and expose quarantine provenance

**Files:**
- Create: `apps/Nexus-Email/crates/nexus-mailstore/migrations/20261104000011_mail_disposition.sql`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/models.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/store.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailapi/src/lib.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailapi/tests/privacy.rs`
- Modify: `apps/Nexus-Dashboard/frontend/src/api.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/mail/MailRead.tsx`
- Test: `apps/Nexus-Dashboard/frontend/src/pages/mail/MailRead.test.tsx`

**Interfaces:**
- Produces `MailStore::erase_all_trash_memberships(mailbox_id, limit) -> Result<u64>`, which selects by folder kind without `mm.deleted` filtering.
- Produces `MessageDisposition::{None, DmarcQuarantine}` in Mail API message DTOs.

- [ ] **Step 1: Write failing API/store and Dashboard tests**
```rust
#[tokio::test]
async fn empty_trash_erases_imap_deleted_memberships() {
    mark_trash_membership_imap_deleted(mailbox, message).await;
    assert_eq!(store.erase_all_trash_memberships(mailbox, 200).await.unwrap(), 1);
    assert!(!membership_exists(mailbox, message).await);
}
```
```tsx
it("identifies DMARC quarantine as sender policy", async () => {
  render(<MailRead message={{ ...message, disposition: "dmarc_quarantine" }} />);
  expect(screen.getByText("Quarantined by sender policy")).toBeVisible();
});
```

- [ ] **Step 2: Run tests to verify RED**
Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailapi --test privacy empty_trash_erases_imap_deleted_memberships -- --exact && cd ../Nexus-Dashboard && bun test frontend/src/pages/mail/MailRead.test.tsx`
Expected: FAIL: Empty Trash depends on the display list and the DTO has no disposition.

- [ ] **Step 3: Add disposition and authoritative bulk erase**
```sql
ALTER TABLE mailbox_messages
  ADD COLUMN disposition TEXT NOT NULL DEFAULT 'none'
  CHECK (disposition IN ('none', 'dmarc_quarantine'));
```
Select Trash rows through `folders.kind = 'trash'` in bounded batches, then call the Task 1 locked erase path. Set `dmarc_quarantine` only when inbound DMARC policy places mail in Junk. Serialize the enum through API and render the exact policy notice only for that enum.

- [ ] **Step 4: Run focused green tests**
Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailapi --test privacy && cd ../Nexus-Dashboard && bun test frontend/src/pages/mail/MailRead.test.tsx`
Expected: PASS.

- [ ] **Step 5: Commit**
```bash
git add apps/Nexus-Email apps/Nexus-Dashboard
git commit -m "fix: preserve mail trash and quarantine disposition"
```

### Task 3: Implement version-aware object upload, reconciliation, and rollout gates

**Files:**
- Create: `apps/Nexus-Email/crates/nexus-mailstore/migrations/20261104000012_object_version_fencing.sql`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/models.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/store.rs`
- Create: `apps/Nexus-Email/crates/nexus-mailsmtp/src/object_store.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/main.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/object_cleanup.rs`
- Modify: `apps/Nexus-Email/crates/nexus-maildelivery/src/deliver.rs`
- Modify: `deploy/production/deploy.sh`
- Modify: `apps/Nexus-Email/README.md`
- Test: `apps/Nexus-Email/crates/nexus-mailstore/tests/store.rs`
- Test: `apps/Nexus-Email/crates/nexus-mailsmtp/tests/object_cleanup.rs`

**Interfaces:**
- Produces `ObjectStore::verify_capabilities()`, `upload_reserved(reservation, bytes) -> Result<ObjectVersion>`, `list_generation_versions(reservation)`, and `delete_version(version)`.
- `ObjectVersion { version_id: String, etag: String }` is persisted against a generation.
- Produces `MailStore::reconcile_expired_external_generations(now, limit) -> Result<u64>`.

- [ ] **Step 1: Write failing version-fencing tests**
```rust
#[tokio::test]
async fn cleanup_waits_for_quiescence_and_deletes_exact_uploaded_version() {
    let reservation = reserve_uploaded_generation(now).await;
    assert_eq!(store.reconcile_expired_external_generations(now, 10).await.unwrap(), 0);
    let jobs = reconcile_after(now + Duration::minutes(30)).await;
    assert_eq!(jobs[0].version_id, reservation.version_id);
}
```
```rust
#[tokio::test]
async fn unsupported_bucket_rejects_external_body_but_keeps_inline_delivery_available() {
    assert!(adapter_without_versioning.upload_reserved(reservation, bytes).await.is_err());
    assert!(deliver_inline_message().await.is_ok());
}
```

- [ ] **Step 2: Run tests to verify RED**
Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailsmtp cleanup_waits_for_quiescence_and_deletes_exact_uploaded_version unsupported_bucket_rejects_external_body_but_keeps_inline_delivery_available -- --exact`
Expected: FAIL: no object adapter or version identity exists.

- [ ] **Step 3: Add version-fencing migration**
```sql
CREATE TABLE object_generation_versions (
  generation_id UUID NOT NULL REFERENCES object_generations(id) ON DELETE RESTRICT,
  version_id TEXT NOT NULL,
  etag TEXT NOT NULL,
  PRIMARY KEY (generation_id, version_id)
);
ALTER TABLE object_generations
  ADD COLUMN upload_authority_expires_at TIMESTAMPTZ,
  ADD COLUMN cleanup_eligible_at TIMESTAMPTZ,
  DROP CONSTRAINT object_generations_lifecycle_state_check,
  ADD CONSTRAINT object_generations_lifecycle_state_check
    CHECK (lifecycle_state IN (
      'reserved', 'uploading', 'uploaded', 'referenced',
      'cleanup_queued', 'deleted', 'legacy_unmanaged'
    ));
ALTER TABLE object_deletion_queue ADD COLUMN version_id TEXT;
DROP INDEX object_deletion_queue_generation_idx;
CREATE UNIQUE INDEX object_deletion_queue_generation_version_idx
  ON object_deletion_queue (generation_id, version_id)
  WHERE generation_id IS NOT NULL AND version_id IS NOT NULL;
```
Backfill `body_object_key` rows into a `legacy_unmanaged` state. Abort the migration command if any cannot be resolved to a verified version ID; do not enqueue legacy deletion jobs.

- [ ] **Step 4: Implement the S3-compatible adapter and reservation state machine**
Use store-issued keys and `put_object().if_none_match("*")`; require a returned version ID, tag each version with generation ID, and persist it before finalization. Use reservation-scoped short-lived authority, reject new uploads after expiry, and set `cleanup_eligible_at = upload_authority_expires_at + interval '15 minutes'`. At startup, call bucket versioning and conditional-create probes; expose only aggregate health and return a transient failure for oversized mail when unavailable.

- [ ] **Step 5: Implement reconciliation and version-targeted deletion**
List versions only under the reservation's unique key, accept only matching generation tags, persist missing version IDs, and queue deletion after `cleanup_eligible_at`. Change cleanup jobs to carry `generation_id` plus `version_id`; call S3 `delete_object` with both key and version ID. Mark a generation deleted only after no version rows remain.

- [ ] **Step 6: Add rollout documentation and controlled-writer gate**
Update `deploy/production/deploy.sh` and `apps/Nexus-Email/README.md` with the required stop-migrate-inventory-verify-start order. Refuse `bg` Mail startup if the new migration marker or object capability check is absent; do not log secret values or object identifiers.

- [ ] **Step 7: Run focused and full validation**
Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailstore && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailsmtp && CARGO_BUILD_JOBS=1 cargo test -p nexus-maildelivery && CARGO_BUILD_JOBS=1 bash ./check.sh`
Expected: PASS.

- [ ] **Step 8: Commit**
```bash
git add apps/Nexus-Email deploy/production/deploy.sh
git commit -m "feat: fence mail object storage versions"
```

## Self-review

- **Spec coverage:** Task 1 covers active delivery references and concurrent erasure; Task 2 covers authoritative Trash and quarantine provenance; Task 3 covers versioned storage, quiescence, legacy inventory, controlled rollout, and capability failure.
- **Placeholder scan:** No unresolved requirement is intentionally deferred within this foundation.
- **Type consistency:** Task 1's lock is used by Task 2 erase; Task 3 retains Task 1's orphan predicate and supplies version-bound cleanup instead of key-bound cleanup.
