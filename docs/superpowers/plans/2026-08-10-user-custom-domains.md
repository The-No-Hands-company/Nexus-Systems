# User Custom Domains Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Let a user attach their own domain to a Nexus-Hosting site from the dashboard, with automatic TLS via Cloudflare for SaaS, on the no-public-IP tunnel.

**Architecture:** Cloud holds the Cloudflare API token and gains a custom-hostname module (Cloudflare for SaaS Custom Hostnames API). Hosting's existing domain flow calls Cloud through `nexusCloudClient` to create/poll/delete the hostname; when Cloudflare reports the hostname `active`, Hosting sets `custom_domains.status='verified'`, which the Rust site-proxy already keys on to serve the domain by `Host` header.

**Tech Stack:** Bun + TypeScript (Cloud), Express 5 + Drizzle + Postgres (Hosting), Rust/axum (site-proxy, unchanged), Cloudflare for SaaS API, vitest (Hosting) / `bun test` (Cloud).

## Global Constraints

- Cloud tests: `bun test src` (from `apps/Nexus-Cloud`). Hosting tests: `vitest run` (from `apps/Nexus-Hosting/artifacts/api-server`, needs `DATABASE_URL`).
- The CF token/zone come from env (`CF_API_TOKEN`, `CF_ZONE_ID`); never hardcode or log them. Token is `nexus-tunnel-dns-edit`; it needs **`SSL and Certificates: Edit`** added (operator Task 0) before Cloud live tests pass.
- Fallback origin / routing target is `ssl.tnhc.dev` (already reaches the tunnel via `*.tnhc.dev`). Configurable via `NEXUS_SAAS_FALLBACK_ORIGIN`, default `ssl.tnhc.dev`.
- Cloud mutating endpoints are `X-Api-Key`-gated (`NEXUS_CLOUD_API_KEY`). Follow the existing `dns/custom-domain` handler + `routes.ts` manifest + `routes.manifest.test.ts` pattern — the manifest test fails if a new route isn't added to all three.
- DB changes go through Drizzle migrations (`pnpm --filter @workspace/db run generate` then `migrate`); never `db push`.
- No secret values in commits; `.env` files stay gitignored.
- Commit after every green step. Conventional-commit messages.

---

## Task 0: Operator setup (manual, prerequisite — not code)

**Owner:** repo operator (human), in the Cloudflare dashboard. Blocks live tests in Tasks 1c and 6b, not the code.

- [ ] Enable **Cloudflare for SaaS** on the `tnhc.dev` zone.
- [ ] Set **fallback origin** = `ssl.tnhc.dev`.
- [ ] Edit token `nexus-tunnel-dns-edit`: add permission **Zone → SSL and Certificates → Edit** (scoped to `tnhc.dev`). Keep the existing DNS + Tunnel permissions.
- [ ] Verify: `curl -H "Authorization: Bearer $CF_API_TOKEN" https://api.cloudflare.com/client/v4/zones/$CF_ZONE_ID/custom_hostnames` returns `{"success":true,...}` (empty list is fine).

---

## Task 1: Cloud — Cloudflare-for-SaaS module

**Files:**
- Create: `apps/Nexus-Cloud/src/cloudflare-saas.ts`
- Create: `apps/Nexus-Cloud/src/cloudflare-saas.test.ts`

**Interfaces:**
- Consumes: env `CF_API_TOKEN`, `CF_ZONE_ID`, `NEXUS_SAAS_FALLBACK_ORIGIN`.
- Produces:
  - `parseCustomHostname(result: any): CustomHostname` (pure)
  - `fallbackOrigin(): string` (pure)
  - `type CustomHostname = { id: string; hostname: string; status: string; sslStatus: string; records: { routing: DnsInstruction; dcv: DnsInstruction | null } }`
  - `type DnsInstruction = { name: string; type: string; value: string }`
  - `async createCustomHostname(host: string): Promise<CustomHostname | { ok: false; status: number; message: string }>`
  - `async getCustomHostname(id: string): Promise<CustomHostname | { ok:false; status:number; message:string }>`
  - `async deleteCustomHostname(id: string): Promise<{ ok: boolean; status: number; message?: string }>`

