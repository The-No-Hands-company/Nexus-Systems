import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import CloudIdentity from "./CloudIdentity";

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

/**
 * The real shape. The endpoint returns `exampleAddress` and `namingScheme`,
 * never `address` — and `@alice:…` is an illustration of how names are formed
 * on this node, not an address anyone holds.
 */
const IDENTITY_BODY = {
  shortId: "AC-1234",
  did: "did:key:z6Mk...",
  publicKey: "did:key:z6Mk...",
  namingScheme: "@user:shortId",
  exampleAddress: "@alice:AC-1234",
};

function stubFetch(overrides: Record<string, () => Response> = {}) {
  const spy = vi.fn(async (url: RequestInfo | URL) => {
    const u = String(url);
    if (overrides[u]) return overrides[u]!();
    if (u.startsWith("/api/cloud/federation/identity")) return jsonResponse(IDENTITY_BODY);
    throw new Error(`unexpected fetch: ${u}`);
  });
  vi.stubGlobal("fetch", spy);
  return spy;
}

beforeEach(() => {
  vi.restoreAllMocks();
});

describe("CloudIdentity", () => {
  it("renders the identity fields the proxy returns", async () => {
    stubFetch();
    render(<MemoryRouter><CloudIdentity /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText("AC-1234")).toBeTruthy());
    expect(screen.getAllByText("did:key:z6Mk...").length).toBeGreaterThan(0);
    // The address example must actually reach the screen. Reading the wrong
    // field name is precisely how this page rendered a dash for months.
    expect(screen.getByText("@alice:AC-1234")).toBeTruthy();
  });

  it("labels the address as a format, never as this node's own address", async () => {
    // `@alice:AC-1234` under a bare "NS address" heading states that this node
    // is @alice. It is an illustration of the naming scheme and has to read
    // as one.
    stubFetch();
    render(<MemoryRouter><CloudIdentity /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText("@alice:AC-1234")).toBeTruthy());
    expect(screen.getByText(/Address format/i)).toBeTruthy();
    expect(screen.getByText(/not a registered address/i)).toBeTruthy();
  });

  it("falls back to the naming scheme, then a dash, when the example is absent", async () => {
    stubFetch({
      "/api/cloud/federation/identity": () =>
        jsonResponse({ shortId: "AC-1234", did: "did:key:z6Mk..." }),
    });
    render(<MemoryRouter><CloudIdentity /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText("AC-1234")).toBeTruthy());
    expect(screen.getByText("—")).toBeTruthy();
  });

  it("shows Cloud as unavailable, not a blank page, when the proxy returns 503", async () => {
    stubFetch({ "/api/cloud/federation/identity": () => jsonResponse({ error: "cloud_unavailable" }, 503) });
    render(<MemoryRouter><CloudIdentity /></MemoryRouter>);

    await waitFor(() => expect(screen.getByRole("alert")).toBeTruthy());
    expect(screen.getByText(/unavailable/i)).toBeTruthy();
    expect(screen.queryByText("AC-1234")).toBeNull();
  });
});
