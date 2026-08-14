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

const STATUS_BODY = {
  tools: { total: 9, healthy: 7, degraded: 1, offline: 1 },
  users: { total: 42 },
  peers: { total: 3 },
  node: { id: "node-x" },
};

const IDENTITY_BODY = { address: "ns:acme.federation-test-node", shortId: "AC-1234", did: "did:key:z6Mk..." };

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

    // Identity: comes from federation/identity, not the status payload's
    // node.*. It appears both as the node title and the NS Address field.
    await waitFor(() => expect(screen.getAllByText(IDENTITY_BODY.address).length).toBeGreaterThan(0));
    expect(screen.getByText(IDENTITY_BODY.did)).toBeTruthy();

    // Stats row reflects the status payload, not hardcoded numbers.
    expect(screen.getByText("7")).toBeTruthy(); // healthy tools
    expect(screen.getByText("9 registered")).toBeTruthy();
    expect(screen.getByText("42")).toBeTruthy(); // users

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
    expect(screen.queryByText(IDENTITY_BODY.address)).toBeNull();
  });

  it("still renders the dashboard when only the optional trust card fails", async () => {
    // Trust/audit/tools are best-effort in the original loadDashboard — only
    // status and identity are load-bearing.
    stubFetch({
      "/api/cloud/status?compact=trust": () => jsonResponse({ error: "cloud_unavailable" }, 503),
    });
    render(<MemoryRouter><CloudOverview /></MemoryRouter>);

    await waitFor(() => expect(screen.getAllByText(IDENTITY_BODY.address).length).toBeGreaterThan(0));
    expect(screen.queryByText(/trust lifecycle/i)).toBeNull();
  });
});
