import { describe, it, expect, beforeEach } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import { MemoryRouter, useLocation } from "react-router-dom";
import { filterApps, emptyQueryApps, EMPTY_QUERY_CAP } from "../filterApps";
import AppsDrawer from "../AppsDrawer";
import type { AppEntry } from "../../api";

function app(id: string, name: string, health: AppEntry["health"]): AppEntry {
  return { id, name, description: "", publicUrl: `https://${id}.tnhc.dev`, path: `/${id}`, delivery: "framed", health };
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
    expect(filterApps(APPS, "nexus-vault").map((a) => a.id)).toEqual(["nexus-vault"]);
  });

  it("is case-insensitive", () => {
    expect(filterApps(APPS, "CHAT").map((a) => a.id)).toEqual(["nexus-chat"]);
  });

  it("sorts healthy before offline", () => {
    expect(filterApps(APPS, "").map((a) => a.health)).toEqual(["healthy", "healthy", "offline", "offline"]);
  });

  it("returns everything for an empty query", () => {
    // This is filterApps' own contract as a pure search function — "no filter
    // matches everything" — not what the drawer shows by default. The
    // drawer's empty-query policy is emptyQueryApps, tested below.
    expect(filterApps(APPS, "  ")).toHaveLength(4);
  });

  it("returns nothing rather than everything for no match", () => {
    expect(filterApps(APPS, "zzzz")).toEqual([]);
  });

  it("still reaches offline entries once you type", () => {
    // The spec is explicit: typing searches all apps, including offline ones
    // — hiding them would silently deny that half the ecosystem exists.
    expect(filterApps(APPS, "forge").map((a) => a.id)).toEqual(["nexus-forge"]);
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

  it("lists offline apps once you search, so half the ecosystem is not silently denied", () => {
    open();
    fireEvent.change(screen.getByRole("searchbox"), { target: { value: "forge" } });
    expect(screen.getByText("Forge")).toBeTruthy();
  });

  it("does not make an offline app activatable", () => {
    open();
    fireEvent.change(screen.getByRole("searchbox"), { target: { value: "forge" } });
    const offline = screen.getByText("Forge").closest("[data-app-entry]")!;
    expect(offline.getAttribute("aria-disabled")).toBe("true");
    expect(offline.tagName).not.toBe("A");
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
    const options = screen.getAllByRole("option");
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
    expect(screen.getByText("Forge")).toBeTruthy();
  });
});
