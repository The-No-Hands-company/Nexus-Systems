import { describe, it, expect } from "vitest";
import { render, screen, fireEvent } from "@testing-library/react";
import { MemoryRouter } from "react-router-dom";
import Shell from "../Shell";

const user = { id: "usr-1", username: "eric", email: "e@tnhc.dev", role: "founder" };

describe("Shell", () => {
  function mount() {
    return render(
      <MemoryRouter>
        <Shell user={user} apps={[]}><p>content</p></Shell>
      </MemoryRouter>,
    );
  }

  it("stamps the density attribute so the density tokens apply", () => {
    mount();
    expect(document.documentElement.getAttribute("data-nexus-density")).toBe("balanced");
  });

  it("opens the apps drawer from the Apps button", () => {
    mount();
    expect(screen.queryByRole("dialog")).toBeNull();
    fireEvent.click(screen.getByRole("button", { name: /apps/i }));
    expect(screen.getByRole("dialog")).toBeTruthy();
  });

  it("opens the apps drawer with the keyboard shortcut", () => {
    mount();
    fireEvent.keyDown(document, { key: "k", metaKey: true });
    expect(screen.getByRole("dialog")).toBeTruthy();
  });
});
