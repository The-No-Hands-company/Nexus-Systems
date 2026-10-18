import { describe, it, expect, beforeEach } from "vitest";
import { render, screen, fireEvent, within } from "@testing-library/react";
import { MemoryRouter, useLocation } from "react-router-dom";
import { filterApps, emptyQueryApps, EMPTY_QUERY_CAP } from "../filterApps";
import AppsDrawer from "../AppsDrawer";
import type { AppEntry } from "../../api";

function app(id: string, name: string, health: AppEntry["health"]): AppEntry {
  return { id, name, description: "", publicUrl: `https://${id}.tnhc.dev`, path: `/${id}`, delivery: "framed", health, surface: "user-app", releaseState: "released", maturity: "stable", visibility: "default", reviewStatus: "approved" };
}

const APPS: AppEntry[] = [
  app("nexus-draw", "Draw", "healthy"),
  app("nexus-chat", "Chat", "healthy"),
  app("nexus-forge", "Forge", "offline"),
  app("nexus-vault", "Vault", "offline"),
];

describe("filterApps", () => {
  it("matches on name and on id", () => {
    expect(filterApps(APPS, "draw").map((a) => a.id)).toEqual(["nexus-draw"]);
    expect(filterApps(APPS, "nexus-vault").map((a) => a.id)).toEqual([]);
  });

  it("is case-insensitive", () => {
    expect(filterApps(APPS, "CHAT").map((a) => a.id)).toEqual(["nexus-chat"]);
  });

  it("omits offline apps from search", () => {
    expect(filterApps(APPS, "").map((a) => a.health)).toEqual(["healthy", "healthy"]);
  });

  it("returns released user apps for an empty query", () => {
    expect(filterApps(APPS, "  ")).toHaveLength(2);
  });

  it("returns nothing rather than everything for no match", () => {
    expect(filterApps(APPS, "zzzz")).toEqual([]);
  });

  it("never revives offline entries when the query matches", () => {
    expect(filterApps(APPS, "forge").map((a) => a.id)).toEqual([]);
  });
});

describe("emptyQueryApps", () => {
  it("never returns offline apps", () => {
    const result = emptyQueryApps(APPS);
    expect(result.every((a) => a.health === "healthy")).toBe(true);
  });

  it("is bounded, never all of the ecosystem", () => {
    const many = Array.from({ length: 86 }, (_, i) => app(`nexus-app-${i}`, `App ${i}`, "healthy"));
    const result = emptyQueryApps(many);
    expect(result.length).toBeLessThanOrEqual(EMPTY_QUERY_CAP);
    expect(result.length).toBeLessThan(many.length);
  });

  it("shows pinned apps first", () => {
    const result = emptyQueryApps(APPS, { pinnedIds: ["nexus-chat"] });
    expect(result[0]!.id).toBe("nexus-chat");
  });

  it("falls through to recent, then the rest of the healthy apps", () => {
    const result = emptyQueryApps(APPS, { pinnedIds: [], recentIds: ["nexus-draw"] });
    expect(result[0]!.id).toBe("nexus-draw");
    expect(result.map((a) => a.id)).toContain("nexus-chat");
  });

  it("ignores a pinned id that is not actually healthy", () => {
    // A pin can go stale (the app went offline after it was pinned) — it
    // must not resurrect an offline entry into the bounded list.
    const result = emptyQueryApps(APPS, { pinnedIds: ["nexus-forge"] });
    expect(result.map((a) => a.id)).not.toContain("nexus-forge");
  });
});

describe("AppsDrawer", () => {
  function open() {
    return render(
      <MemoryRouter>
        <AppsDrawer apps={APPS} open onClose={() => {}} />
      </MemoryRouter>,
    );
  }

  it("does not show offline apps in the empty (default) state", () => {
    // C2: the drawer's own empty state must never be the wall of 86 apps the
    // drawer exists to remove — and offline apps specifically must not appear
    // until someone searches for them.
    open();
    expect(screen.queryByText("Forge")).toBeNull();
    expect(screen.queryByText("Vault")).toBeNull();
    expect(screen.getByText("Draw")).toBeTruthy();
    expect(screen.getByText("Chat")).toBeTruthy();
  });

  it("keeps offline apps out when searched", () => {
    open();
    fireEvent.change(screen.getByRole("searchbox"), { target: { value: "forge" } });
    expect(screen.queryByText("Forge")).toBeNull();
    expect(screen.getByText(/no apps match/i)).toBeTruthy();
  });

  it("does not render an offline app as a link", () => {
    open();
    fireEvent.change(screen.getByRole("searchbox"), { target: { value: "forge" } });
    expect(screen.queryByRole("link", { name: "Forge" })).toBeNull();
  });

  it("makes a healthy app a link to its in-shell path", () => {
    open();
    const healthy = screen.getByText("Draw").closest("a")!;
    expect(healthy.getAttribute("href")).toBe("/nexus-draw");
  });

  it("filters as you type", () => {
    open();
    fireEvent.change(screen.getByRole("searchbox"), { target: { value: "chat" } });
    expect(screen.queryByText("Draw")).toBeNull();
    expect(screen.getByText("Chat")).toBeTruthy();
  });

  it("tells you when nothing matched instead of showing an empty box", () => {
    open();
    fireEvent.change(screen.getByRole("searchbox"), { target: { value: "zzzz" } });
    expect(screen.getByText(/no apps match/i)).toBeTruthy();
  });

  it("offers a way to browse every app", () => {
    // C3: Grid must be reachable from the UI, not orphaned behind its own
    // tests. The drawer's "see all" link is that affordance.
    open();
    const link = screen.getByRole("link", { name: /see all .* apps/i });
    expect(link.getAttribute("href")).toBe("/apps");
  });

  it("moves the highlight with the arrow keys", () => {
    open();
    const box = screen.getByRole("searchbox");
    fireEvent.keyDown(box, { key: "ArrowDown" });
    const options = within(screen.getByRole("listbox")).getAllByRole("option");
    expect(options[1]!.getAttribute("aria-selected")).toBe("true");
  });

  it("opens the highlighted app on Enter", () => {
    function LocationProbe() {
      const location = useLocation();
      return <span data-testid="path">{location.pathname}</span>;
    }
    render(
      <MemoryRouter>
        <AppsDrawer apps={APPS} open onClose={() => {}} />
        <LocationProbe />
      </MemoryRouter>,
    );
    const box = screen.getByRole("searchbox");
    // Empty-query results are alphabetical (Chat, then Draw); the highlight
    // starts on the first result, so Enter with no arrow keys opens Chat.
    fireEvent.keyDown(box, { key: "Enter" });
    expect(screen.getByTestId("path").textContent).toBe("/nexus-chat");
  });

  it("does nothing when Enter is pressed on a highlighted offline app", () => {
    open();
    const box = screen.getByRole("searchbox");
    fireEvent.change(box, { target: { value: "forge" } });
    fireEvent.keyDown(box, { key: "Enter" });
    // Still on the same query, still showing the offline entry — Enter must
    // be a no-op, not a navigation to nowhere.
    expect(screen.queryByText("Forge")).toBeNull();
  });
});
