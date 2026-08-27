import { describe, it, expect, vi } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import { filterApps } from "../filterApps";
import AppsDrawer from "../AppsDrawer";
import type { AppEntry } from "../../api";

function app(id: string, name: string, health: AppEntry["health"]): AppEntry {
  return { id, name, description: "", url: `https://${id}.tnhc.dev`, path: `/${id}`, health };
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
    expect(filterApps(APPS, "  ")).toHaveLength(4);
  });

  it("returns nothing rather than everything for no match", () => {
    expect(filterApps(APPS, "zzzz")).toEqual([]);
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

  it("lists offline apps, so half the ecosystem is not silently denied", () => {
    open();
    expect(screen.getByText("Forge")).toBeTruthy();
  });

  it("does not make an offline app activatable", () => {
    open();
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
});
