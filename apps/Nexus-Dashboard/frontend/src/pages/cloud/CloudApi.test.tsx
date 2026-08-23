import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
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
    { method: "GET", path: "/ipa/v1/tools", description: "List registered tools" },
    { method: "POST", path: "/ipa/v1/tools", description: "Register a tool" },
    { method: "GET", path: "/v1/federation/peers", description: "List known federation peers" },
  ],
};

let fetchSpy: ReturnType<typeof vi.fn> | null = null;

function stubFetch(overrides: Record<string, () => Response> = {}) {
  const spy = vi.fn(async (url: RequestInfo | URL) => {
    const u = String(url);
    if (overrides[u]) return overrides[u]!();
    if (u.startsWith("/ipa/cloud/endpoints")) return jsonResponse(ENDPOINTS_BODY);
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

describe("CloudApi", () => {
  it("announces loading while endpoint discovery is pending", () => {
    const spy = vi.fn(() => new Promise<Response>(() => undefined));
    fetchSpy = spy;
    globalThis.fetch = spy as any;
    render(<MemoryRouter><CloudApi /></MemoryRouter>);

    expect(screen.getByRole("status").textContent).toMatch(/loading/i);
  });

  it("renders the routes the proxy returns, grouped by path prefix", async () => {
    stubFetch();
    render(<MemoryRouter><CloudApi /></MemoryRouter>);

    await waitFor(() => expect(screen.getAllByText("/ipa/v1/tools").length).toBeGreaterThan(0));
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
    stubFetch({ "/ipa/cloud/endpoints": () => jsonResponse({ endpoints: [] }) });
    render(<MemoryRouter><CloudApi /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText(/no routes found/i)).toBeTruthy());
  });

  it("shows Cloud as unavailable, not a blank page, when the proxy returns 503", async () => {
    stubFetch({ "/ipa/cloud/endpoints": () => jsonResponse({ error: "cloud_unavailable" }, 503) });
    render(<MemoryRouter><CloudApi /></MemoryRouter>);

    await waitFor(() => expect(screen.getByRole("alert")).toBeTruthy());
    expect(screen.getByText(/unavailable/i)).toBeTruthy();
    expect(screen.queryByText(/no routes found/i)).toBeNull();
  });
});
