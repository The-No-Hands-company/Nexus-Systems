# Nexus Mail P1 Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver private seven-day Trash recovery, permanent erase, mailbox-scoped filtered search, and explicit spam disposition.

**Architecture:** Keep lifecycle state and full-text search in PostgreSQL. `MailStore` owns transactional membership erasure and orphan cleanup; Mail API exposes narrowly authorized endpoints; Dashboard presents destructive actions with confirmation and accessible status.

**Tech Stack:** Rust, Axum, SQLx/PostgreSQL, Bun, React, TypeScript, Vitest.

**Spec:** `docs/superpowers/specs/2026-11-01-nexus-mail-p1-lifecycle-design.md`

## Global Constraints

- Trash is recoverable for exactly seven days; explicit erase is immediate.
- Never retain deleted content in an external index, content log, or analytics event.
- Preserve shared message bodies until the last mailbox membership is erased.
- External bodies use only store-issued reservations and conditionally-created,
  immutable object keys; caller-selected keys are prohibited.
- Mail API remains loopback-only and receives identity only from trusted proxies.
- Build resource-intensive Rust commands with `CARGO_BUILD_JOBS=1` and a 2-GiB/1.25-core systemd scope.

---

### Task 1: Persist Trash expiry and transactional erasure

**Files:**
- Create: `apps/Nexus-Email/crates/nexus-mailstore/migrations/20261101000006_mail_lifecycle.sql`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/store.rs:500-790`
- Test: `apps/Nexus-Email/crates/nexus-mailstore/tests/store.rs`

**Interfaces:**
- Produces `MailStore::move_to_folder_at`, `MailStore::erase_membership`, and `MailStore::purge_expired_trash`.
- `erase_membership(mailbox_id, message_id)` deletes only that membership and deletes message rows/object references only at zero remaining memberships.

- [ ] **Step 1: Write failing store tests**
```rust
#[tokio::test]
async fn trash_membership_expires_after_seven_days_and_last_copy_is_erased() {
    // create two mailbox memberships for one message; erase one, assert the
    // other remains; expire and purge the last Trash membership, then assert
    // no messages/mailbox_messages/search projection rows remain.
}
```
- [ ] **Step 2: Run the failing test**
Run: `cd apps/Nexus-Email && NEXUS_EMAIL_TEST_DATABASE_URL=... cargo test -p nexus-mailstore trash_membership_expires_after_seven_days_and_last_copy_is_erased -- --exact`
Expected: FAIL because lifecycle fields and methods do not exist.
- [ ] **Step 3: Add migration and store implementation**
```sql
ALTER TABLE mailbox_messages ADD COLUMN trash_expires_at TIMESTAMPTZ;
CREATE INDEX mailbox_messages_trash_expiry_idx
  ON mailbox_messages (trash_expires_at) WHERE trash_expires_at IS NOT NULL;
```
Set expiry only for Trash moves; clear it for Inbox/Junk. Implement one SQL transaction that deletes the membership, then removes an orphaned message and its search data only when no membership remains.
- [ ] **Step 4: Run focused store tests**
Run: `cd apps/Nexus-Email && ./check.sh`
Expected: PASS.
- [ ] **Step 5: Commit**
```bash
git add apps/Nexus-Email/crates/nexus-mailstore
git commit -m "feat: add mail lifecycle storage"
```

### Task 2: Expose permanent erase, empty Trash, and filtered search

**Files:**
- Modify: `apps/Nexus-Email/crates/nexus-mailapi/src/lib.rs:123-140,466-505`
- Modify: `apps/Nexus-Email/crates/nexus-mailapi/tests/privacy.rs`
- Modify: `apps/Nexus-API/src/server.ts`
- Modify: `apps/Nexus-API/tests/delegation-boundary.test.ts`

**Interfaces:**
- Produces `DELETE /api/v1/messages/:id`, `POST /api/v1/folders/trash/empty`, and `GET /api/v1/search?q=&folder=&unread=`.

- [ ] **Step 1: Write failing API and gateway tests**
```rust
// API: owner DELETE returns 204 twice; non-owner cannot remove another copy;
// empty Trash deletes only the authenticated mailbox's Trash memberships.
```
```ts
// Gateway: DELETE and POST empty-Trash routes forward only Auth-resolved identity.
```
- [ ] **Step 2: Run focused tests and verify failure**
Run: `cd apps/Nexus-Email && cargo test -p nexus-mailapi --test privacy`
Run: `cd apps/Nexus-API && bun test tests/delegation-boundary.test.ts`
Expected: FAIL for unimplemented routes.
- [ ] **Step 3: Implement minimal handlers**
Add Axum routes and handlers that call Task 1 store methods. Reject empty/oversize search text and invalid filter values with `400`; default search excludes Trash/Junk unless folder is explicitly selected. Extend API gateway allowlists without accepting browser identity headers.
- [ ] **Step 4: Run focused tests**
Run: `cd apps/Nexus-Email && ./check.sh && cd ../Nexus-API && bun test tests/delegation-boundary.test.ts && bun run check`
Expected: PASS.
- [ ] **Step 5: Commit**
```bash
git add apps/Nexus-Email/crates/nexus-mailapi apps/Nexus-API
git commit -m "feat: expose mail deletion lifecycle"
```

### Task 3: Add Dashboard lifecycle and search controls

**Files:**
- Modify: `apps/Nexus-Dashboard/src/server.ts`
- Modify: `apps/Nexus-Dashboard/tests/mail-proxy.test.ts`
- Modify: `apps/Nexus-Dashboard/frontend/src/api.ts:723-745`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/mail/MailList.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/mail/MailRead.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/mail/MailList.test.tsx`
- Modify: `apps/Nexus-Dashboard/frontend/src/pages/mail/MailRead.test.tsx`

