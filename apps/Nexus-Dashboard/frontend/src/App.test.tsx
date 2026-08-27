import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor, fireEvent } from "@testing-library/react";

vi.mock("@xterm/xterm", () => ({ Terminal: class {} }));
vi.mock("@xterm/addon-fit", () => ({ FitAddon: class {} }));

// Mock the api module with only the functions needed by these tests
vi.mock("./api", () => ({
  listApps: vi.fn(async () => [
    { id: "nexus-draw", name: "Draw", description: "", url: "https://draw.tnhc.dev", path: "/draw", health: "healthy" },
  ]),
  me: vi.fn(async () => ({
    id: "user-founder",
    username: "founder",
    email: "founder@example.test",
    role: "founder",
  })),
  cloudStatus: vi.fn(async () => ({ tools: { total: 1, healthy: 1 }, users: { total: 1 }, peers: { total: 0 } })),
  cloudIdentity: vi.fn(async () => ({ address: "ns:test", shortId: "T-1", did: "did:key:test" })),
  cloudTrust: vi.fn(async () => null),
  cloudAudit: vi.fn(async () => []),
  cloudTools: vi.fn(async () => []),
  cloudFederationPeers: vi.fn(async () => []),
  cloudEndpoints: vi.fn(async () => []),
  unreadNotificationCount: vi.fn(async () => 0),
  isAdmin: vi.fn((user) => user?.role === "founder" || user?.role === "admin"),
  ADMIN_ROLES: ["founder", "admin"],
  listSessions: vi.fn(async () => []),
  remainingRecoveryCodes: vi.fn(async () => 7),
}));

import App from "./App";
import { listApps, me, type AppEntry } from "./api";

describe("shell routing", () => {
  beforeEach(() => {
    window.history.pushState({}, "", "/a/nexus-draw");
    vi.mocked(listApps).mockClear();
  });

  it("mounts the requested app inside the shell", async () => {
    render(<App />);
    await waitFor(() => expect(screen.getByTitle("Draw")).toBeTruthy());
    // Chrome and app together: the shell is present, not replaced.
    expect(screen.getByRole("banner")).toBeTruthy();
  });

  it("leaves the public claim page free of shell chrome", async () => {
    // Someone claiming an account has no session and no apps to launch;
    // wrapping that page in a launcher would be nonsense.
    window.history.pushState({}, "", "/claim");
    render(<App />);
    await waitFor(() => expect(screen.queryByRole("banner")).toBeNull());
  });

  it("does not report a valid app as not found while the list is still loading", async () => {
    // Hold the fetch open so we can inspect the in-between state.
    let resolveList: (apps: AppEntry[]) => void = () => {};
    vi.mocked(listApps).mockImplementationOnce(
      () => new Promise((resolve) => { resolveList = resolve; }),
    );

    render(<App />);

    // Still loading: a real app must not flash "not found" while its own
    // existence is simply unknown yet.
    expect(screen.queryByText("App not found.")).toBeNull();
    expect(screen.queryByTitle("Draw")).toBeNull();

    resolveList([
      { id: "nexus-draw", name: "Draw", description: "", url: "https://draw.tnhc.dev", path: "/draw", health: "healthy" },
    ]);

    await waitFor(() => expect(screen.getByTitle("Draw")).toBeTruthy());
  });

  it("keeps old /a/:id links working by redirecting to the flat path", async () => {
    // Someone's bookmark from before the URL scheme changed must not 404
    // because we tidied the scheme.
    window.history.pushState({}, "", "/a/nexus-draw");
    render(<App />);
    await waitFor(() => expect(screen.getByTitle("Draw")).toBeTruthy());
    expect(window.location.pathname).toBe("/draw");
  });

  it("reports a load failure distinctly from an unknown app", async () => {
    vi.mocked(listApps).mockRejectedValueOnce(new Error("network"));

    render(<App />);

    await waitFor(() => expect(screen.getByText("Could not load your apps.")).toBeTruthy());
    // The two failure modes need different user action, so they must not share text.
    expect(screen.queryByText("App not found.")).toBeNull();
  });
});

describe("shell-native views", () => {
  beforeEach(() => {
    vi.mocked(listApps).mockClear();
  });

  for (const path of [
    "/account", "/admin", "/cloud", "/cloud/tools",
    "/cloud/federation", "/cloud/identity", "/cloud/api", "/terminal",
  ]) {
    it(`wraps ${path} in the shell, apps drawer and all`, async () => {
      if (path === "/terminal") {
        vi.mocked(me).mockResolvedValueOnce({
          id: "user-member",
          username: "member",
          email: "member@example.test",
          role: "member",
        });
      }
      window.history.pushState({}, "", path);
      render(<App />);

      await waitFor(() => expect(screen.getByRole("banner")).toBeTruthy());
      expect(screen.getByRole("navigation", { name: "Shell" })).toBeTruthy();
      // Populated, not merely present: the drawer carries the app list these
      // pages used to sit beside in a permanent sidebar.
      fireEvent.click(screen.getByRole("button", { name: /apps/i }));
      await waitFor(() => expect(screen.getByRole("link", { name: /Draw/ })).toBeTruthy());
      if (path === "/terminal") {
        expect(screen.getByText("Terminal access required")).toBeTruthy();
      }
    });
  }

  it("still renders the account page when the app list fails", async () => {
    // The distinction that matters. For /a/:appId a failed list means the app
    // cannot be shown; here it only means an empty sidebar. Treating it as
    // fatal would turn an unrelated network blip into a broken account page.
    vi.mocked(listApps).mockRejectedValueOnce(new Error("network"));
    window.history.pushState({}, "", "/account");

    render(<App />);

    await waitFor(() => expect(screen.getByRole("banner")).toBeTruthy());
    // The frame's error state belongs to app-mounting, not to this page.
    expect(screen.queryByText("Could not load your apps.")).toBeNull();
  });

  it("wraps the signed-in home in the shell, same as every other signed-in route", async () => {
    // / used to render bare on the theory that the shell's sidebar was also a
    // launcher, so wrapping it would double the app list. The drawer replaced
    // that sidebar, so the reason to keep Home bare is gone — and keeping it
    // bare anyway was the actual bug: the front door had no header, so it
    // looked like a different, older application than everything behind it.
    window.history.pushState({}, "", "/");
    render(<App />);

    await waitFor(() => expect(screen.getByRole("banner")).toBeTruthy());
  });
});
