import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import CloudFederation from "./CloudFederation";

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

const PEERS_BODY = {
  peers: [
    { domain: "peer-a.example.com", trustLevel: "trusted", lastSeen: new Date().toISOString(), address: "ns:a.node-y" },
    // A real FederationPeer as Cloud actually returns it: no trustLevel, an
    // object for `trust`, and neither lastSeen nor address/nodeAddress —
    // exactly the drift documented in CloudFederation.tsx.
    { domain: "peer-b.example.com", trust: { identity: "did:nexus:b", issuer: "peer-b" } },
  ],
};

let fetchSpy: ReturnType<typeof vi.fn> | null = null;

function stubFetch(overrides: Record<string, () => Response> = {}) {
  const spy = vi.fn(async (url: RequestInfo | URL) => {
    const u = String(url);
    if (overrides[u]) return overrides[u]!();
    if (u.startsWith("/ipa/cloud/federation/peers")) return jsonResponse(PEERS_BODY);
    throw new Error(`unexpected fetch: ${u}`);
  });
  fetchSpy = spy;
  globalThis.fetch = spy as any;
  return spy;
}

beforeEach(() => {
  vi.restoreAllMocks();
});

afterEach(() => {
  if (fetchSpy) {
    delete (globalThis as any).fetch;
    fetchSpy = null;
  }
});

describe("CloudFederation", () => {
  it("announces loading while federation peers are pending", () => {
    const spy = vi.fn(() => new Promise<Response>(() => undefined));
    fetchSpy = spy;
    globalThis.fetch = spy as any;
    render(<MemoryRouter><CloudFederation /></MemoryRouter>);

    expect(screen.getByRole("status").textContent).toMatch(/loading/i);
  });

  it("renders the peers the proxy returns", async () => {
    stubFetch();
    render(<MemoryRouter><CloudFederation /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText("peer-a.example.com")).toBeTruthy());
    expect(screen.getByText("trusted")).toBeTruthy();
    expect(screen.getByText("ns:a.node-y")).toBeTruthy();
    expect(screen.getByRole("columnheader", { name: "Domain" }).getAttribute("scope")).toBe("col");
  });

  it("does not crash when a peer's trust field is the real object shape, not a display string", async () => {
    stubFetch();
    render(<MemoryRouter><CloudFederation /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText("peer-b.example.com")).toBeTruthy());
  });

  it("names the bootstrap hint when there are no peers, matching status.html's empty state", async () => {
    stubFetch({ "/ipa/cloud/federation/peers": () => jsonResponse({ peers: [] }) });
    render(<MemoryRouter><CloudFederation /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText(/no federation peers discovered/i)).toBeTruthy());
    expect(screen.getByText(/BOOTSTRAP_PEERS/)).toBeTruthy();
  });

  it("shows Cloud as unavailable, not a blank page, when the proxy returns 503", async () => {
    stubFetch({ "/ipa/cloud/federation/peers": () => jsonResponse({ error: "cloud_unavailable" }, 503) });
    render(<MemoryRouter><CloudFederation /></MemoryRouter>);

    await waitFor(() => expect(screen.getByRole("alert")).toBeTruthy());
    expect(screen.getByText(/unavailable/i)).toBeTruthy();
    expect(screen.queryByText(/no federation peers discovered/i)).toBeNull();
  });
});