**Interfaces:**
- Produces `deleteMailMessage(id)`, `emptyMailTrash()`, and `searchMail(query, filters)`.

- [ ] **Step 1: Write failing UI tests**
```tsx
it("confirms and empties Trash", async () => {
  // open Trash, confirm Empty Trash, assert POST /ipa/mail/folders/trash/empty
  // and an accessible success status.
});
```
```tsx
it("filters search without including Trash or Junk by default", async () => {
  // submit search and assert serialized query parameters.
});
```
- [ ] **Step 2: Run tests and verify failure**
Run: `cd apps/Nexus-Dashboard && bun test tests/mail-proxy.test.ts frontend/src/pages/mail/MailList.test.tsx frontend/src/pages/mail/MailRead.test.tsx`
Expected: FAIL because clients and controls are absent.
- [ ] **Step 3: Implement typed clients, proxy allowlist, and controls**
Add accessible confirmation UI, Restore/Delete permanently controls in Trash, Junk disposition labels/actions, search filters, status announcements, focus restoration, and responsive layout without changing unrelated shell behavior.
- [ ] **Step 4: Run Dashboard gate**
Run: `cd apps/Nexus-Dashboard && bun test && bun run check`
Expected: PASS.
- [ ] **Step 5: Commit**
```bash
git add apps/Nexus-Dashboard
git commit -m "feat: add mail lifecycle controls"
```

### Task 4: Add bounded purge operation and release evidence

