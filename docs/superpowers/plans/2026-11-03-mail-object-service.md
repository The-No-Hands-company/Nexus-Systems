# Mail Object Service Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deliver external Mail bodies through an authenticated loopback service
that resolves only recorded immutable versions and refuses unsafe rollout.

**Architecture:** `nexus-mailsmtpd` owns the S3 client and a loopback-only
object service. Mail API, IMAP, and outbound delivery authenticate to that
service with distinct per-service secrets and request a message body by
message ID only. Startup and deployment reject external-body operation without
the migration/inventory marker and verified object-store capabilities.

**Tech Stack:** Rust, Axum/HTTP, Tokio, SQLx/PostgreSQL, aws-sdk-s3/MinIO,
Bash.

**Spec:** `docs/superpowers/specs/2026-11-01-nexus-mail-lifecycle-safety-design.md`

## Global Constraints

- The service binds loopback only and rejects non-loopback or unauthenticated callers.
- Requests contain only message ID and authenticated service identity; clients never send or receive keys, generation IDs, or version IDs.
- The service resolves the `RecordedExternalBody` for the message, then reads the exact recorded version.
- Do not log or return body bytes, object keys, version IDs, generation IDs, or credentials.
- Object-store failures map to HTTP `503`, IMAP `NO [UNAVAILABLE]`, and outbound deferral; inline bodies remain available.
- Do not enable external-body upload or reads without a completed migration/inventory marker and verified versioning, conditional-create, and version-delete capability.
- Use `CARGO_BUILD_JOBS=1` inside a 2-GiB/1.25-core systemd scope.

---

### Task 1: Implement the authenticated loopback object service

**Files:**
- Create: `apps/Nexus-Email/crates/nexus-mailsmtp/src/object_service.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/object_gateway.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/main.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/lib.rs`
- Test: `apps/Nexus-Email/crates/nexus-mailsmtp/tests/object_service.rs`

**Interfaces:**
- Consumes `MailStore::external_body_for_message(message_id) -> Result<RecordedExternalBody>`.
- Produces `ObjectServiceConfig { bind: SocketAddr, api_token: SecretString, imap_token: SecretString, outbound_token: SecretString }`.
- Produces `router(store, gateway, config) -> axum::Router`.

- [ ] **Step 1: Write failing service tests**

```rust
#[tokio::test]
async fn authenticated_service_streams_only_the_message_recorded_version() {
    let response = request(service(), "GET", "/internal/object/messages/MSG",
        [("authorization", "Bearer api-test-token")]).await;
    assert_eq!(response.status(), StatusCode::OK);
    assert_eq!(response.body_bytes().await, b"recorded-v1");
}

#[tokio::test]
async fn service_rejects_missing_or_invalid_service_credentials_without_metadata() {
    let response = request(service(), "GET", "/internal/object/messages/MSG", []).await;
    assert_eq!(response.status(), StatusCode::UNAUTHORIZED);
    assert!(!response.text().await.contains("object"));
}
```

- [ ] **Step 2: Verify RED**

Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailsmtp --test object_service authenticated_service_streams_only_the_message_recorded_version -- --exact`

Expected: FAIL because the service module and route do not exist.

- [ ] **Step 3: Implement the loopback route**

```rust
async fn body(
    State(service): State<ObjectService>,
    headers: HeaderMap,
    Path(message_id): Path<Uuid>,
) -> Result<Response, ObjectServiceError>
```

Authenticate the token to an internal service identity, load only the
message’s `RecordedExternalBody`, and call `gateway.read_version`. Return
bytes with `application/octet-stream`; map a missing body to `404` and
gateway unavailability to `503` without evidence in the response. In `main`,
require a loopback IP bind and spawn the listener with the SMTP daemon.

- [ ] **Step 4: Verify GREEN**

Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailsmtp --test object_service`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Email/crates/nexus-mailsmtp
git commit -m "feat: add mail object service"
```

### Task 2: Wire authenticated API, IMAP, and outbound body readers

**Files:**
- Create: `apps/Nexus-Email/crates/nexus-mailstore/src/object_client.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailapi/src/lib.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailimap/src/server.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailout/src/worker.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/main.rs`
- Test: `apps/Nexus-Email/crates/nexus-mailapi/tests/privacy.rs`
- Test: `apps/Nexus-Email/crates/nexus-mailimap/tests/external_body.rs`
- Test: `apps/Nexus-Email/crates/nexus-mailout/tests/worker_routes.rs`

**Interfaces:**
- Consumes `ObjectBodyClient::read_message(message_id) -> Result<Vec<u8>, ObjectBodyClientError>`.
- Uses `NEXUS_EMAIL_OBJECT_SERVICE_URL` and a process-specific secret only in process configuration.
- Produces no public HTTP route or client-visible object metadata.

- [ ] **Step 1: Write failing reader tests**

```rust
#[tokio::test]
async fn authorized_api_read_uses_object_service_without_exposing_evidence() {
    let response = api_with_object_service().read_message(AUTHORIZED, EXTERNAL_MESSAGE).await;
    assert_eq!(response.status(), StatusCode::OK);
    assert_eq!(response.body_bytes(), b"recorded-v1");
}

