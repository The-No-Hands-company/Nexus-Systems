# Nexus Mail Submission State Machine Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Atomically persist recipient delivery intent and safely reclaim interrupted delivery work.

**Architecture:** A submission and its recipient intents are inserted with the message in one transaction. Outbound queue rows are leased with fencing tokens; completion, failure, retention purge, and final erase lock the message first and derive lifetime only from non-terminal intents.

**Tech Stack:** Rust, SQLx/PostgreSQL, Tokio.

**Spec:** `docs/superpowers/specs/2026-11-01-nexus-mail-lifecycle-safety-design.md`

## Global Constraints

- Lock order is message, object generation when present, then mailboxes in ascending UUID order.
- A message with a non-terminal recipient intent is never deleted.
- Queue claims use a lease token and expiry; restart reclaims expired work.
- Logs contain no content, address, object key, version ID, or account identifier.
- Run Rust commands with `CARGO_BUILD_JOBS=1` inside the 2-GiB/1.25-core scope.

---

### Task 1: Persist submissions, recipient intents, and fenced leases

**Files:**
- Create: `apps/Nexus-Email/crates/nexus-mailstore/migrations/20261105000013_submission_intents.sql`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/models.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/store.rs`
- Modify: `apps/Nexus-Email/crates/nexus-maildelivery/src/deliver.rs`
- Modify: `apps/Nexus-Email/crates/nexus-maildelivery/src/queue.rs`
- Test: `apps/Nexus-Email/crates/nexus-mailstore/tests/store.rs`
- Test: `apps/Nexus-Email/crates/nexus-maildelivery/tests/queue.rs`

**Interfaces:**
```rust
pub enum RecipientIntentState {
    LocalPending,
    OutboundPending,
    Delivered,
    Failed,
}

pub struct ClaimedDelivery {
    pub queue_id: Uuid,
    pub message_id: Uuid,
    pub lease_token: Uuid,
}
```
`Deliverer::submit` resolves routes before beginning one transaction. It creates
the message, submission, every intent, memberships/queue rows, then commits.
`Queue::claim_due` claims pending or expired leased rows; terminal operations
require the supplied lease token and run checked orphan cleanup.

- [ ] **Step 1: Write failing state-machine tests**
```rust
#[tokio::test]
async fn submission_commits_all_recipient_intents_or_none() {
    let result = submit_with_one_local_and_one_external_recipient().await;
    assert!(result.is_ok());
    assert_eq!(recipient_intent_count(message_id()).await, 2);
}

#[tokio::test]
async fn expired_delivery_lease_is_reclaimed_after_restart() {
    let first = queue.claim_due(1).await.unwrap().pop().unwrap();
    expire_lease(first.queue_id).await;
    let second = queue.claim_due(1).await.unwrap().pop().unwrap();
    assert_ne!(first.lease_token, second.lease_token);
}

#[tokio::test]
async fn terminal_queue_retention_erases_queue_only_orphan() {
    let message = queue_only_message_with_delivered_intent().await;
    queue.purge_finished(Duration::zero(), Duration::zero()).await.unwrap();
    assert!(!message_exists(message).await);
}
```

- [ ] **Step 2: Run tests to verify RED**
Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-maildelivery submission_commits_all_recipient_intents_or_none expired_delivery_lease_is_reclaimed_after_restart terminal_queue_retention_erases_queue_only_orphan -- --exact`
Expected: FAIL because submissions, intents, and lease fencing do not exist.

- [ ] **Step 3: Add the additive migration**
```sql
CREATE TABLE mail_submissions (
  id UUID PRIMARY KEY,
  message_id UUID NOT NULL UNIQUE REFERENCES messages(id) ON DELETE RESTRICT,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE TABLE mail_submission_recipients (
  id UUID PRIMARY KEY,
  submission_id UUID NOT NULL REFERENCES mail_submissions(id) ON DELETE RESTRICT,
  route TEXT NOT NULL CHECK (route IN ('local', 'smtp', 'federated')),
  state TEXT NOT NULL CHECK (state IN ('local_pending', 'outbound_pending', 'delivered', 'failed')),
  queue_id UUID UNIQUE,
  created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
ALTER TABLE outbound_queue ADD COLUMN lease_token UUID, ADD COLUMN lease_expires_at TIMESTAMPTZ;
```
Replace the queue message foreign key cascade with `ON DELETE RESTRICT`.

- [ ] **Step 4: Implement transaction-aware submission and lease transitions**
Lock every target mailbox with `SELECT id FROM mailboxes WHERE id = ANY($1) ORDER BY id FOR UPDATE` before writing membership UIDs. Insert all intents with the message transaction. Claim queue rows only when `state = 'pending'` or `lease_expires_at <= now()`, assigning `Uuid::now_v7()` and `now() + interval '5 minutes'`. Require `WHERE id = $1 AND lease_token = $2` on completion/failure. Under the message lock, delete only if no membership and no `local_pending`/`outbound_pending` intent exists.

- [ ] **Step 5: Run focused green tests and Mail gate**
Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailstore && CARGO_BUILD_JOBS=1 cargo test -p nexus-maildelivery && CARGO_BUILD_JOBS=1 bash ./check.sh`
Expected: PASS.

- [ ] **Step 6: Commit**
```bash
git add apps/Nexus-Email/crates/nexus-mailstore apps/Nexus-Email/crates/nexus-maildelivery
git commit -m "fix: persist mail submission intents"
```

## Self-review

- **Spec coverage:** The task supplies atomic recipient intent creation, deterministic mailbox locks, fenced delivery recovery, and queue-retention orphan cleanup.
- **Placeholder scan:** No unresolved requirement remains in this plan.
- **Type consistency:** `ClaimedDelivery.lease_token` is produced by `claim_due` and consumed by terminal queue transitions.
