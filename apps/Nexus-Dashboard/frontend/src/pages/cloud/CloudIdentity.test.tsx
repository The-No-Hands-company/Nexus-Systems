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

const IDENTITY_BODY = {
  shortId: "AC-1234",
  did: "did:key:z6Mk...",
  publicKey: "did:key:z6Mk...",
  // No `address` — this is the real shape (`/v1/federation/identity` returns
  // `exampleAddress`, not `address`), documented in CloudIdentity.tsx.
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
  });

  it("shows a dash for NS address rather than blowing up when the field is absent", async () => {
    stubFetch();
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
