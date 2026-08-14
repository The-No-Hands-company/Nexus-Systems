import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import CloudUsers from "./CloudUsers";

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

const USERS_BODY = {
  users: [
    { id: "u1", username: "founder", address: "ns:founder.node-x", nodeId: "N-1", registeredAt: new Date().toISOString() },
    { id: "u2", username: "operator", address: "ns:operator.node-x", nodeId: "N-1", registeredAt: new Date().toISOString() },
  ],
};

function stubFetch(overrides: Record<string, () => Response> = {}) {
  const spy = vi.fn(async (url: RequestInfo | URL) => {
    const u = String(url);
    if (overrides[u]) return overrides[u]!();
    if (u.startsWith("/api/cloud/users")) return jsonResponse(USERS_BODY);
    throw new Error(`unexpected fetch: ${u}`);
  });
  vi.stubGlobal("fetch", spy);
  return spy;
}

beforeEach(() => {
  vi.restoreAllMocks();
});

describe("CloudUsers", () => {
  it("renders the users the proxy returns, one row per account", async () => {
    stubFetch();
    render(<MemoryRouter><CloudUsers /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText("founder")).toBeTruthy());
    expect(screen.getByText("operator")).toBeTruthy();
    expect(screen.getByText("ns:founder.node-x")).toBeTruthy();
    expect(screen.getByText("ns:operator.node-x")).toBeTruthy();
  });

  it("says no users are registered rather than showing an empty table", async () => {
    stubFetch({ "/api/cloud/users": () => jsonResponse({ users: [] }) });
    render(<MemoryRouter><CloudUsers /></MemoryRouter>);

    await waitFor(() => expect(screen.getByText(/no users registered/i)).toBeTruthy());
  });

  it("explains a 403 as a permissions problem, not a generic error or an empty roster", async () => {
    // The dashboard proxy's own admin gate (server.ts CLOUD_ALLOWLIST
    // users.adminOnly) returns exactly this shape to a non-admin caller.
    stubFetch({ "/api/cloud/users": () => jsonResponse({ error: "forbidden" }, 403) });
    render(<MemoryRouter><CloudUsers /></MemoryRouter>);

    await waitFor(() => expect(screen.getByRole("alert")).toBeTruthy());
    expect(screen.getByText(/do not have permission/i)).toBeTruthy();
    // Must not read as either of the other two failure shapes.
    expect(screen.queryByText(/no users registered/i)).toBeNull();
    expect(screen.queryByText(/cloud is unavailable/i)).toBeNull();
  });

  it("shows Cloud as unavailable, not a blank page, when the proxy returns 503", async () => {
    stubFetch({ "/api/cloud/users": () => jsonResponse({ error: "cloud_unavailable" }, 503) });
    render(<MemoryRouter><CloudUsers /></MemoryRouter>);

    await waitFor(() => expect(screen.getByRole("alert")).toBeTruthy());
    expect(screen.getByText(/cloud is unavailable/i)).toBeTruthy();
    expect(screen.queryByText(/no users registered/i)).toBeNull();
    expect(screen.queryByText(/do not have permission/i)).toBeNull();
  });
});