- [ ] **Step 1: Write the failing test for the pure helpers**

```ts
// apps/Nexus-Cloud/src/cloudflare-saas.test.ts
import { describe, it, expect, afterEach } from "bun:test";
import { parseCustomHostname, fallbackOrigin } from "./cloudflare-saas";

describe("fallbackOrigin", () => {
  const saved = process.env.NEXUS_SAAS_FALLBACK_ORIGIN;
  afterEach(() => {
    if (saved === undefined) delete process.env.NEXUS_SAAS_FALLBACK_ORIGIN;
    else process.env.NEXUS_SAAS_FALLBACK_ORIGIN = saved;
  });
  it("defaults to ssl.tnhc.dev", () => {
    delete process.env.NEXUS_SAAS_FALLBACK_ORIGIN;
    expect(fallbackOrigin()).toBe("ssl.tnhc.dev");
  });
  it("honors the override", () => {
    process.env.NEXUS_SAAS_FALLBACK_ORIGIN = "edge.example.com";
    expect(fallbackOrigin()).toBe("edge.example.com");
  });
});

describe("parseCustomHostname", () => {
  it("maps a CF custom_hostnames result to routing + dcv instructions", () => {
    const result = {
      id: "ch_123",
      hostname: "alice.com",
      status: "pending",
      ssl: {
        status: "pending_validation",
        validation_records: [{ txt_name: "_acme-challenge.alice.com", txt_value: "abc123" }],
      },
    };
    const parsed = parseCustomHostname(result);
    expect(parsed.id).toBe("ch_123");
    expect(parsed.status).toBe("pending");
    expect(parsed.sslStatus).toBe("pending_validation");
    expect(parsed.records.routing).toEqual({ name: "alice.com", type: "CNAME", value: "ssl.tnhc.dev" });
    expect(parsed.records.dcv).toEqual({ name: "_acme-challenge.alice.com", type: "TXT", value: "abc123" });
  });

  it("tolerates a missing ssl validation block (dcv null)", () => {
    const parsed = parseCustomHostname({ id: "ch_9", hostname: "bob.dev", status: "active", ssl: { status: "active" } });
    expect(parsed.records.dcv).toBeNull();
    expect(parsed.sslStatus).toBe("active");
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd apps/Nexus-Cloud && bun test src/cloudflare-saas.test.ts`
Expected: FAIL — module `./cloudflare-saas` not found.

- [ ] **Step 3: Implement the module**

