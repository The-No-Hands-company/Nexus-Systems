import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import Launcher from "./Launcher";
import { appById } from "./apps";
import type { AppEntry } from "../api";

const app = (id: string, name: string, overrides: Partial<AppEntry> = {}): AppEntry => ({
  id, name, description: "", publicUrl: `https://${id}.tnhc.dev`, path: `/${id.replace(/^nexus-/, "")}`,
  delivery: "framed", health: "healthy", surface: "user-app", releaseState: "released",
  maturity: "stable", visibility: "default", reviewStatus: "approved", ...overrides,
});
const apps = [app("nexus-draw", "Draw"), app("nexus-chat", "Chat", { health: "offline" })];

describe("Launcher", () => {
  it("lists only released user apps", () => {
    render(<MemoryRouter><Launcher apps={apps} /></MemoryRouter>);
    expect(screen.getByText("Draw")).toBeTruthy();
    expect(screen.queryByText("Chat")).toBeNull();
  });

  it("links a healthy app into the shell, not out to its own host", () => {
    // The whole point of the shell: clicking an app must not navigate away.
    render(<MemoryRouter><Launcher apps={apps} /></MemoryRouter>);
    // Flat path: the URL names the app, not how the shell delivers it.
    expect(screen.getByRole("link", { name: /Draw/ }).getAttribute("href")).toBe("/draw");
  });

  it("does not link an offline app anywhere", () => {
    render(<MemoryRouter><Launcher apps={apps} /></MemoryRouter>);
    expect(screen.queryByRole("link", { name: /Chat/ })).toBeNull();
  });

  it("does not show an admin console as a launcher app", () => {
    const withCloud = [...apps, app("nexus-cloud", "Cloud", { path: "/cloud", publicUrl: undefined, delivery: "shell-native", surface: "admin-console" })];
    render(<MemoryRouter><Launcher apps={withCloud} /></MemoryRouter>);
    expect(screen.queryByText("Cloud")).toBeNull();
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
