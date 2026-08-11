# Signup/Registration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

Goal: Implement the hybrid Nexus-Auth primary + Phantom DID mapping signup flow described in the spec (docs/superpowers/specs/2026-08-11-signup-registration-design.md). Deliverables: DID Mapper service, Nexus-Auth DB migration and endpoints, SDK client, tests, and migration job.

Architecture: Nexus-Auth (IdP) issues tokens and manages user profiles; DID Mapper is a lightweight service storing did<->user mappings; apps use SDK/middleware to validate tokens and query DID Mapper.

Tech Stack: Bun + TypeScript (services), SQLite/Postgres (per-app DB), simple REST for DID Mapper, Phantom SDK JS bindings for signature verification.

## Global Constraints
- Follow existing repo conventions (Bun & TypeScript services under apps/ or services/).
- All new commits use commit message prefix type: and include Co-authored-by trailer.
- Tests: use existing test runner (bun test). Use `scripts/ecosystem-local.sh` for E2E.

---

### Task 1: DID Mapper microservice

**Files:**
- Create: `services/did-mapper/package.json`
- Create: `services/did-mapper/src/index.ts`
- Create: `services/did-mapper/src/db.ts`
- Create: `services/did-mapper/tests/did-mapper.test.ts`

**Interfaces:**
- GET /v1/dids/:did -> { did, user_id, linked_at }
- GET /v1/users/:user_id/did -> { did }
- POST /v1/dids (service auth) -> create mapping { did, user_id }
- DELETE /v1/dids/:did (admin only) -> remove mapping

- [ ] **Step 1: Create package.json**

package.json (minimal)

```json
{
  "name": "did-mapper",
  "version": "0.1.0",
  "scripts": { "start": "bun src/index.ts", "test": "bun test" },
  "devDependencies": {}
}
```

- [ ] **Step 2: Write a minimal server (index.ts)**

services/did-mapper/src/index.ts

```ts
import { serve } from 'bun';
import { getUserByDid, createMapping, deleteMapping } from './db';

const app = serve({
  port: 4001,
  fetch(req) {
    const url = new URL(req.url);
    if (req.method === 'GET' && url.pathname.startsWith('/v1/dids/')) {
      const did = url.pathname.split('/').pop()!;
      const row = getUserByDid(did);
      return new Response(JSON.stringify(row || {}), { status: row ? 200 : 404 });
    }
    if (req.method === 'POST' && url.pathname === '/v1/dids') {
      // TODO: authenticate service call using API key or mTLS (enforce later)
      const body = JSON.parse(Bun.readableStreamToTextSync(req.body));
      const { did, user_id } = body;
      createMapping(did, user_id);
      return new Response('', { status: 201 });
    }
    return new Response('Not Found', { status: 404 });
  }
});

console.log('DID Mapper listening on :4001');
```

- [ ] **Step 3: Implement simple DB layer (db.ts)**

services/did-mapper/src/db.ts

```ts
const DB: Record<string, { did: string; user_id: string; linked_at: string }> = {};
export function getUserByDid(did: string) { return DB[did] ?? null; }
export function createMapping(did: string, user_id: string) { DB[did] = { did, user_id, linked_at: new Date().toISOString() }; }
export function deleteMapping(did: string) { delete DB[did]; }
```

(Implementation note: start with an in-memory store for dev; switch to SQLite/Postgres via knex/pg when moving to production. Add migrations later.)

- [ ] **Step 4: Add tests**

services/did-mapper/tests/did-mapper.test.ts

```ts
import { createMapping, getUserByDid } from '../src/db';
createMapping('did:phantom:abc123','user-1');
if (!getUserByDid('did:phantom:abc123')) throw new Error('mapping failed');
```

- [ ] **Step 5: Run tests & commit**

Run: `cd services/did-mapper && bun test`
Commit: `git add services/did-mapper && git commit -m "feat(did-mapper): scaffold in-memory DID Mapper"`

### Task 2: Add phantom_did column & migration for Nexus-Auth

**Files:**
- Create: `apps/Nexus-Auth/db/migrations/2026-08-11-add-phantom-did.sql`
- Modify: `apps/Nexus-Auth/src/models/user.ts` (or equivalent ORM file)
- Test: `apps/Nexus-Auth/tests/migration.test.ts`

**Step 1: Migration SQL**

`apps/Nexus-Auth/db/migrations/2026-08-11-add-phantom-did.sql`

```sql
ALTER TABLE users ADD COLUMN phantom_did TEXT;
ALTER TABLE users ADD COLUMN did_metadata JSON;
CREATE INDEX IF NOT EXISTS idx_users_phantom_did ON users (phantom_did);
```

- [ ] **Step 2: Update ORM model**

In `apps/Nexus-Auth/src/models/user.ts` add:

```ts
export interface User { id: string; email?: string; username: string; phantom_did?: string | null; did_metadata?: any | null; }
```