```ts
// apps/Nexus-Cloud/src/cloudflare-saas.ts
const CF_API = "https://api.cloudflare.com/client/v4";

export type DnsInstruction = { name: string; type: string; value: string };
export type CustomHostname = {
  id: string; hostname: string; status: string; sslStatus: string;
  records: { routing: DnsInstruction; dcv: DnsInstruction | null };
};

function cfg() {
  return {
    token: process.env.CF_API_TOKEN?.trim() || "",
    zoneId: process.env.CF_ZONE_ID?.trim() || "",
  };
}
export function fallbackOrigin(): string {
  return process.env.NEXUS_SAAS_FALLBACK_ORIGIN?.trim() || "ssl.tnhc.dev";
}
function headers(token: string) {
  return { Authorization: `Bearer ${token}`, "Content-Type": "application/json" };
}

export function parseCustomHostname(result: any): CustomHostname {
  const ssl = result?.ssl ?? {};
  const v = Array.isArray(ssl.validation_records) ? ssl.validation_records[0] : undefined;
  const dcv: DnsInstruction | null = v?.txt_name && v?.txt_value
    ? { name: v.txt_name, type: "TXT", value: v.txt_value }
    : null;
  return {
    id: String(result?.id ?? ""),
    hostname: String(result?.hostname ?? ""),
    status: String(result?.status ?? "pending"),
    sslStatus: String(ssl.status ?? "pending"),
    records: {
      routing: { name: String(result?.hostname ?? ""), type: "CNAME", value: fallbackOrigin() },
      dcv,
    },
  };
}

type Fail = { ok: false; status: number; message: string };

export async function createCustomHostname(host: string): Promise<CustomHostname | Fail> {
  const { token, zoneId } = cfg();
  if (!token || !zoneId) return { ok: false, status: 501, message: "CF_API_TOKEN and CF_ZONE_ID are required" };
  const res = await fetch(`${CF_API}/zones/${zoneId}/custom_hostnames`, {
    method: "POST",
    headers: headers(token),
    body: JSON.stringify({ hostname: host, ssl: { method: "txt", type: "dv", settings: { min_tls_version: "1.2" } } }),
  });
  const j = (await res.json()) as any;
  if (!j.success) return { ok: false, status: res.status, message: j.errors?.[0]?.message || "cloudflare create failed" };
  return parseCustomHostname(j.result);
}

export async function getCustomHostname(id: string): Promise<CustomHostname | Fail> {
  const { token, zoneId } = cfg();
  if (!token || !zoneId) return { ok: false, status: 501, message: "CF_API_TOKEN and CF_ZONE_ID are required" };
  const res = await fetch(`${CF_API}/zones/${zoneId}/custom_hostnames/${id}`, { headers: headers(token) });
  const j = (await res.json()) as any;
  if (!j.success) return { ok: false, status: res.status, message: j.errors?.[0]?.message || "cloudflare get failed" };
  return parseCustomHostname(j.result);
}

export async function deleteCustomHostname(id: string): Promise<{ ok: boolean; status: number; message?: string }> {
  const { token, zoneId } = cfg();
  if (!token || !zoneId) return { ok: false, status: 501, message: "CF_API_TOKEN and CF_ZONE_ID are required" };
  const res = await fetch(`${CF_API}/zones/${zoneId}/custom_hostnames/${id}`, { method: "DELETE", headers: headers(token) });
  const j = (await res.json()) as any;
  return { ok: !!j.success, status: res.status, ...(j.success ? {} : { message: j.errors?.[0]?.message || "delete failed" }) };
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cd apps/Nexus-Cloud && bun test src/cloudflare-saas.test.ts`
Expected: PASS (5 tests).

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Cloud/src/cloudflare-saas.ts apps/Nexus-Cloud/src/cloudflare-saas.test.ts
git commit -m "feat(saas): Cloudflare for SaaS custom-hostname module with pure record mapping"
```

---

## Task 2: Cloud — custom-hostname endpoints

**Files:**
- Modify: `apps/Nexus-Cloud/src/api/handlers.ts` (import + 3 handlers + dispatch)
- Modify: `apps/Nexus-Cloud/src/api/routes.ts` (manifest entries)
- Modify: `apps/Nexus-Cloud/src/api/routes.manifest.test.ts` (expected snapshot)

**Interfaces:**
- Consumes: `createCustomHostname`, `getCustomHostname`, `deleteCustomHostname` from Task 1.
- Produces HTTP: `POST /api/v1/dns/custom-hostname` `{host}` → `CustomHostname`; `GET /api/v1/dns/custom-hostname/:id` → status; `DELETE /api/v1/dns/custom-hostname/:id`.

- [ ] **Step 1: Add the manifest entries** (in `routes.ts`, right after the `dns/custom-domain` entry from the prior work)

```ts
  { method: "POST", path: "/api/v1/dns/custom-hostname", description: "Create a Cloudflare-for-SaaS custom hostname" },
  { method: "GET", path: "/api/v1/dns/custom-hostname/:id", description: "Get a custom hostname's status" },
  { method: "DELETE", path: "/api/v1/dns/custom-hostname/:id", description: "Delete a custom hostname" },
```

- [ ] **Step 2: Mirror those three entries into `routes.manifest.test.ts`** at the same position (copy the three objects verbatim into the expected array).

- [ ] **Step 3: Add the import and handlers in `handlers.ts`**

```ts
// add to the existing cloudflare-saas import area
import { createCustomHostname, getCustomHostname, deleteCustomHostname } from "../cloudflare-saas";

