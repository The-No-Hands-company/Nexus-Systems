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

describe("shell routing", () => {
  beforeEach(() => {
    window.history.pushState({}, "", "/a/nexus-draw");
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
});