#[tokio::test]
async fn external_body_service_failure_defers_outbound_without_attempt_exhaustion() {
    worker_with_failing_object_service().process(claimed_delivery()).await;
    assert!(delivery_is_pending_without_incrementing_remote_budget().await);
}
```

- [ ] **Step 2: Verify RED**

Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailapi --test privacy authorized_api_read_uses_object_service_without_exposing_evidence -- --exact`

Expected: FAIL because API has no authenticated object-service client.

- [ ] **Step 3: Implement client injection**

```rust
#[async_trait::async_trait]
pub trait ObjectBodyClient: Send + Sync {
    async fn read_message(&self, message_id: Uuid) -> Result<Vec<u8>, ObjectBodyClientError>;
}
```

Construct one client per process from its URL and secret. API continues to
authorize mailbox membership before calling the client; IMAP continues to
authorize the selected mailbox; outbound calls it only for a claimed queue
message. Map unavailable errors to the required service-specific transient
outcome. Do not add a fallback that reconstructs object storage identifiers.

- [ ] **Step 4: Verify GREEN**

Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailapi -p nexus-mailimap -p nexus-mailout`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Email/crates/nexus-mailstore apps/Nexus-Email/crates/nexus-mailapi apps/Nexus-Email/crates/nexus-mailimap apps/Nexus-Email/crates/nexus-mailout apps/Nexus-Email/crates/nexus-mailsmtp
git commit -m "feat: read mail bodies through object service"
```

### Task 3: Enforce object capability and migration rollout fencing

**Files:**
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/object_gateway.rs`
- Modify: `apps/Nexus-Email/crates/nexus-mailsmtp/src/main.rs`
- Modify: `deploy/production/deploy.sh`
- Modify: `apps/Nexus-Email/README.md`
- Test: `apps/Nexus-Email/crates/nexus-mailsmtp/tests/object_gateway.rs`
- Test: `deploy/production/tests/mail-rollout.test.sh`

**Interfaces:**
- Produces `verify_capabilities() -> Result<(), GatewayError>`.
- Requires `NEXUS_EMAIL_OBJECT_STORAGE_MIGRATION_MARKER` with a completed zero-legacy inventory before external-body service startup.

- [ ] **Step 1: Write failing rollout tests**

```rust
#[tokio::test]
async fn capability_probe_rejects_unversioned_or_nonconditional_storage() {
    assert!(gateway_without_versioning_or_conditional_create().verify_capabilities().await.is_err());
}
```

```bash
NEXUS_EMAIL_OBJECT_STORAGE_MIGRATION_MARKER=missing \
  bash deploy/production/tests/mail-rollout.test.sh
```

Expected: non-zero because the external-body service cannot start.

- [ ] **Step 2: Verify RED**

Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 cargo test -p nexus-mailsmtp --test object_gateway capability_probe_rejects_unversioned_or_nonconditional_storage -- --exact`

Expected: FAIL because capability verification does not exist.

- [ ] **Step 3: Implement capability and rollout gate**

```rust
pub async fn verify_capabilities(&self) -> Result<(), GatewayError> {
    self.verify_bucket_versioning().await?;
    self.verify_conditional_create_and_version_delete().await
}
```

Require the marker before enabling external-body uploads, object service, or
cleanup. The deployment script stops Mail writers, applies migrations, checks
the zero legacy-unmanaged inventory, runs the capability probe and
reconciliation dry run, starts only new binaries, then resumes ingress.

- [ ] **Step 4: Verify GREEN**

Run: `cd apps/Nexus-Email && CARGO_BUILD_JOBS=1 bash ./check.sh`

Expected: PASS.

Run: `bash deploy/production/tests/mail-rollout.test.sh`

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Email deploy/production
git commit -m "fix: fence mail object service rollout"
```

## Self-review

- **Spec coverage:** Task 1 creates the loopback authenticated service; Task 2
  makes every separate reader use it; Task 3 fences capabilities and migration
  rollout.
- **Placeholder scan:** No unfinished markers or unspecified tests remain.
- **Type consistency:** Task 1 produces the service endpoint; Task 2 consumes
  it through `ObjectBodyClient`; Task 3 gates its startup.
