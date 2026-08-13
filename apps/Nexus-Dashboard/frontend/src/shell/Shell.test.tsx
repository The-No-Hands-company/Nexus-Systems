import { describe, it, expect } from "vitest";
import { render, screen } from "@testing-library/react";
import Shell from "./Shell";

describe("Shell", () => {
  it("renders the three regions as landmarks", () => {
    // Landmarks rather than test ids: they are what a screen reader uses, so
    // asserting on them tests the accessibility the doctrine requires.
    render(<Shell sidebar={<nav />}>content</Shell>);
    expect(screen.getByRole("banner")).toBeTruthy();
    expect(screen.getByRole("main")).toBeTruthy();
  });

  it("puts children in the content region, not the header", () => {
    render(<Shell sidebar={<div />}>the app</Shell>);
    expect(screen.getByRole("main").textContent).toContain("the app");
    expect(screen.getByRole("banner").textContent).not.toContain("the app");
  });

  it("renders whatever sidebar it is given, scoped to the sidebar region", () => {
    render(<Shell sidebar={<div>launcher here</div>}>x</Shell>);
    const sidebar = screen.getByRole("complementary", { name: "Applications" });
    expect(sidebar.textContent).toContain("launcher here");
    expect(screen.getByRole("main").textContent).not.toContain("launcher here");
  });

  it("shows the wordmark in the header", () => {
    render(<Shell sidebar={<div />}>x</Shell>);
    expect(screen.getByRole("banner").textContent).toContain("Nexus");
  });
});
