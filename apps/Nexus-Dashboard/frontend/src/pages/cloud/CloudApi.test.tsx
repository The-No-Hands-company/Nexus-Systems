import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import CloudApi from "./CloudApi";

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

const ENDPOINTS_BODY = {
  endpoints: [
    { method: "GET", path: "/api/v1/tools", description: "List registered tools" },
    { method: "POST", path: "/api/v1/tools", description: "Register a tool" },
    { method: "GET", path: "/v1/federation/peers", description: "List known federation peers" },
  ],
};

function stubFetch(overrides: Record<string, () => Response> = {}) {
  const spy = vi.fn(async (url: RequestInfo | URL) => {
    const u = String(url);
    if (overrides[u]) return overrides[u]!();
    if (u.startsWith("/api/cloud/endpoints")) return jsonResponse(ENDPOINTS_BODY);
    throw new Error(`unexpected fetch: ${u}`);
  });
  vi.stubGlobal("fetch", spy);
  return spy;
}

beforeEach(() => {
  vi.restoreAllMocks();
});

describe("CloudApi", () => {
  it("announces loading while endpoint discovery is pending", () => {
    vi.stubGlobal("fetch", vi.fn(() => new Promise<Response>(() => undefined)));
    render(<MemoryRouter><CloudApi /></MemoryRouter>);

    expect(screen.getByRole("status").textContent).toMatch(/loading/i);
  });

  it("renders the routes the proxy returns, grouped by path prefix", async () => {
    stubFetch();
    render(<MemoryRouter><CloudApi /></MemoryRouter>);

    await waitFor(() => expect(screen.getAllByText("/api/v1/tools").length).toBeGreaterThan(0));
    // Two routes share the /api/v1/tools group; GET and POST both appear
    // (GET also labels the federation route, so there are two).
    expect(screen.getAllByText("GET").length).toBeGreaterThan(0);
    expect(screen.getByText("POST")).toBeTruthy();
    expect(screen.getByText("List registered tools")).toBeTruthy();
    // /v1/federation/peers groups under /v1/federation (v1-prefixed paths
    // group on two segments, not three — status.html's grouping rule).
    expect(screen.getByText("/v1/federation")).toBeTruthy();
  });

  it("says no routes were found rather than showing an empty page", async () => {
    stubFetch({ "/api/cloud/endpoints": () => jsonResponse({ endpoints: [] }) });
    render(<MemoryRouter><CloudApi /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText(/no routes found/i)).toBeTruthy());
  });

  it("shows Cloud as unavailable, not a blank page, when the proxy returns 503", async () => {
    stubFetch({ "/api/cloud/endpoints": () => jsonResponse({ error: "cloud_unavailable" }, 503) });
    render(<MemoryRouter><CloudApi /></MemoryRouter>);

    await waitFor(() => expect(screen.getByRole("alert")).toBeTruthy());
    expect(screen.getByText(/unavailable/i)).toBeTruthy();
    expect(screen.queryByText(/no routes found/i)).toBeNull();
  });
});
