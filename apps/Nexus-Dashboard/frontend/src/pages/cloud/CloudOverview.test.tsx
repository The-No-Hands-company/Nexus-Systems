import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor, within } from "@testing-library/react";
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
  // `mode` is in the real payload; the fixture lacked it, which is how the
  // trust panel rendered "mode: unknown" instead of naming the node's state.
  status: {
    mode: "standalone",
    toolCount: 9,
    healthyToolCount: 7,
    exposedToolCount: 4,
    trust: { peers: { total: 3 } },
  },
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
    // Healthy and heartbeating, but with no address — the scaffold shape that
    // made "healthy tools" a misleading headline. Must not count as reachable.
    {
      id: "nexus-recipes",
      name: "Nexus Recipes",
      health: "healthy",
      lastHeartbeatAt: new Date().toISOString(),
    },
    {
      id: "nexus-draw",
      name: "Nexus Draw",
      health: "healthy",
      publicUrl: "https://draw.tnhc.dev",
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
  it("announces loading while the load-bearing Cloud requests are pending", () => {
    vi.stubGlobal("fetch", vi.fn(() => new Promise<Response>(() => undefined)));
    render(<MemoryRouter><CloudOverview /></MemoryRouter>);

    expect(screen.getByRole("status").textContent).toMatch(/loading/i);
  });

  it("renders the real data returned by the proxy — node identity, stats, trust and audit", async () => {
    stubFetch();
    render(<MemoryRouter><CloudOverview /></MemoryRouter>);

    // Identity comes from federation/identity. The status payload has no
    // node.* block to fall back to — it never did.
    await waitFor(() => expect(screen.getAllByText(IDENTITY_BODY.exampleAddress).length).toBeGreaterThan(0));
    expect(screen.getByText(IDENTITY_BODY.did)).toBeTruthy();

    // Stats row reflects the payload, not hardcoded numbers. The headline is
    // reachable — registered, healthy AND carrying a public URL — because a
    // heartbeat from a scaffold that serves nothing is not a working service.
    // Scoped to the card: a bare "1" also matches the trust pills.
    const reachableCard = screen.getByText("Reachable tools").closest(".rounded-lg");
    expect(reachableCard?.textContent).toContain("1"); // only nexus-draw has an address
    expect(reachableCard?.textContent).toContain("7 heartbeating");
    expect(reachableCard?.textContent).toContain("9 registered");
    expect(screen.getByText("4")).toBeTruthy(); // exposed tools
    expect(screen.getByRole("navigation", { name: "Cloud console" })).toBeTruthy();

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

  it("uses the current status counters instead of legacy nested totals", async () => {
    stubFetch({
      "/api/cloud/status": () => jsonResponse({
        status: {
          mode: "standalone", toolCount: 86, healthyToolCount: 36,
          exposedToolCount: 7, trust: { peers: { total: 0 }, nodes: { total: 0 } },
        },
      }),
    });
    render(<MemoryRouter><CloudOverview /></MemoryRouter>);

    expect(await screen.findByText("86 registered")).toBeTruthy();
    expect(screen.getByRole("navigation", { name: "Cloud console" })).toBeTruthy();
  });

  it("names this node in the trust panel rather than reporting no nodes at all", async () => {
    stubFetch();
    render(<MemoryRouter><CloudOverview /></MemoryRouter>);
    await waitFor(() => expect(screen.getByText("Trust lifecycle")).toBeTruthy());

    // The registry counts relationships with OTHER nodes, so it is empty on a
    // standalone node. Labelling that "Nodes (0 total) — none" told an operator
    // looking straight at a node that no nodes existed.
    // Scoped: the shortId also appears in the identity section above.
    const thisNode = screen.getByText("This node").closest("div");
    expect(thisNode?.textContent).toContain(IDENTITY_BODY.shortId);
    expect(thisNode?.textContent).toContain("standalone");
    // Labelled as *other* nodes, so an empty registry reads as "none besides
    // this one" rather than "no nodes exist".
    expect(screen.getByText(/Other nodes \(5\)/)).toBeTruthy();
    expect(screen.queryByText(/^Nodes \(5 total\)$/)).toBeNull();
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

  it("marks only the internal-service card unavailable when the tools registry fails", async () => {
    // Treating a failed optional request as [] reports a trustworthy-looking
    // 0/3 and labels every service "not registered", which is false evidence.
    stubFetch({
      "/api/cloud/tools": () => jsonResponse({ error: "cloud_unavailable" }, 503),
    });
    render(<MemoryRouter><CloudOverview /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText("Internal cloud services")).toBeTruthy());
    const card = screen.getByText("Internal cloud services").closest(".rounded-lg");
    expect(card).not.toBeNull();
    expect(within(card as HTMLElement).getByText(/registry unavailable/i)).toBeTruthy();
    expect(within(card as HTMLElement).queryByText(/0\/3 online/i)).toBeNull();
    expect(within(card as HTMLElement).queryByText(/not registered/i)).toBeNull();

    const reachableCard = screen.getByText("Reachable tools").closest(".rounded-lg");
    expect(reachableCard).not.toBeNull();
    expect(within(reachableCard as HTMLElement).getByText("—")).toBeTruthy();
    expect(within(reachableCard as HTMLElement).queryByText(/^0$/)).toBeNull();

    // The optional-card failure must not replace the load-bearing overview.
    expect(screen.getByText(IDENTITY_BODY.did)).toBeTruthy();
  });
});