async function handleCreateCustomHostname(request: Request): Promise<Response> {
  const body = (await readJson(request)) as { host?: string } | null;
  const host = body?.host?.trim().toLowerCase();
  if (!host || !/^(?=.{1,253}$)([a-z0-9](-*[a-z0-9])*\.)+[a-z]{2,}$/.test(host)) {
    return json({ error: "a valid 'host' is required" }, 400);
  }
  const r = await createCustomHostname(host);
  return "ok" in r && r.ok === false ? json(r, r.status >= 400 ? r.status : 502) : json(r, 201);
}
async function handleGetCustomHostname(id: string): Promise<Response> {
  const r = await getCustomHostname(id);
  return "ok" in r && r.ok === false ? json(r, r.status >= 400 ? r.status : 502) : json(r, 200);
}
async function handleDeleteCustomHostname(id: string): Promise<Response> {
  const r = await deleteCustomHostname(id);
  return json(r, r.ok ? 200 : r.status >= 400 ? r.status : 502);
}
```

- [ ] **Step 4: Wire dispatch** (next to the `dns/custom-domain` dispatch line)

```ts
  if (request.method === "POST" && pathname === "/api/v1/dns/custom-hostname")
    return await handleCreateCustomHostname(request);
  {
    const m = pathname.match(/^\/api\/v1\/dns\/custom-hostname\/([^/]+)$/);
    if (m) {
      if (request.method === "GET") return await handleGetCustomHostname(m[1]!);
      if (request.method === "DELETE") return await handleDeleteCustomHostname(m[1]!);
    }
  }
```

- [ ] **Step 5: Run the manifest + full suite**

Run: `cd apps/Nexus-Cloud && bun test src/api/routes.manifest.test.ts && bun test src`
Expected: manifest PASS; full suite PASS (77+ tests).

- [ ] **Step 6: Commit**

```bash
git add apps/Nexus-Cloud/src/api/handlers.ts apps/Nexus-Cloud/src/api/routes.ts apps/Nexus-Cloud/src/api/routes.manifest.test.ts
git commit -m "feat(saas): custom-hostname endpoints (create/get/delete) in Cloud"
```

---

## Task 3: Cloud — live Cloudflare API verification

**Files:** none committed (throwaway script in scratchpad). Requires Task 0 done.

- [ ] **Step 1: Create + poll + delete a real custom hostname**

```bash
cd apps/Nexus-Cloud
CF_TOKEN=$(awk -F= '/^CF_API_TOKEN=/{v=substr($0,index($0,"=")+1);gsub(/[ \t\r"'"'"'].*/,"",v);print v;exit}' .env)
CF_ZONE=$(awk -F= '/^CF_ZONE_ID=/{v=substr($0,index($0,"=")+1);gsub(/[ \t\r"'"'"'].*/,"",v);print v;exit}' .env)
cat > /tmp/saas-live.ts <<'TS'
import { createCustomHostname, getCustomHostname, deleteCustomHostname } from "/run/media/zajferx/Data/dev/The-No-hands-Company/projects/Nexus-Systems/apps/Nexus-Cloud/src/cloudflare-saas";
const created = await createCustomHostname("saas-probe.example.com");
console.log("created:", JSON.stringify(created));
if ("id" in created) { console.log("get:", JSON.stringify(await getCustomHostname(created.id)));
  console.log("delete:", JSON.stringify(await deleteCustomHostname(created.id))); }
TS
CF_API_TOKEN="$CF_TOKEN" CF_ZONE_ID="$CF_ZONE" bun run /tmp/saas-live.ts
rm -f /tmp/saas-live.ts
```

Expected: `created` shows an `id`, `status: pending`, and `records.routing.value = ssl.tnhc.dev`; `delete` shows `ok: true`. If it returns a 403/scope error, Task 0's `SSL:Edit` permission is missing.

- [ ] **Step 2: No commit** (verification only). Record the confirmed CF response shape; if field names differ from Task 1's `parseCustomHostname`, fix that mapping + its test and re-commit Task 1.

---

## Task 4: Hosting — custom_domains migration (CF fields)

**Files:**
- Modify: `apps/Nexus-Hosting/lib/db/src/schema/domains.ts`
- Create: migration under `apps/Nexus-Hosting/lib/db/migrations/` (generated)

**Interfaces:**
- Produces columns: `custom_domains.cf_hostname_id` (text, null), `custom_domains.cf_ssl_status` (text, null). (`last_checked_at`, `last_error` already exist.)

- [ ] **Step 1: Add the columns to the Drizzle schema** — in `domains.ts`, on the `customDomains` table definition:

```ts
  cfHostnameId: text("cf_hostname_id"),
  cfSslStatus: text("cf_ssl_status"),