**Files:**
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/main.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/tests/session.rs`
- Modify: `apps/Nexus-Email/README.md`
- Modify: `CHANGELOG.md`

- [ ] **Step 1: Write failing bounded-purge recovery test**
```rust
#[tokio::test]
async fn purge_resumes_after_restart_without_deleting_unexpired_trash() {
    // seed expired and unexpired memberships; run one bounded batch twice;
    // assert only expired memberships are erased and the second pass is safe.
}
```
- [ ] **Step 2: Run it and verify failure**
Run: `cd apps/Nexus-Email && cargo test -p nexus-mailsmtp purge_resumes_after_restart_without_deleting_unexpired_trash -- --exact`
Expected: FAIL because no purge worker exists.
- [ ] **Step 3: Implement bounded daemon purge loop**
Use a periodic task with a fixed batch size and aggregate-only tracing. Wire it to `MailStore::purge_expired_trash`; do not log message or identity data.
- [ ] **Step 4: Run release gates and staging journey**
Run: `cd apps/Nexus-Email && ./check.sh`
Run: `cd apps/Nexus-Dashboard && bun test && bun run check`
Run: `cd apps/Nexus-API && bun test && bun run check`
Verify staging: send, receive, read, Trash, restore, erase, automatic purge, search, Junk/Not Spam.
- [ ] **Step 5: Record approval and commit**
```bash
git add apps/Nexus-Email/README.md CHANGELOG.md
git commit -m "docs: record mail lifecycle release"
```

## Self-review

- **Spec coverage:** Tasks 1-2 implement private storage, erase, recovery, search, and spam route semantics; Task 3 implements user controls and accessibility; Task 4 covers scheduled purge, validation, staging, founder approval, and changelog.
- **Placeholder scan:** No TBD/TODO or unspecified tests remain.
- **Type consistency:** Task 1 store methods are consumed by Task 2; Task 2 HTTP routes are consumed by Task 3 typed Dashboard clients; Task 4 consumes Task 1’s purge method.

### Task 5: Fence external object generations

**Files:**
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/store.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/models.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/error.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/src/lib.rs`
- Create: `apps/Nexus-Email/crates/nexus-mailstore/migrations/20261103000009_external_body_reservations.sql`
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/object_cleanup.rs`
- Test: `apps/Nexus-Email/crates/nexus-mailstore/tests/store.rs`
- Test: `apps/Nexus-Email/crates/nexus-mailsmtp/tests/object_cleanup.rs`

**Interfaces:**
- Produces:
  ```rust
  pub struct ExternalBodyReservation {
      pub generation_id: Uuid,
      pub object_key: String,
      pub expires_at: DateTime<Utc>,
  }

  pub async fn reserve_external_body(
      &self,
      now: DateTime<Utc>,
  ) -> Result<ExternalBodyReservation>;

  pub async fn queue_expired_external_body_reservations(
      &self,
      now: DateTime<Utc>,
      limit: i64,
  ) -> Result<u64>;
  ```
- Changes `MailStore::store_message` so its final argument is
  `Option<&ExternalBodyReservation>`, never `Option<&str>`. A supplied
  reservation must be live, unfinalized, and matched to its generated object
  key. A missing reservation for a body larger than `INLINE_BODY_LIMIT` returns
  `MailStoreError::ExternalBodyReservationRequired`.
- `run_object_deletion_pass` first schedules a bounded batch of expired
  reservations, then claims and deletes only `cleanup_queued` generations.
- The future object-storage adapter conditionally creates
  `reservation.object_key` before passing the reservation to `store_message`;
  it must use `If-None-Match: *` or an equivalent no-overwrite operation.

- [ ] **Step 1: Write the failing caller-key rejection test**
```rust
#[tokio::test]
async fn externally_stored_body_rejects_a_caller_selected_key() {
    let s = store().await;
    let from = Address::parse("sender@example.test").unwrap();
    let thread = s.thread_for(None, &[], Some("reserved")).await.unwrap();
    let result = s
        .store_message(
            b"externally uploaded body",
            thread,
            &from,
            Some("reserved"),
            None,
            None,
            &[],
            Transport::Internal,
            Some("caller-selected-key"),
        )
        .await;
    assert!(result.is_err());
}
```
- [ ] **Step 2: Run the caller-key test to verify RED**
Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailstore externally_stored_body_rejects_a_caller_selected_key -- --exact`
Expected: FAIL because `store_message` currently accepts a caller-selected
object key.

- [ ] **Step 3: Add the additive reservation migration**
```sql
ALTER TABLE object_generations
    ADD COLUMN reservation_expires_at TIMESTAMPTZ,
    ADD COLUMN finalized_message_id UUID REFERENCES messages(id) ON DELETE SET NULL,
    DROP CONSTRAINT object_generations_lifecycle_state_check,
    ADD CONSTRAINT object_generations_lifecycle_state_check
        CHECK (lifecycle_state IN ('reserved', 'referenced', 'cleanup_queued', 'deleted'));

CREATE INDEX object_generations_expired_reservations_idx
    ON object_generations (reservation_expires_at)
    WHERE lifecycle_state = 'reserved';
```
Keep the existing object key unique index. Do not edit already-applied
`20261102000008_object_generation_registry.sql`.

- [ ] **Step 4: Implement reservation and transactional finalization**
```rust
pub async fn reserve_external_body(
    &self,
    now: DateTime<Utc>,
) -> Result<ExternalBodyReservation> {
    let generation_id = Uuid::now_v7();
    let object_key = format!("mail-bodies/{generation_id}");
    let expires_at = now + Duration::minutes(15);
    sqlx::query(
        "INSERT INTO object_generations
         (id, object_key, lifecycle_state, created_at, reservation_expires_at)
         VALUES ($1, $2, 'reserved', $3, $4)",
    )
    .bind(generation_id)
    .bind(&object_key)
    .bind(now)
    .bind(expires_at)
    .execute(&self.pool)
    .await?;
    Ok(ExternalBodyReservation { generation_id, object_key, expires_at })
}
```
Add `ExternalBodyReservation` to `models.rs` and re-export it from `lib.rs`.
Replace `store_message`'s raw object key parameter with the reservation type.
Inside its transaction, lock the generation row with `FOR UPDATE`; reject a
missing, expired, finalized, queued, or deleted generation; write both the
exact key and generation ID to `messages`; record the content hash and final
message ID; and transition the generation from `reserved` to `referenced`.
For an existing content hash, transition the supplied reservation to
`cleanup_queued` and insert its unique queue job in the same transaction so an
already-uploaded duplicate body is not retained. Add explicit error variants
for a missing, expired, and finalized reservation without including object
keys in their display text. Update every existing external-body test call site
to reserve first; inline callers continue passing `None`.

