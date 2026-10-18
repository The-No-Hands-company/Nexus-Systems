import { render, screen, waitFor } from "@testing-library/react";
import { afterEach, describe, expect, it, vi } from "vitest";
import { useCatalogPreferences } from "../useCatalogPreferences";

function Probe() {
  const { preferences, loading } = useCatalogPreferences();
  return <output data-testid="preferences">{loading ? "loading" : JSON.stringify(preferences)}</output>;
}

afterEach(() => {
  vi.restoreAllMocks();
  localStorage.clear();
});

describe("useCatalogPreferences", () => {
  it("uses account preferences when Auth is available", async () => {
    vi.stubGlobal("fetch", vi.fn().mockResolvedValue(new Response(JSON.stringify({
      preferences: { pinnedIds: ["nexus-chat"], recentIds: ["nexus-mail"], preferredShelf: "work" },
    }), { status: 200, headers: { "content-type": "application/json" } })));
    render(<Probe />);
    await waitFor(() => expect(screen.getByTestId("preferences").textContent).toContain('"preferredShelf":"work"'));
    expect(screen.getByTestId("preferences").textContent).toContain("nexus-chat");
  });

  it("falls back to the pre-migration local keys when Auth is unavailable", async () => {
    localStorage.setItem("nexus.pinnedApps", JSON.stringify(["nexus-draw"]));
    localStorage.setItem("nexus.recentApps", JSON.stringify(["nexus-video"]));
    localStorage.setItem("nexus.preferredShelf", "create");
    vi.stubGlobal("fetch", vi.fn().mockRejectedValue(new Error("offline")));
    render(<Probe />);
    await waitFor(() => expect(screen.getByTestId("preferences").textContent).toContain('"preferredShelf":"create"'));
    expect(screen.getByTestId("preferences").textContent).toContain("nexus-draw");
  });
});
