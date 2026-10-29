import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor, fireEvent, within } from "@testing-library/react";

vi.mock("@xterm/xterm", () => ({ Terminal: class {} }));
vi.mock("@xterm/addon-fit", () => ({ FitAddon: class {} }));

// Mock the api module with only the functions needed by these tests
vi.mock("./api", () => ({
  listApps: vi.fn(async () => [
    { id: "nexus-draw", name: "Draw", description: "", publicUrl: "https://draw.tnhc.dev", path: "/draw", delivery: "framed", health: "healthy", surface: "user-app", releaseState: "released", maturity: "stable", visibility: "default", reviewStatus: "approved", primaryShelf: "create", secondaryShelves: [], audiences: ["creator"], capabilities: ["drawing"] },
  ]),
  getCatalogPreferences: vi.fn(async () => ({ pinnedIds: [], recentIds: [], preferredShelf: "everyday" })),
  saveCatalogPreferences: vi.fn(async (preferences) => preferences),
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
  listMaintenance: vi.fn(async () => []),
  setMaintenance: vi.fn(async () => ({ subjectId: "x", part: "x", startedAt: "", setBy: "u" })),
  clearMaintenance: vi.fn(async () => ({ ok: true })),
  isAdmin: vi.fn((user) => user?.role === "founder" || user?.role === "admin"),
  ADMIN_ROLES: ["founder", "admin"],
  listSessions: vi.fn(async () => []),
  myActivity: vi.fn(async () => []),
  remainingRecoveryCodes: vi.fn(async () => 7),
  listCalendarEvents: vi.fn(async () => []),
  createCalendarEvent: vi.fn(async (input) => ({ id: "new-1", ...input })),
  deleteCalendarEvent: vi.fn(async () => undefined),
  ApiError: class ApiError extends Error {
    constructor(public readonly reason: string, message?: string) {
      super(message ?? reason);
      this.name = "ApiError";
    }
  },
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

  it("keeps the old /nexus name as a compatibility redirect to Chat", async () => {
    window.history.pushState({}, "", "/nexus");
    render(<App />);
    await waitFor(() => expect(window.location.pathname).toBe("/chat"));
  });

  it("forwards an old /claim link to the page Nexus-Auth serves, query intact", async () => {
    // The ways in live on the identity host now; this host is behind the
    // sign-in gate. Old links in people's inboxes must still land.
    const original = window.location;
    const replace = vi.fn();
    window.history.pushState({}, "", "/claim?email=ada%40example.com");
    Object.defineProperty(window, "location", {
      configurable: true,
      value: { ...original, pathname: "/claim", search: "?email=ada%40example.com", replace },
    });
    try {
      render(<App />);
      await waitFor(() =>
        expect(replace).toHaveBeenCalledWith("https://auth.tnhc.dev/claim?email=ada%40example.com"),
      );
      expect(screen.queryByRole("banner")).toBeNull();
    } finally {
      Object.defineProperty(window, "location", { configurable: true, value: original });
    }
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
      { id: "nexus-draw", name: "Draw", description: "", publicUrl: "https://draw.tnhc.dev", path: "/draw", delivery: "framed", health: "healthy", surface: "user-app", releaseState: "released", maturity: "stable", visibility: "default", reviewStatus: "approved", primaryShelf: "create", secondaryShelves: [], audiences: ["creator"], capabilities: ["drawing"] },
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

  it("mounts the Calendar view inside the shell at /calendar", async () => {
    window.history.pushState({}, "", "/calendar");
    render(<App />);
    // CalendarView renders a month heading (e.g. "October 2026"). Waiting for
    // the navigation buttons is sufficient to prove the route mounted.
    await waitFor(() => expect(screen.getByRole("button", { name: /previous month/i })).toBeTruthy());
    expect(screen.getByRole("button", { name: /next month/i })).toBeTruthy();
  });
});

describe("shell-native views", () => {
  beforeEach(() => {
    vi.mocked(listApps).mockClear();
  });

  for (const path of [
    "/account", "/admin", "/cloud", "/cloud/tools",
    "/cloud/federation", "/cloud/identity", "/cloud/api", "/terminal",
    "/calendar",
    // C3: Grid was built and tested but unrouted — this proves /apps
    // actually reaches it inside the shell, the browse view behind the
    // drawer's "See all N apps" affordance.
    "/apps",
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
      // pages used to sit beside in a permanent sidebar. Scoped to the dialog
      // itself — /apps renders Grid behind it, which lists the same apps a
      // second time in the page body, and an unscoped query would find both.
      fireEvent.click(screen.getByRole("button", { name: /apps/i }));
      await waitFor(() => {
        const dialog = screen.getByRole("dialog");
        expect(within(dialog).getByRole("link", { name: /Draw/ })).toBeTruthy();
      });
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