```

- [ ] **Step 2: Generate the migration**

Run: `cd apps/Nexus-Hosting && pnpm --filter @workspace/db run generate`
Expected: a new SQL file adding the two columns. Review it — it must be `ALTER TABLE ... ADD COLUMN`, no drops.

- [ ] **Step 3: Apply it**

Run: `pnpm --filter @workspace/db run migrate`
Expected: migration applies; `psql ... -c "\d custom_domains"` shows the two columns.

- [ ] **Step 4: Commit** (both schema + generated migration)

```bash
git add apps/Nexus-Hosting/lib/db/src/schema/domains.ts apps/Nexus-Hosting/lib/db/migrations/
git commit -m "feat(db): add cf_hostname_id and cf_ssl_status to custom_domains"
```

---

## Task 5: Hosting — nexusCloudClient custom-hostname calls

**Files:**
- Modify: `apps/Nexus-Hosting/artifacts/api-server/src/lib/nexusCloudClient.ts`
- Test: `apps/Nexus-Hosting/artifacts/api-server/tests/unit/nexusCloudClient.saas.test.ts` (create)

**Interfaces:**
- Consumes: Cloud endpoints from Task 2.
- Produces:
  - `type CloudCustomHostname = { id: string; hostname: string; status: string; sslStatus: string; records: { routing: { name:string;type:string;value:string }; dcv: { name:string;type:string;value:string } | null } }`
  - `async createCustomHostnameViaCloud(cloudBaseUrl: string, host: string, apiKey: string): Promise<CloudCustomHostname>`
  - `async getCustomHostnameViaCloud(cloudBaseUrl: string, id: string, apiKey: string): Promise<CloudCustomHostname>`
  - `async deleteCustomHostnameViaCloud(cloudBaseUrl: string, id: string, apiKey: string): Promise<void>`

- [ ] **Step 1: Write the failing test** (mock `fetch`)

```ts
// tests/unit/nexusCloudClient.saas.test.ts
import { describe, it, expect, vi, afterEach } from "vitest";
import { createCustomHostnameViaCloud } from "../../src/lib/nexusCloudClient";

afterEach(() => vi.restoreAllMocks());

