import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import Launcher from "./Launcher";
import { appById } from "./apps";

const apps = [
  { id: "nexus-draw", name: "Draw", description: "", url: "https://draw.tnhc.dev", health: "healthy" as const },
  { id: "nexus-chat", name: "Chat", description: "", url: "https://chat.tnhc.dev", health: "offline" as const },
];

describe("Launcher", () => {
  it("lists every app", () => {
    render(<MemoryRouter><Launcher apps={apps} /></MemoryRouter>);
    expect(screen.getByText("Draw")).toBeTruthy();
    expect(screen.getByText("Chat")).toBeTruthy();
  });

  it("links a healthy app into the shell, not out to its own host", () => {
    // The whole point of the shell: clicking an app must not navigate away.
    render(<MemoryRouter><Launcher apps={apps} /></MemoryRouter>);
    expect(screen.getByRole("link", { name: /Draw/ }).getAttribute("href")).toBe("/a/nexus-draw");
  });

  it("does not link an offline app anywhere", () => {
    render(<MemoryRouter><Launcher apps={apps} /></MemoryRouter>);
    expect(screen.queryByRole("link", { name: /Chat/ })).toBeNull();
  });

  it("links an internal app (relative url) straight to its shell route, not through /a/", () => {
    // Cloud's tile arrives with url "/cloud" (see apps.ts toAppEntries): a
    // shell-native view, not a site to frame.
    const withCloud = [
      ...apps,
      { id: "nexus-cloud", name: "Cloud", description: "", url: "/cloud", health: "healthy" as const },
    ];
    render(<MemoryRouter><Launcher apps={withCloud} /></MemoryRouter>);
    expect(screen.getByRole("link", { name: /Cloud/ }).getAttribute("href")).toBe("/cloud");
  });

  it("marks the active app for assistive tech, not just visually", () => {
    render(<MemoryRouter><Launcher apps={apps} activeId="nexus-draw" /></MemoryRouter>);
    expect(screen.getByRole("link", { name: /Draw/ }).getAttribute("aria-current")).toBe("page");
  });
});

describe("appById", () => {
  it("finds an app", () => {
    expect(appById(apps, "nexus-chat")?.name).toBe("Chat");
  });

  it("returns undefined for an unknown id rather than throwing", () => {
    expect(appById(apps, "nope")).toBeUndefined();
  });
});
