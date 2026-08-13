import { describe, it, expect, vi, beforeEach } from "vitest";
import { render, screen, waitFor } from "@testing-library/react";

vi.mock("./api", async () => ({
  ...(await vi.importActual<typeof import("./api")>("./api")),
  listApps: vi.fn(async () => [
    { id: "nexus-draw", name: "Draw", description: "", url: "https://draw.tnhc.dev", health: "healthy" },
  ]),
  me: vi.fn(async () => ({ username: "founder", role: "founder" })),
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