- [ ] **Step 3: Write migration test**

`apps/Nexus-Auth/tests/migration.test.ts`

```ts
// connect to test DB, run migrations, assert phantom_did column exists
```

- [ ] **Step 4: Run migrations locally and commit**

Run: `cd apps/Nexus-Auth && bun run migrate` (follow project migration command)
Commit changes.

### Task 3: Include phantom_did in ID token issuance

**Files:**
- Modify: `apps/Nexus-Auth/src/auth/token.ts` (or wherever ID token is constructed)
- Test: `apps/Nexus-Auth/tests/token.test.ts`

- [ ] **Step 1: Write failing test**

`apps/Nexus-Auth/tests/token.test.ts`

```ts
import { issueIdToken } from '../src/auth/token';
const token = issueIdToken({ id: 'user-1', phantom_did: 'did:phantom:abc' });
const payload = decodeJwt(token);
if (payload.phantom_did !== 'did:phantom:abc') throw new Error('phantom_did claim missing');
```

- [ ] **Step 2: Implement**

In token builder: add `if (user.phantom_did) payload.phantom_did = user.phantom_did;`

- [ ] **Step 3: Run tests & commit**

Run: `bun test apps/Nexus-Auth/tests/token.test.ts` and commit.

### Task 4: Endpoint to link existing DID (proof-of-possession)

**Files:**
- Modify: `apps/Nexus-Auth/src/routes/account.ts` (add POST /api/v1/account/link-did)
- Use Phantom SDK helper: `packages/phantom-sdk-js` or `packages/phantom-sdk` if present; otherwise add lightweight verifier wrapper in `packages/phantom-utils/`
- Test: `apps/Nexus-Auth/tests/link-did.test.ts`

- [ ] **Step 1: Write failing test**

```ts
// Simulate a user signing a nonce with their DID key and posting proof; expect mapping created and user phantom_did updated
```

- [ ] **Step 2: Implement endpoint (example)**

```ts
// inside account route
app.post('/api/v1/account/link-did', async (req, res) => {
  const { did, signature, nonce } = await req.json();
  // verify signature using Phantom SDK
  const ok = verifyDidSignature(did, nonce, signature);
  if (!ok) return res.status(400).json({ error: 'invalid signature' });
  // create mapping via DID Mapper service
  await fetch(process.env.DID_MAPPER_URL + '/v1/dids', { method: 'POST', headers: { 'x-api-key': process.env.DID_MAPPER_KEY }, body: JSON.stringify({ did, user_id: req.user.id }) });
  // update local user profile
  await db.users.update({ id: req.user.id }, { phantom_did: did });
  res.status(200).json({ ok: true });
});
```

- [ ] **Step 3: Run integration tests & commit**

### Task 5: SDK / middleware for apps

**Files:**
- Create: `packages/phantom-did-client/src/index.ts`
- Create: `packages/phantom-did-client/tests/client.test.ts`

**API:**
- `getUserIdByDid(did: string): Promise<string | null>`
- `getDidByUserId(userId: string): Promise<string | null>`

Example implementation

```ts
export async function getUserIdByDid(did: string) {
  const base = process.env.DID_MAPPER_URL || 'http://localhost:4001';
  const r = await fetch(`${base}/v1/dids/${encodeURIComponent(did)}`);
  if (!r.ok) return null;
  const j = await r.json();
  return j.user_id;
}
```

Tests: call against local DID Mapper test instance.

### Task 6: Migration job to backfill DIDs

**Files:**
- Create: `scripts/backfill-dids.ts`
- Test: `scripts/tests/backfill.test.ts`

Job behavior: scan users without phantom_did, generate DID using Phantom SDK (or request user to link), write mapping, rate-limit 50/s, idempotent.

### Task 7: Integration & E2E tests

- [ ] Add tests verifying full signup flow: app -> Nexus-Auth -> DID created -> token contains claim -> app fetches DID mapping.
- Use `scripts/ecosystem-local.sh start` to boot local ecosystem; add tests in `scripts/test/e2e-signup.sh`.

### Task 8: Docs & Dev experience

- [ ] Update `docs/LOCAL-DEV.md` with DID Mapper and Nexus-Auth env vars: DID_MAPPER_URL, DID_MAPPER_KEY, DID_MAPPER_PORT
- [ ] Add design & plan links to `docs/superpowers/` and commit.

---

## Self-review checklist
1. Spec coverage: Each spec bullet maps to Tasks 1-6 (mapping service, DB field, token claim, linking flow, SDK, migration).
2. No placeholders: Tests include minimal code; where project-specific commands are unknown, specified `bun test` and typical file paths follow repo conventions.
3. Type consistency: function names and endpoints defined inline and referenced by name across tasks.

Plan saved: docs/superpowers/plans/2026-08-11-signup-registration-plan.md

Execution options: reply with which to use: "Subagent-Driven (recommended)" or "Inline Execution".
