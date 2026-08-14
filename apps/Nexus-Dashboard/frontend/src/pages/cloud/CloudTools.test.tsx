import { describe, it, expect, vi, beforeEach } from "vitest";
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

function stubFetch(overrides: Record<string, () => Response> = {}) {
  const spy = vi.fn(async (url: RequestInfo | URL) => {
    const u = String(url);
    if (overrides[u]) return overrides[u]!();
    if (u.startsWith("/api/cloud/tools")) return jsonResponse(TOOLS_BODY);
    throw new Error(`unexpected fetch: ${u}`);
  });
  vi.stubGlobal("fetch", spy);
  return spy;
}

beforeEach(() => {
  vi.restoreAllMocks();
});

describe("CloudTools", () => {
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
    expect(screen.getByRole("link", { name: /open/i })).toBeTruthy();
  });

  it("says no tools are registered rather than showing an empty table", async () => {
    stubFetch({ "/api/cloud/tools": () => jsonResponse({ tools: [] }) });
    render(<MemoryRouter><CloudTools /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText(/no tools registered/i)).toBeTruthy());
  });

  it("shows Cloud as unavailable, not a blank page, when the proxy returns 503", async () => {
    stubFetch({ "/api/cloud/tools": () => jsonResponse({ error: "cloud_unavailable" }, 503) });
    render(<MemoryRouter><CloudTools /></MemoryRouter>);

    await waitFor(() => expect(screen.getByRole("alert")).toBeTruthy());
    expect(screen.getByText(/unavailable/i)).toBeTruthy();
    expect(screen.queryByText(/no tools registered/i)).toBeNull();
  });
});
