import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";

vi.mock("./api", async () => ({
  ...(await vi.importActual<typeof import("./api")>("./api")),
  listApps: vi.fn(async () => [
    { id: "nexus-draw", name: "Draw", description: "", url: "https://draw.tnhc.dev", health: "healthy" },
  ]),
  me: vi.fn(async () => ({ username: "founder", role: "founder" })),
  // Every /cloud/* page has its own dedicated, data-asserting tests in
  // pages/cloud/*.test.tsx. Here they only need to resolve so the routing
  // tests below aren't exercising real network calls.
  cloudStatus: vi.fn(async () => ({ tools: { total: 1, healthy: 1 }, users: { total: 1 }, peers: { total: 0 } })),
  cloudIdentity: vi.fn(async () => ({ address: "ns:test", shortId: "T-1", did: "did:key:test" })),
  cloudTrust: vi.fn(async () => null),
  cloudAudit: vi.fn(async () => []),
  cloudTools: vi.fn(async () => []),
  cloudUsers: vi.fn(async () => []),
  cloudFederationPeers: vi.fn(async () => []),
  cloudEndpoints: vi.fn(async () => []),
}));

import App from "./App";
import { listApps, type AppEntry } from "./api";

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
      { id: "nexus-draw", name: "Draw", description: "", url: "https://draw.tnhc.dev", health: "healthy" },
    ]);

    await waitFor(() => expect(screen.getByTitle("Draw")).toBeTruthy());
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
    "/cloud/users", "/cloud/federation", "/cloud/identity", "/cloud/api",
  ]) {
    it(`wraps ${path} in the shell, launcher and all`, async () => {
      window.history.pushState({}, "", path);
      render(<App />);

      await waitFor(() => expect(screen.getByRole("banner")).toBeTruthy());
      expect(screen.getByRole("complementary", { name: "Applications" })).toBeTruthy();
      // Populated, not merely present: these pages sit beside the app list
      // rather than replacing it.
      await waitFor(() => expect(screen.getByRole("link", { name: /Draw/ })).toBeTruthy());
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

  it("leaves the signed-in home free of shell chrome, so the launcher is not doubled", async () => {
    // / renders the launcher grid; the shell's sidebar is also a launcher.
    // Wrapping it would list the same apps twice on one screen.
    window.history.pushState({}, "", "/");
    render(<App />);

    await waitFor(() => expect(screen.queryByRole("banner")).toBeNull());
  });
});
