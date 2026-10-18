import { describe, expect, it } from "vitest";
import { render, screen } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import AppShelf from "./AppShelf";
import type { AppEntry } from "../api";

const app = (overrides: Partial<AppEntry> = {}): AppEntry => ({
  id: "nexus-chat", name: "Nexus Chat", description: "Messaging", path: "/chat",
  publicUrl: "https://chat.tnhc.dev", delivery: "framed", health: "healthy",
  primaryShelf: "everyday", secondaryShelves: [], audiences: ["general"], capabilities: ["chat"],
  maturity: "stable", visibility: "default", reviewStatus: "approved", ...overrides,
});

describe("AppShelf", () => {
  it("renders a labelled shelf and links healthy apps", () => {
    render(<MemoryRouter><AppShelf title="Everyday" shelf="everyday" apps={[app()]} /></MemoryRouter>);
    expect(screen.getByRole("heading", { name: "Everyday" })).toBeTruthy();
    expect(screen.getByRole("link", { name: /nexus chat/i }).getAttribute("href")).toBe("/chat");
  });

  it("explains unavailable apps without rendering dead links", () => {
    render(<MemoryRouter><AppShelf title="Create" shelf="create" apps={[app({ id: "nexus-draw", name: "Nexus Draw", health: "offline", primaryShelf: "create" })]} /></MemoryRouter>);
    expect(screen.getByText(/offline/i)).toBeTruthy();
    expect(screen.queryByRole("link", { name: /nexus draw/i })).toBeNull();
  });
});
