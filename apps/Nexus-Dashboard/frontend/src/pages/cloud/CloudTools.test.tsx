import { describe, it, expect, vi, beforeEach, afterEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import CloudTools from "./CloudTools";

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

const TOOLS_BODY = {
  tools: [
    {
      id: "nexus-draw",
      name: "Nexus Draw",
      health: "healthy",
      capabilities: ["chat", "ai"],
      publicUrl: "https://draw.tnhc.dev",
      lastHeartbeatAt: new Date().toISOString(),
    },
    {
      id: "nexus-hosting",
      name: "Nexus Hosting",
      health: "degraded",
      capabilities: [],
      lastHeartbeatAt: new Date().toISOString(),
    },
  ],
};

let fetchSpy: ReturnType<typeof vi.fn> | null = null;

function stubFetch(overrides: Record<string, () => Response> = {}) {
  const spy = vi.fn(async (url: RequestInfo | URL) => {
    const u = String(url);
    if (overrides[u]) return overrides[u]!();
    if (u.startsWith("/ipa/cloud/tools")) return jsonResponse(TOOLS_BODY);
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

describe("CloudTools", () => {
  it("announces loading while the tool registry is pending", () => {
    const spy = vi.fn(() => new Promise<Response>(() => undefined));
    fetchSpy = spy;
    globalThis.fetch = spy as any;
    render(<MemoryRouter><CloudTools /></MemoryRouter>);

    expect(screen.getByRole("status").textContent).toMatch(/loading/i);
  });

  it("renders the tools the proxy returns, capabilities and health included", async () => {
    stubFetch();
    render(<MemoryRouter><CloudTools /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText("Nexus Draw")).toBeTruthy());
    expect(screen.getByText("Nexus Hosting")).toBeTruthy();
    expect(screen.getByText("chat")).toBeTruthy();
    expect(screen.getByText("ai")).toBeTruthy();
    expect(screen.getByText("healthy")).toBeTruthy();
    expect(screen.getByText("degraded")).toBeTruthy();
    expect(screen.getByText("https://draw.tnhc.dev")).toBeTruthy();
    expect(screen.getByRole("link", { name: "Open Nexus Draw" })).toBeTruthy();
    expect(screen.getByRole("columnheader", { name: "Name" }).getAttribute("scope")).toBe("col");
  });

  it("says no tools are registered rather than showing an empty table", async () => {
    stubFetch({ "/ipa/cloud/tools": () => jsonResponse({ tools: [] }) });
    render(<MemoryRouter><CloudTools /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText(/no tools registered/i)).toBeTruthy());
  });

  it("shows Cloud as unavailable, not a blank page, when the proxy returns 503", async () => {
    stubFetch({ "/ipa/cloud/tools": () => jsonResponse({ error: "cloud_unavailable" }, 503) });
    render(<MemoryRouter><CloudTools /></MemoryRouter>);

    await waitFor(() => expect(screen.getByRole("alert")).toBeTruthy());
    expect(screen.getByText(/unavailable/i)).toBeTruthy();
    expect(screen.queryByText(/no tools registered/i)).toBeNull();
  });
});
