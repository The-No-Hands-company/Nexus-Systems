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
- Modify: `apps/Nexus-Email/crates/nexus-mailstore/migrations/20261101000006_mail_lifecycle.sql`
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/object_cleanup.rs`
- Test: `apps/Nexus-Email/crates/nexus-mailstore/tests/store.rs`

- [ ] **Step 1: Write the failing key-reuse race test**
```rust
#[tokio::test]
async fn cleanup_never_deletes_a_new_object_generation_after_erasure() {
    // Queue an old generation, store a new message with equivalent content,
    // then assert the cleanup worker can delete only the old generation.
}
```
- [ ] **Step 2: Run the test to verify RED**
Run: `cd apps/Nexus-Email && cargo test -p nexus-mailstore cleanup_never_deletes_a_new_object_generation_after_erasure -- --exact`
Expected: FAIL because queued jobs name a reusable object key.
- [ ] **Step 3: Implement immutable generation registry**
Create durable generation/reference state. Store operations allocate a fresh
generation for each externally stored object, erasure queues that generation,
and cleanup claims/deletes only a generation that remains unreferenced.
- [ ] **Step 4: Verify GREEN**
Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 bash ./check.sh`
Expected: PASS.
- [ ] **Step 5: Commit**
```bash
git add apps/Nexus-Email
git commit -m "fix: fence mail object cleanup"
```
