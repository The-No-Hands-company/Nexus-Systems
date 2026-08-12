import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";
import Grid from "./Grid";

const APPS = [
  { id: "nexus-chat", name: "Nexus Chat", description: "Real-time messaging",
    url: "https://chat.tnhc.dev", health: "healthy" },
  { id: "nexus-draw", name: "Nexus Draw", description: "Whiteboard",
    url: "https://draw.tnhc.dev", health: "offline" },
];

function jsonResponse(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { "content-type": "application/json" },
  });
}

beforeEach(() => {
  vi.restoreAllMocks();
});

describe("Grid", () => {
  it("renders a tile per app, linking to its subdomain", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({ apps: APPS })));

    render(<Grid />);
    await waitFor(() => expect(screen.getByText("Nexus Chat")).toBeTruthy());
    expect(screen.getByRole("link", { name: /nexus chat/i }).getAttribute("href"))
      .toBe("https://chat.tnhc.dev");
  });

  it("marks a down app as unavailable rather than offering a dead link", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({ apps: APPS })));

    render(<Grid />);
    await waitFor(() => expect(screen.getByText("Nexus Draw")).toBeTruthy());
    expect(screen.getByText(/unavailable|offline/i)).toBeTruthy();
    // An offline app must not be a link at all — a click that goes nowhere is
    // worse than a tile that says why.
    expect(screen.queryByRole("link", { name: /nexus draw/i })).toBeNull();
  });

  it("says so when no apps are reachable instead of rendering an empty page", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({ apps: [] })));

    render(<Grid />);
    await waitFor(() => expect(screen.getByText(/no apps|nothing available/i)).toBeTruthy());
  });

  it("reports a failure to load rather than looking like an empty ecosystem", async () => {
    // An empty grid and a broken grid must not look identical: one means "you
    // have no apps", the other means "we could not ask".
    vi.stubGlobal("fetch", vi.fn(async () => { throw new TypeError("offline"); }));

    render(<Grid />);
    await waitFor(() => expect(screen.getByText(/could not load|try again|failed/i)).toBeTruthy());
  });

  it("reads the grid from the same-origin endpoint", async () => {
    const spy = vi.fn(async (_url: RequestInfo | URL, _init?: RequestInit) =>
      jsonResponse({ apps: APPS }));
    vi.stubGlobal("fetch", spy);

    render(<Grid />);
    await waitFor(() => expect(spy).toHaveBeenCalled());
    expect(String(spy.mock.calls[0]![0])).toBe("/api/apps");
  });

  it("shows each app's description so the grid is legible to a newcomer", async () => {
    vi.stubGlobal("fetch", vi.fn(async () => jsonResponse({ apps: APPS })));

    render(<Grid />);
    await waitFor(() => expect(screen.getByText("Real-time messaging")).toBeTruthy());
  });
});