- [ ] **Step 5: Write and run the failing reservation-expiry test**
```rust
#[tokio::test]
async fn expired_uploaded_reservation_is_queued_without_affecting_live_body() {
    let s = store().await;
    let expired = s
        .reserve_external_body(Utc::now() - Duration::minutes(16))
        .await
        .unwrap();
    let live = s.reserve_external_body(Utc::now()).await.unwrap();

    assert_eq!(
        s.queue_expired_external_body_reservations(Utc::now(), 10)
            .await
            .unwrap(),
        1
    );
    let jobs = s.claim_object_deletion_jobs(Utc::now(), 10).await.unwrap();
    assert_eq!(jobs.iter().map(|job| &job.object_key).collect::<Vec<_>>(), vec![&expired.object_key]);
    assert_ne!(jobs[0].object_key, live.object_key);
}
```
Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailstore expired_uploaded_reservation_is_queued_without_affecting_live_body -- --exact`
Expected: FAIL because expiration does not schedule deletion.

- [ ] **Step 6: Implement bounded reservation-expiry recovery**
```rust
pub async fn queue_expired_external_body_reservations(
    &self,
    now: DateTime<Utc>,
    limit: i64,
) -> Result<u64> {
    let mut tx = self.pool.begin().await?;
    let rows: Vec<(Uuid, String)> = sqlx::query_as(
        "SELECT id, object_key FROM object_generations
         WHERE lifecycle_state = 'reserved' AND reservation_expires_at <= $1
         ORDER BY reservation_expires_at
         FOR UPDATE SKIP LOCKED LIMIT $2",
    )
    .bind(now)
    .bind(limit.clamp(1, 1_000))
    .fetch_all(&mut *tx)
    .await?;
    for (generation_id, object_key) in &rows {
        sqlx::query(
            "UPDATE object_generations
             SET lifecycle_state = 'cleanup_queued', reservation_expires_at = NULL
             WHERE id = $1 AND lifecycle_state = 'reserved'",
        )
        .bind(generation_id)
        .execute(&mut *tx)
        .await?;
        sqlx::query(
            "INSERT INTO object_deletion_queue
             (id, generation_id, object_key, next_attempt_at, created_at)
             VALUES ($1, $2, $3, $4, $4)
             ON CONFLICT DO NOTHING",
        )
        .bind(Uuid::now_v7())
        .bind(generation_id)
        .bind(object_key)
        .bind(now)
        .execute(&mut *tx)
        .await?;
    }
    tx.commit().await?;
    Ok(rows.len() as u64)
}
```
Use the same job insertion path as final message erasure. In
`run_object_deletion_pass`, call this method before claiming jobs, using the
same bounded limit. A missing object is treated as a successful deletion by
the object-store adapter; no content or key is written to logs.

- [ ] **Step 7: Write and run the real cleanup-pass regression**
```rust
#[tokio::test]
async fn deletion_pass_cleans_only_the_expired_reserved_generation() {
    let s = store().await;
    let expired = s
        .reserve_external_body(Utc::now() - Duration::minutes(16))
        .await
        .unwrap();
    let live = s.reserve_external_body(Utc::now()).await.unwrap();
    let objects = Arc::new(Mutex::new(HashSet::from([
        expired.object_key.clone(),
        live.object_key.clone(),
    ])));
    let deleted = Arc::new(Mutex::new(Vec::new()));
    let callback_objects = Arc::clone(&objects);
    let callback_deleted = Arc::clone(&deleted);

    run_object_deletion_pass(&s, Utc::now(), 10, move |key| {
        callback_deleted.lock().unwrap().push(key.clone());
        let existed = callback_objects.lock().unwrap().remove(&key);
        future::ready(if existed { Ok(()) } else { Err(()) })
    })
    .await
    .unwrap();

    assert_eq!(*deleted.lock().unwrap(), vec![expired.object_key]);
    assert!(objects.lock().unwrap().contains(&live.object_key));
}
```
Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailsmtp deletion_pass_cleans_only_the_expired_reserved_generation -- --exact`
Expected: FAIL before Step 6 because reservations are not scheduled, then PASS.

- [ ] **Step 8: Verify focused suites and the capped Mail gate**
Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailstore && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailsmtp && CARGO_BUILD_JOBS=1 bash ./check.sh`
Expected: PASS.

- [ ] **Step 9: Commit**
```bash
git add apps/Nexus-Email
git commit -m "fix: reserve mail object generations"
```
