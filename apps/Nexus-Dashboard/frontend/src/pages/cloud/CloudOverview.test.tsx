import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import CloudOverview from "./CloudOverview";

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

/**
 * The real shape of Cloud's /api/v1/status, transcribed from the live
 * response. The earlier fixture used `tools.total` / `users.total` /
 * `peers.total`, which the endpoint has never returned — so the tests passed
 * against a shape that does not exist while the page showed zeros in
 * production. A fixture that agrees with the code but not the server proves
 * nothing.
 */
const STATUS_BODY = {
  toolCount: 9,
  healthyToolCount: 7,
  exposedToolCount: 4,
  trust: { peers: { total: 3 } },
};

/**
 * `exampleAddress`, not `address` — and it is an illustration of the naming
 * scheme, not an address this node holds.
 */
const IDENTITY_BODY = {
  exampleAddress: "@alice:AC-1234",
  namingScheme: "@user:shortId",
  shortId: "AC-1234",
  did: "did:key:z6Mk...",
};

const TRUST_BODY = {
  trust: {
    nodes: { total: 5, trusted: 3, pending: 2 },
    peers: { total: 2, verified: 2 },
    updatedAt: new Date().toISOString(),
  },
};

const AUDIT_BODY = {
  events: [
    {
      subjectId: "peer-galaxy-brain",
      timestamp: new Date().toISOString(),
      metadata: { action: "promote", actor: "founder", previousState: "pending", nextState: "trusted" },
    },
  ],
};

const TOOLS_BODY = {
  tools: [
    {
      id: "nexus-guardian",
      name: "Nexus Guardian",
      health: "healthy",
      capabilities: ["security"],
      lastHeartbeatAt: new Date().toISOString(),
    },
  ],
};

function stubFetch(overrides: Record<string, () => Response> = {}) {
  const spy = vi.fn(async (url: RequestInfo | URL) => {
    const u = String(url);
    for (const [key, fn] of Object.entries(overrides)) {
      if (u.startsWith(key)) return fn();
    }
    if (u.startsWith("/api/cloud/status?compact=trust")) return jsonResponse(TRUST_BODY);
    if (u.startsWith("/api/cloud/status")) return jsonResponse(STATUS_BODY);
    if (u.startsWith("/api/cloud/federation/identity")) return jsonResponse(IDENTITY_BODY);
    if (u.startsWith("/api/cloud/audit")) return jsonResponse(AUDIT_BODY);
    if (u.startsWith("/api/cloud/tools")) return jsonResponse(TOOLS_BODY);
    throw new Error(`unexpected fetch: ${u}`);
  });
  vi.stubGlobal("fetch", spy);
  return spy;
}

beforeEach(() => {
  vi.restoreAllMocks();
});

describe("CloudOverview", () => {
  it("renders the real data returned by the proxy — node identity, stats, trust and audit", async () => {
    stubFetch();
    render(<MemoryRouter><CloudOverview /></MemoryRouter>);

    // Identity comes from federation/identity. The status payload has no
    // node.* block to fall back to — it never did.
    await waitFor(() => expect(screen.getAllByText(IDENTITY_BODY.exampleAddress).length).toBeGreaterThan(0));
    expect(screen.getByText(IDENTITY_BODY.did)).toBeTruthy();

    // Stats row reflects the status payload, not hardcoded numbers.
    expect(screen.getByText("7")).toBeTruthy(); // healthy tools
    expect(screen.getByText("9 registered")).toBeTruthy();
    expect(screen.getByText("4")).toBeTruthy(); // exposed tools

    // Trust lifecycle pills.
    expect(screen.getByText(/3 trusted/)).toBeTruthy();
    expect(screen.getByText(/2 pending/)).toBeTruthy();

    // Recent trust action, from the audit feed.
    expect(screen.getByText("peer-galaxy-brain")).toBeTruthy();
    expect(screen.getByText(/pending → trusted/)).toBeTruthy();

    // Internal service card: Guardian is in the tool registry response, so it
    // must show as online rather than "not registered".
    expect(screen.getByText("Nexus Guardian")).toBeTruthy();
    expect(screen.queryAllByText("not registered").length).toBe(2); // Edge, Tunnel absent
  });

  it("shows Cloud as unavailable, not a blank page, when the proxy returns 503", async () => {
    // This is the proxy's degrade path (see server.ts proxyToCloud) — Cloud
    // being down must not take the shell page down with it.
    stubFetch({ "/api/cloud/status": () => jsonResponse({ error: "cloud_unavailable" }, 503) });
    render(<MemoryRouter><CloudOverview /></MemoryRouter>);

    await waitFor(() => expect(screen.getByRole("alert")).toBeTruthy());
    expect(screen.getByText(/unavailable/i)).toBeTruthy();
    expect(screen.queryByText(IDENTITY_BODY.exampleAddress)).toBeNull();
  });

  it("still renders the dashboard when only the optional trust card fails", async () => {
    // Trust/audit/tools are best-effort in the original loadDashboard — only
    // status and identity are load-bearing.
    stubFetch({
      "/api/cloud/status?compact=trust": () => jsonResponse({ error: "cloud_unavailable" }, 503),
    });
    render(<MemoryRouter><CloudOverview /></MemoryRouter>);

    await waitFor(() => expect(screen.getAllByText(IDENTITY_BODY.exampleAddress).length).toBeGreaterThan(0));
    expect(screen.queryByText(/trust lifecycle/i)).toBeNull();
  });
});