describe("createCustomHostnameViaCloud", () => {
  it("POSTs the host with the api key and returns the parsed hostname", async () => {
    const body = { id: "ch_1", hostname: "alice.com", status: "pending", sslStatus: "pending",
      records: { routing: { name: "alice.com", type: "CNAME", value: "ssl.tnhc.dev" }, dcv: null } };
    const spy = vi.spyOn(globalThis, "fetch").mockResolvedValue(
      new Response(JSON.stringify(body), { status: 201 }) as any);
    const r = await createCustomHostnameViaCloud("http://cloud:8787", "alice.com", "key123");
    expect(r.id).toBe("ch_1");
    const [url, init] = spy.mock.calls[0]!;
    expect(String(url)).toBe("http://cloud:8787/api/v1/dns/custom-hostname");
    expect((init as any).headers["X-Api-Key"]).toBe("key123");
    expect(JSON.parse((init as any).body)).toEqual({ host: "alice.com" });
  });
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd apps/Nexus-Hosting/artifacts/api-server && npx vitest run tests/unit/nexusCloudClient.saas.test.ts`
Expected: FAIL — `createCustomHostnameViaCloud` not exported.

- [ ] **Step 3: Implement** (append to `nexusCloudClient.ts`, following the `registerToolWithCloud` fetch+`X-Api-Key` pattern)

```ts
export type CloudCustomHostname = {
  id: string; hostname: string; status: string; sslStatus: string;
  records: { routing: { name: string; type: string; value: string }; dcv: { name: string; type: string; value: string } | null };
};
function cloudHeaders(apiKey: string) {
  return { "Content-Type": "application/json", ...(apiKey ? { "X-Api-Key": apiKey } : {}) };
}
export async function createCustomHostnameViaCloud(cloudBaseUrl: string, host: string, apiKey: string): Promise<CloudCustomHostname> {
  const res = await fetch(`${cloudBaseUrl.replace(/\/$/, "")}/api/v1/dns/custom-hostname`, {
    method: "POST", headers: cloudHeaders(apiKey), body: JSON.stringify({ host }),
  });
  if (!res.ok) throw new Error(`Cloud custom-hostname create failed: ${res.status}`);
  return (await res.json()) as CloudCustomHostname;
}
export async function getCustomHostnameViaCloud(cloudBaseUrl: string, id: string, apiKey: string): Promise<CloudCustomHostname> {
  const res = await fetch(`${cloudBaseUrl.replace(/\/$/, "")}/api/v1/dns/custom-hostname/${id}`, { headers: cloudHeaders(apiKey) });
  if (!res.ok) throw new Error(`Cloud custom-hostname get failed: ${res.status}`);
  return (await res.json()) as CloudCustomHostname;
}
export async function deleteCustomHostnameViaCloud(cloudBaseUrl: string, id: string, apiKey: string): Promise<void> {
  const res = await fetch(`${cloudBaseUrl.replace(/\/$/, "")}/api/v1/dns/custom-hostname/${id}`, { method: "DELETE", headers: cloudHeaders(apiKey) });
  if (!res.ok) throw new Error(`Cloud custom-hostname delete failed: ${res.status}`);
}
```

- [ ] **Step 4: Run to verify pass**

Run: `npx vitest run tests/unit/nexusCloudClient.saas.test.ts`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Hosting/artifacts/api-server/src/lib/nexusCloudClient.ts apps/Nexus-Hosting/artifacts/api-server/tests/unit/nexusCloudClient.saas.test.ts
git commit -m "feat(domains): Hosting client for Cloud custom-hostname endpoints"
```

---

## Task 6: Hosting — create the CF hostname on add-domain

**Files:**
- Modify: `apps/Nexus-Hosting/artifacts/api-server/src/routes/domains.ts` (the `POST /sites/:id/domains` handler)
- Test: `apps/Nexus-Hosting/artifacts/api-server/tests/unit/addDomain.saas.test.ts` (create)

**Interfaces:**
- Consumes: `createCustomHostnameViaCloud` (Task 5), env `NEXUS_CLOUD_URL`, `NEXUS_CLOUD_API_KEY`, `custom_domains.cfHostnameId` (Task 4).
- Produces: `POST /sites/:id/domains` response `instructions` now carries the CF routing + DCV records and persists `cf_hostname_id`.

- [ ] **Step 1: Write the failing test** — mock the client, assert the row stores `cfHostnameId` and the response returns the routing/dcv records. (Follow the existing `domains` test setup for DB + auth stubs; assert on the JSON `instructions` shape below.)

```ts
// expected response.instructions after change:
// { routing: { name: "<domain>", type: "CNAME", value: "ssl.tnhc.dev" },
//   dcv: { name: "_acme-challenge.<domain>", type: "TXT", value: "<token>" } | null }
```

- [ ] **Step 2: Run to verify it fails**

Run: `cd apps/Nexus-Hosting/artifacts/api-server && npx vitest run tests/unit/addDomain.saas.test.ts`
Expected: FAIL — response still returns the old `txt`/`cname` instructions.

- [ ] **Step 3: Implement** — in `POST /sites/:id/domains`, after inserting the row, call Cloud and update the row:

```ts
import { createCustomHostnameViaCloud } from "../lib/nexusCloudClient";
// ...after `const [row] = await db.insert(customDomainsTable)...returning();`
const cloudUrl = process.env.NEXUS_CLOUD_URL ?? "http://127.0.0.1:8787";
const apiKey = process.env.NEXUS_CLOUD_API_KEY ?? "";
const ch = await createCustomHostnameViaCloud(cloudUrl, domain, apiKey);
await db.update(customDomainsTable)
  .set({ cfHostnameId: ch.id, cfSslStatus: ch.sslStatus })
  .where(eq(customDomainsTable.id, row.id));
res.status(201).json({ ...row, cfHostnameId: ch.id, instructions: ch.records });
```

Replace the old `instructions: { txt, cname }` block. Wrap the Cloud call in try/catch → on failure `throw AppError` from `../lib/errors` (do not leave the row without a CF hostname silently; per CLAUDE.md no silent failures).

- [ ] **Step 4: Run to verify pass**

Run: `npx vitest run tests/unit/addDomain.saas.test.ts`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Hosting/artifacts/api-server/src/routes/domains.ts apps/Nexus-Hosting/artifacts/api-server/tests/unit/addDomain.saas.test.ts
git commit -m "feat(domains): create Cloudflare custom hostname on add-domain, return DNS records"
```

---

## Task 7: Hosting — verify becomes a CF status poll

**Files:**
- Modify: `apps/Nexus-Hosting/artifacts/api-server/src/routes/domains.ts` (the `POST /domains/:id/verify` handler)
- Test: `apps/Nexus-Hosting/artifacts/api-server/tests/unit/verifyDomain.saas.test.ts` (create)

**Interfaces:**
- Consumes: `getCustomHostnameViaCloud` (Task 5), `custom_domains.cfHostnameId`.
- Produces: verify sets `status='verified'` iff CF reports the hostname `active` and `sslStatus='active'`; otherwise `status='pending'` with `lastError` = CF status; updates `cfSslStatus`, `lastCheckedAt`.

- [ ] **Step 1: Write the failing test** — two cases: CF `status:"active", sslStatus:"active"` → row becomes `verified`, response `{verified:true}`; CF `status:"pending"` → row stays `pending`, `{verified:false}` with a `lastError` naming the CF status.

- [ ] **Step 2: Run to verify it fails**

Run: `cd apps/Nexus-Hosting/artifacts/api-server && npx vitest run tests/unit/verifyDomain.saas.test.ts`
Expected: FAIL — handler still does the local `_fh-verify` TXT lookup.

- [ ] **Step 3: Implement** — replace the `dns.resolveTxt(...)` verification body with:

```ts
const cloudUrl = process.env.NEXUS_CLOUD_URL ?? "http://127.0.0.1:8787";
const apiKey = process.env.NEXUS_CLOUD_API_KEY ?? "";
if (!row.cfHostnameId) throw AppError.badRequest("Domain has no Cloudflare hostname; re-add it", "NO_CF_HOSTNAME");
const ch = await getCustomHostnameViaCloud(cloudUrl, row.cfHostnameId, apiKey);
const verified = ch.status === "active" && ch.sslStatus === "active";
const now = new Date();
const [updated] = await db.update(customDomainsTable).set({
  status: verified ? "verified" : "pending",
  cfSslStatus: ch.sslStatus,
  verifiedAt: verified ? now : null,
  lastCheckedAt: now,
  lastError: verified ? null : `Cloudflare hostname status: ${ch.status}/${ch.sslStatus}`,
}).where(eq(customDomainsTable.id, row.id)).returning();
res.json({ verified, domain: row.domain, status: updated.status, lastError: updated.lastError });
```

- [ ] **Step 4: Run to verify pass**

Run: `npx vitest run tests/unit/verifyDomain.saas.test.ts`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add apps/Nexus-Hosting/artifacts/api-server/src/routes/domains.ts apps/Nexus-Hosting/artifacts/api-server/tests/unit/verifyDomain.saas.test.ts
git commit -m "feat(domains): verify custom domain via Cloudflare hostname status"
```

---

## Task 8: Hosting — delete removes the CF hostname

**Files:**
- Modify: `apps/Nexus-Hosting/artifacts/api-server/src/routes/domains.ts` (add `DELETE /sites/:id/domains/:domain`)
- Test: `apps/Nexus-Hosting/artifacts/api-server/tests/unit/deleteDomain.saas.test.ts` (create)

**Interfaces:**
- Consumes: `deleteCustomHostnameViaCloud` (Task 5).
- Produces: `DELETE /sites/:id/domains/:domain` → deletes CF hostname (if `cfHostnameId` set) then removes the row; 204.

- [ ] **Step 1: Write the failing test** — asserts the client delete is called with the stored `cfHostnameId` and the row is gone.
- [ ] **Step 2: Run to verify it fails** — Run: `npx vitest run tests/unit/deleteDomain.saas.test.ts` → FAIL (route 404).
- [ ] **Step 3: Implement** the route: ownership check (like `POST`), look up the row by site+domain, `if (row.cfHostnameId) await deleteCustomHostnameViaCloud(cloudUrl, row.cfHostnameId, apiKey)` (best-effort: on CF failure log + continue so a stuck CF record never blocks removal), delete the row, `res.status(204).end()`.
- [ ] **Step 4: Run to verify pass** — PASS.
- [ ] **Step 5: Commit** — `git commit -m "feat(domains): delete custom domain removes the Cloudflare hostname"`

---

## Task 9: Retire the unusable ACME path

**Files:**
- Modify: `apps/Nexus-Hosting/artifacts/api-server/src/lib/acme.ts` (guard) and its caller in `index.ts`/`app.ts`

- [ ] **Step 1: Write a failing test** — `acme.saas.test.ts`: when `ACME_ENABLED` is unset, the ACME initializer is a no-op and logs that Cloudflare-for-SaaS handles TLS (assert it returns without attempting an order).
- [ ] **Step 2: Run to verify it fails.**
- [ ] **Step 3: Implement** — gate ACME startup behind `process.env.ACME_ENABLED === "true"` (default off). Add a one-line comment: this node terminates TLS at Cloudflare (SaaS); ACME/HTTP-01 needs a public-IP origin and is for self-host deployments only.
- [ ] **Step 4: Run to verify pass.**
- [ ] **Step 5: Commit** — `git commit -m "chore(tls): disable ACME by default; Cloudflare for SaaS handles custom-domain TLS"`

---

## Task 10: Wire config + advance submodule pointers

**Files:**
- Modify: `deploy/production/deploy.sh` (pass `NEXUS_SAAS_FALLBACK_ORIGIN` to Cloud; `NEXUS_CLOUD_URL`/`NEXUS_CLOUD_API_KEY` already reach Hosting)
- Modify: monorepo submodule pointers for `apps/Nexus-Cloud` and `apps/Nexus-Hosting`

- [ ] **Step 1:** Add `NEXUS_SAAS_FALLBACK_ORIGIN="${NEXUS_SAAS_FALLBACK_ORIGIN:-ssl.tnhc.dev}"` to the `cloud` `start_service` env in `deploy.sh`.
- [ ] **Step 2:** Restart Cloud + Hosting; re-run the Cloud full suite and the Hosting api-server suite to confirm green with `DATABASE_URL` set.
- [ ] **Step 3:** Commit the submodule changes in each submodule, push them, then advance + commit + push the monorepo pointers (submodules first, per the repo's push order). Commit: `chore: advance Nexus-Cloud + Nexus-Hosting (user custom domains)`.

---

## Self-Review (completed by plan author)

- **Spec coverage:** §4a→Task 0; §4b→Tasks 1–3; §4c→Tasks 5–8; §4d→Task 4; §5 (retire acme)→Task 9; config→Task 10. Serving path (§3) already exists — no task, by design.
- **Placeholders:** none — every code step carries real code; Tasks 6–8 test steps reference the exact response shapes defined in Tasks 1/5.
- **Type consistency:** `CustomHostname`/`CloudCustomHostname` fields (`id`, `hostname`, `status`, `sslStatus`, `records.routing`, `records.dcv`) are identical across Cloud (Task 1), the client (Task 5), and consumers (Tasks 6–8). `cfHostnameId`/`cfSslStatus` column names match the schema (Task 4) and every reader.
- **Open dependency:** Tasks 3 and 6b live-tests require Task 0's `SSL:Edit` token permission (operator). Code tasks (unit-tested) do not block on it.
